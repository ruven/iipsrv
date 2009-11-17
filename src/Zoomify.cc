/*
    IIP Zoomify Request Command Handler Class Member Function

    * Development carried out thanks to R&D grant DC08P02OUK006 - Old Maps Online *
    * (www.oldmapsonline.org) from Ministry of Culture of the Czech Republic      *


    Copyright (C) 2008-2009 Ruven Pillay.

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

#include <cmath>

#include "Task.h"
#include "ColourTransforms.h"
#include "Tokenizer.h"



using namespace std;



void Zoomify::run( Session* session, const std::string& argument ){

  if( session->loglevel >= 3 ) (*session->logfile) << "Zoomify handler reached" << endl;
  session = session;

  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // The argument is in the form Zoomify=TileGroup0/r-x-y.jpg where r is the resolution
  // number and x and y are the tile coordinates starting from the bottom left.
  string prefix, suffix;
  suffix = argument.substr( argument.find_last_of( "/" )+1, argument.length() );

  // We need to extract the image path, which is not always the same
  if( suffix == "ImageProperties.xml" )
    prefix = argument.substr( 0, argument.find_last_of( "/" ) );
  else
    prefix = argument.substr( 0, argument.find( "TileGroup" )-1 );


  // As we don't have an independent FIF request, we need to run it now
  FIF fif;
  fif.run( session, prefix );

  // Load image info
  (*session->image)->loadImageInfo( session->view->xangle, session->view->yangle );


  // Get the full image size and the total number of resolutions available
  unsigned int width = (*session->image)->getImageWidth();
  unsigned int height = (*session->image)->getImageHeight();


  unsigned int tw = (*session->image)->getTileWidth();
  unsigned int numResolutions = (*session->image)->getNumResolutions();


  // Zoomify does not accept arbitrary numbers of resolutions. The lowest
  // level must be the largest size that can fit within a single tile, so
  // we must discard any smaller than this
  unsigned int n;

  unsigned int discard = 0;

  for( n=0; n<numResolutions; n++ ){
    if( (*session->image)->image_widths[n] < tw && (*session->image)->image_heights[n] < tw ){
      discard++;
    }
  }


  if( discard > 0 ) discard -= 1;

  if( session->loglevel >= 2 ){
    if( discard > 0 ){
      *(session->logfile) << "Zoomify :: Discarding " << discard << " resolutions that are too small for Zoomify" << endl;
    }
  }

  // Zoomify clients have 2 phases, the initialization phase where they request
  // an XML file containing image data and the tile requests themselves.
  // These 2 phases are handled separately
  if( suffix == "ImageProperties.xml" ){

    if( session->loglevel >= 2 )
      *(session->logfile) << "Zoomify :: ImageProperties.xml request" << endl;


    *(session->logfile) << "Zoomify :: Total resolutions: " << numResolutions << ", image width: " << width
			<< ", image height: " << height << endl;

    int ntiles = (int) ceil( (double)width/tw ) * (int) ceil( (double)height/tw );

    char str[1024];
    snprintf( str, 1024, "Content-Type: application/xml\r\n"
	      "Cache-Control: max-age=604800\r\n"
	      "Last-Modified: Sat, 01 Jan 2000 00:00:00 GMT\r\n"
	      "Etag: ImageProperties.xml\r\n"
	      "\r\n"
	      "<IMAGE_PROPERTIES WIDTH=\"%d\" HEIGHT=\"%d\" NUMTILES=\"%d\" NUMIMAGES=\"1\" VERSION=\"1.8\" TILESIZE=\"%d\" />",
	      width, height, ntiles, tw );

    session->out->printf( (const char*) str );
    session->out->printf( "\r\n" );
    session->response->setImageSent();

    return;
  }


  // Get the tile coordinates. Zoomify requests are of the form r-x-y.jpg
  // where r is the resolution number and x and y are the tile coordinates
  Tokenizer izer( suffix, "-" );
  int resolution, x, y;
  if( izer.hasMoreTokens() ) resolution = atoi( izer.nextToken().c_str() );
  if( izer.hasMoreTokens() ) x = atoi( izer.nextToken().c_str() );
  if( izer.hasMoreTokens() ) y = atoi( izer.nextToken().c_str() );

  // Bump up to take account of any levels too small for Zoomify
  resolution += discard;

  if( session->loglevel >= 2 ){
    *(session->logfile) << "Zoomify :: Tile request for resolution:"
			<< resolution << " at x:" << x << ", y:" << y << endl;
  }


  // Get the width and height for the requested resolution
  width = (*session->image)->getImageWidth(numResolutions-resolution-1);
  height = (*session->image)->getImageHeight(numResolutions-resolution-1);


  // Get the width of the tiles and calculate the number
  // of tiles in each direction
  unsigned int rem_x = width % tw;
  unsigned int ntlx = (width / tw) + (rem_x == 0 ? 0 : 1);


  // Calculate the tile index for this resolution from our x, y
  unsigned int tile = y*ntlx + x;


  // Get our tile
  TileManager tilemanager( session->tileCache, *session->image, session->jpeg, session->logfile, session->loglevel );

  CompressionType ct;
  if( (*session->image)->getColourSpace() == CIELAB ) ct = UNCOMPRESSED;
  else if( (*session->image)->getNumBitsPerPixel() == 16 ) ct = UNCOMPRESSED;
  else if( session->view->getContrast() != 1.0 ) ct = UNCOMPRESSED;
  else ct = JPEG;


  RawTile rawtile = tilemanager.getTile( resolution, tile, session->view->xangle,
					 session->view->yangle, session->view->layers, ct );

  int len = rawtile.dataLength;

  if( session->loglevel >= 3 ){
    *(session->logfile) << "Zoomify :: Tile size: " << rawtile.width << " x " << rawtile.height << endl
			<< "Zoomify :: Channels per sample: " << rawtile.channels << endl
			<< "Zoomify :: Bits per channel: " << rawtile.bpc << endl
			<< "Zoomify :: Compressed tile size is " << len << endl;
  }



  float contrast = session->view->getContrast();

  unsigned char* buf = new unsigned char[ rawtile.width*rawtile.height*rawtile.channels ];
  unsigned char* ptr = (unsigned char*) rawtile.data;


  // Convert CIELAB to sRGB, performing tile cropping if necessary
  if( (*session->image)->getColourSpace() == CIELAB ){
    if( session->loglevel >= 4 ) *(session->logfile) << "Zoomify :: Converting from CIELAB->sRGB" << endl;
    for( unsigned int j=0; j<rawtile.height; j++ ){
      for( unsigned int i=0; i<rawtile.width*rawtile.channels; i+=rawtile.channels ){
	iip_LAB2sRGB( &ptr[j*tw*rawtile.channels + i], &buf[j*rawtile.width*rawtile.channels + i] );
      }
    }
    delete[] ptr;
    rawtile.data = buf;
  }

  // Handle 16bit images or contrast adjustments
  else if( (rawtile.bpc==16) || (contrast !=1.0) ){

    // Normalise 16bit images to 8bit for JPEG
    if( rawtile.bpc == 16 ) contrast = contrast / 256.0;

    float v;
    if( session->loglevel >= 4 ) *(session->logfile) << "Zoomify :: Applying contrast scaling of " << contrast << endl;
    for( unsigned int j=0; j<rawtile.height; j++ ){
      for( unsigned int i=0; i<rawtile.width*rawtile.channels; i++ ){

	if( rawtile.bpc == 16 ){
	  unsigned short* sptr = (unsigned short*) rawtile.data;
	  v = sptr[j*tw*rawtile.channels + i];
	}
	else{
	  unsigned char* sptr = (unsigned char*) rawtile.data;
	  v = sptr[j*tw*rawtile.channels + i];
	}

	v = v * contrast;
	if( v > 255.0 ) v = 255.0;
	if( v < 0.0 ) v = 0.0;
	buf[j*rawtile.width*rawtile.channels + i] = (unsigned char) v;
      }
    }
    delete[] ptr;
    rawtile.data = buf;
  }


  // Compress to JPEG
  if( ct == UNCOMPRESSED ){
    if( session->loglevel >= 4 ) *(session->logfile) << "Zoomify :: Compressing UNCOMPRESSED to JPEG" << endl;
    len = session->jpeg->Compress( rawtile );
  }


#ifndef DEBUG
  char str[1024];
  snprintf( str, 1024, "Content-Type: image/jpeg\r\n"
            "Content-Length: %d\r\n"
	    "Cache-Control: max-age=604800\r\n"
	    "Last-Modified: Sat, 01 Jan 2000 00:00:00 GMT\r\n"
	    "Etag: zoomify.jpg\r\n"
	    "\r\n", len );

  session->out->printf( (const char*) str );
#endif


  if( session->out->putStr( (const char*) rawtile.data, len ) != len ){
    if( session->loglevel >= 1 ){
      *(session->logfile) << "Zoomify :: Error writing jpeg tile" << endl;
    }
  }


  session->out->printf( "\r\n" );

  if( session->out->flush() == -1 ) {
    if( session->loglevel >= 1 ){
      *(session->logfile) << "Zoomify :: Error flushing jpeg tile" << endl;
    }
  }


  // Inform our response object that we have sent something to the client
  session->response->setImageSent();

  // Total Zoomify response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "Zoomify :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }


}
