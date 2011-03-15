/*
    IIP JTLS Command Handler Class Member Function

    Copyright (C) 2006-2009 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
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
#include "ColourTransforms.h"

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
  else if( (*session->image)->getNumBitsPerPixel() == 16 ) ct = UNCOMPRESSED;
  else if( session->view->getContrast() != 1.0 ) ct = UNCOMPRESSED;
  else ct = JPEG;

  RawTile rawtile = tilemanager.getTile( resolution, tile, session->view->xangle,
					 session->view->yangle, session->view->layers, ct );

  int len = rawtile.dataLength;

  if( session->loglevel >= 2 ){
    *(session->logfile) << "JTL :: Tile size: " << rawtile.width << " x " << rawtile.height << endl
			<< "JTL :: Channels per sample: " << rawtile.channels << endl
			<< "JTL :: Bits per channel: " << rawtile.bpc << endl
			<< "JTL :: Compressed tile size is " << len << endl;
  }


  float contrast = session->view->getContrast();
  unsigned int w = (*session->image)->getTileWidth();
  //  unsigned int h = (*session->image)->getTileHeight();



  // Convert CIELAB to sRGB, performing tile cropping if necessary
  if( (*session->image)->getColourSpace() == CIELAB ){
    if( session->loglevel >= 4 ) *(session->logfile) << "JTL :: Converting from CIELAB->sRGB" << endl;

    unsigned char* buf = new unsigned char[ rawtile.width*rawtile.height*rawtile.channels ];
    unsigned char* ptr = (unsigned char*) rawtile.data;
    for( unsigned int j=0; j<rawtile.height; j++ ){
      for( unsigned int i=0; i<rawtile.width*rawtile.channels; i+=rawtile.channels ){
	iip_LAB2sRGB( &ptr[j*w*rawtile.channels + i], &buf[j*rawtile.width*rawtile.channels + i] );
      }
    }

    // Delete our old tile data and set it to our new buffer
    delete[] ptr;
    rawtile.data = buf;
  }

  // Handle 16bit images or contrast adjustments, performing tile cropping if necessary
  else if( (rawtile.bpc==16) || (contrast !=1.0) ){

    // Normalise 16bit images to 8bit for JPEG
    if( rawtile.bpc == 16 ) contrast = contrast / 256.0;

    float v;
    if( session->loglevel >= 4 ) *(session->logfile) << "JTL :: Applying contrast scaling of " << contrast << endl;

    unsigned int dataLength = rawtile.width*rawtile.height*rawtile.channels;
    unsigned char* buf = new unsigned char[ dataLength ];

    for( unsigned int j=0; j<rawtile.height; j++ ){
      for( unsigned int i=0; i<rawtile.width*rawtile.channels; i++ ){

	if( rawtile.bpc == 16 ){
	  unsigned short* sptr = (unsigned short*) rawtile.data;
	  v = (float) sptr[j*w*rawtile.channels + i];
	}
	else{
	  unsigned char* sptr = (unsigned char*) rawtile.data;
	  v = (float) sptr[j*w*rawtile.channels + i];
	}

	v = v * contrast;
	if( v > 255.0 ) v = 255.0;
	if( v < 0.0 ) v = 0.0;
	buf[j*rawtile.width*rawtile.channels + i] = (unsigned char) v;
      }
    }

    // Copy this new buffer back
    memcpy(rawtile.data, buf, dataLength);
    rawtile.dataLength = dataLength;

    // And delete our buffer
    delete[] buf;
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
