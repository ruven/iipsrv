/*
    IIP JTLS Command Handler Class Member Function

    Copyright (C) 2006-2013 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include "Task.h"
#include "Transforms.h"

#include <cmath>
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
  Timer function_timer;


  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // Parse the argument list
  int delimitter = argument.find( "," );
  resolution = atoi( argument.substr( 0, delimitter ).c_str() );

  delimitter = argument.find( "," );
  tile = atoi( argument.substr( delimitter + 1, argument.length() ).c_str() );


  // If we have requested a rotation, remap the tile index to rotated coordinates
  if( (int)((session->view)->getRotation()) % 360 == 90 ){

  }
  else if( (int)((session->view)->getRotation()) % 360 == 270 ){

  }
  else if( (int)((session->view)->getRotation()) % 360 == 180 ){
    int num_res = (*session->image)->getNumResolutions();
    unsigned int im_width = (*session->image)->image_widths[num_res-resolution-1];
    unsigned int im_height = (*session->image)->image_heights[num_res-resolution-1];
    unsigned int tw = (*session->image)->getTileWidth();
    //    unsigned int th = (*session->image)->getTileHeight();
    int ntiles = (int) ceil( (double)im_width/tw ) * (int) ceil( (double)im_height/tw );
    tile = ntiles - tile - 1;
  }


  // Sanity check
  if( (resolution<0) || (tile<0) ){
    ostringstream error;
    error << "JTL :: Invalid resolution/tile number: " << resolution << "," << tile; 
    throw error.str();
  }

  TileManager tilemanager( session->tileCache, *session->image, session->watermark, session->jpeg, session->logfile, session->loglevel );

  CompressionType ct;
  if( (*session->image)->getNumBitsPerPixel() > 8 ) ct = UNCOMPRESSED;
  else if( (*session->image)->getColourSpace() == CIELAB ) ct = UNCOMPRESSED;
  else if( session->view->getContrast() != 1.0 ) ct = UNCOMPRESSED;
  else if( session->view->getGamma() != 1.0 ) ct = UNCOMPRESSED;
  else if( session->view->getRotation() != 0.0 ) ct = UNCOMPRESSED;
  else if( session->view->shaded ) ct = UNCOMPRESSED;
  else if( session->view->cmapped ) ct = UNCOMPRESSED;
  else if( session->view->inverted ) ct = UNCOMPRESSED;
  else ct = JPEG;

  RawTile rawtile = tilemanager.getTile( resolution, tile, session->view->xangle,
					 session->view->yangle, session->view->getLayers(), ct );

  // For image sequences where images are not all the same bitdepth, the TileManager will return an uncompressed tile
  if( rawtile.bpc > 8 ) ct = UNCOMPRESSED;


  int len = rawtile.dataLength;

  if( session->loglevel >= 2 ){
    *(session->logfile) << "JTL :: Tile size: " << rawtile.width << " x " << rawtile.height << endl
			<< "JTL :: Channels per sample: " << rawtile.channels << endl
			<< "JTL :: Bits per channel: " << rawtile.bpc << endl
			<< "JTL :: Data size is " << len << " bytes" << endl;
  }


  // Convert CIELAB to sRGB
  if( (*session->image)->getColourSpace() == CIELAB ){

    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Converting from CIELAB->sRGB" << endl;
      function_timer.start();
    }

    filter_LAB2sRGB( rawtile );

    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: CIELAB->sRGB conversion in " << function_timer.getTime() << " microseconds" << endl;
    }
  }

  // Apply normalization and float conversion
  if( session->loglevel >= 3 ){
    *(session->logfile) << "JTL :: Normalizing and converting to float" << endl;
  }

  filter_normalize( rawtile, (*session->image)->max, (*session->image)->min );

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
      function_timer.start();
    }
    filter_gamma( rawtile, gamma);
    if( session->loglevel >= 3 ){
      *(session->logfile) << "JTL :: Gamma applied in " << function_timer.getTime() << " microseconds" << endl;
    }
  }

  // Apply inversion if requested
  if( session->view->inverted ){
    if( session->loglevel >= 3 ){
      *(session->logfile) << "JTL :: Applying inversion" << endl;
    }
    filter_inv( rawtile );
  }

  // Apply color mapping if requested
  if( session->view->cmapped ){
    if( session->loglevel >= 3 ){
      *(session->logfile) << "JTL :: Applying color map" << endl;
    }
    filter_cmap( rawtile, session->view->cmap );
  }

  // Apply any contrast adjustments and/or clipping to 8bit from 16 or 32 bit
  float contrast = session->view->getContrast();
  if( session->loglevel >= 3 ){
    *(session->logfile) << "JTL :: Applying contrast of " << contrast << endl;
    function_timer.start();
  }
  filter_contrast( rawtile, contrast );
  if( session->loglevel >= 3 ){
    *(session->logfile) << "JTL :: Conversion to 8 bit applied in " << function_timer.getTime() << " microseconds" << endl;
  }


  // Convert to greyscale if requested
  if( (*session->image)->getColourSpace() == sRGB && session->view->colourspace == GREYSCALE ){
    if( session->loglevel >= 3 ){
      *(session->logfile) << "JTL :: Converting to greyscale" << endl;
    }
    filter_greyscale( rawtile );
  }


  // Apply rotation - can apply this safely after gamma and contrast adjustment
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
