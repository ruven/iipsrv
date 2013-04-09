/*
    IIP JTLS Command Handler Class Member Function

    Copyright (C) 2006-2012 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "Task.h"
#include "Transforms.h"

#include <sstream>

using namespace std;



void JTL::run( Session* session, const std::string& argument ){

  /* The argument should consist of 2 comma separated values:
     1) resolution
     2) tile number
  */

  if( session->loglevel >= 3 ) (*session->logfile) << "JTL handler reached" << endl;
  session = session;

  int resolution, tile;


  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // Parse the argument list
  int delimitter = argument.find( "," );
  resolution = atoi( argument.substr( 0, delimitter ).c_str() );

  delimitter = argument.find( "," );
  tile = atoi( argument.substr( delimitter + 1, argument.length() ).c_str() );

  //Sanity check
  if( (resolution<0) || (tile<0) ){
    ostringstream error;
    error << "JTL :: Invalid resolution/tile number: " << resolution << "," << tile; 
    throw error.str();
  }


  TileManager tilemanager( session->tileCache, *session->image, session->watermark, session->jpeg, session->logfile, session->loglevel );

  CompressionType ct;
  if( (*session->image)->getColourSpace() == CIELAB ) ct = UNCOMPRESSED;
  else if( (*session->image)->getNumBitsPerPixel() > 8 ) ct = UNCOMPRESSED;
  else if( session->view->getContrast() != 1.0 ) ct = UNCOMPRESSED;
  else if( session->view->getRotation() != 0.0 ) ct = UNCOMPRESSED;
  else if( session->view->shaded ) ct = UNCOMPRESSED;
  else ct = JPEG;

  //next block cares about jpeg 2000 difference of ceiling image dimensions (tiff truncates it)
  //there may exist small (1px) tile on the right edge, that viewer doesn't know about
  //so we must adjust requested tile numbers in that case
  unsigned int tw = (*session->image)->getTileWidth();
  unsigned int width = (*session->image)->getImageWidth();
  int differenceOfResolutions = (*session->image)->getNumResolutions() - 1 - resolution;
  int divideFactor = std::pow(2.0, differenceOfResolutions);
  int rowSize =  std::ceil((width/divideFactor) / (double)tw);
  int resolutionWidth = (*session->image)->image_widths[differenceOfResolutions-1];
  //adjusting is needed only if size of tier computed by viewer (truncated) is dividable by tile width
  // and real width of this resolution is higher than computed, also don't adjust anything for full size
  if(width/divideFactor % tw == 0 && resolutionWidth > width/divideFactor && divideFactor > 1){
    if (tile >= rowSize){
      tile += tile / rowSize; //move tile by 1 for every row
    }
  }

  RawTile rawtile = tilemanager.getTile( resolution, tile, session->view->xangle,
					 session->view->yangle, session->view->getLayers(), ct );

  int len = rawtile.dataLength;

  if( session->loglevel >= 2 ){
    *(session->logfile) << "JTL :: Tile size: " << rawtile.width << " x " << rawtile.height << endl
			<< "JTL :: Channels per sample: " << rawtile.channels << endl
			<< "JTL :: Bits per channel: " << rawtile.bpc << endl
			<< "JTL :: Data size is " << len << " bytes" << endl;
  }


  // Convert CIELAB to sRGB
  if( (*session->image)->getColourSpace() == CIELAB ){

    Timer cielab_timer;
    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Converting from CIELAB->sRGB" << endl;
      cielab_timer.start();
    }

    filter_LAB2sRGB( rawtile );

    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: CIELAB->sRGB conversion in " << cielab_timer.getTime() << " microseconds" << endl;
    }
  }


  // Apply hill shading if requested
  if( session->view->shaded ){
    if( session->loglevel >= 3 ){
      *(session->logfile) << "JTL :: Applying hill-shading" << endl;
    }
    filter_shade( rawtile, session->view->shade[0], session->view->shade[1] );
  }


  // Apply any gamma correction
  if( session->view->getGamma() != 1.0 ){
    float gamma = session->view->getGamma();
    if( session->loglevel >= 3 ){
      *(session->logfile) << "JTL :: Applying gamma of " << gamma << endl;
    }
    filter_gamma( rawtile, gamma, (*session->image)->max, (*session->image)->min );
  }


  // Apply any contrast adjustments and/or clipping to 8bit from 16bit
  filter_contrast( rawtile, session->view->getContrast(), (*session->image)->max, (*session->image)->min );


  // Apply rotation
  if( session->view->getRotation() != 0.0 ){
    float rotation = session->view->getRotation();
    if( session->loglevel >= 3 ){
      *(session->logfile) << "JTL :: Rotating image by " << rotation << " degrees" << endl; 
    }
    filter_rotate( rawtile, rotation );
  }


  // Compress to JPEG
  if( ct == UNCOMPRESSED ){
    if( session->loglevel >= 4 ) *(session->logfile) << "JTL :: Compressing UNCOMPRESSED to JPEG" << endl;
    len = session->jpeg->Compress( rawtile );
  }


#ifndef DEBUG
  char str[1024];

  snprintf( str, 1024,
	    "Server: iipsrv/%s\r\n"
	    "Content-Type: image/jpeg\r\n"
            "Content-Length: %d\r\n"
	    "Cache-Control: max-age=%d\r\n"
	    "Last-Modified: %s\r\n"
	    "\r\n",
	    VERSION, len, MAX_AGE, (*session->image)->getTimestamp().c_str() );

  session->out->printf( str );
#endif


  if( session->out->putStr( static_cast<const char*>(rawtile.data), len ) != len ){
    if( session->loglevel >= 1 ){
      *(session->logfile) << "JTL :: Error writing jpeg tile" << endl;
    }
  }


  if( session->out->flush() == -1 ) {
    if( session->loglevel >= 1 ){
      *(session->logfile) << "JTL :: Error flushing jpeg tile" << endl;
    }
  }


  // Inform our response object that we have sent something to the client
  session->response->setImageSent();

  // Total JTL response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "JTL :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
