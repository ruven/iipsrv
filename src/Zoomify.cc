/*
    IIP Zoomify Request Command Handler Class Member Function

    * Development carried out thanks to R&D grant DC08P02OUK006 - Old Maps Online *
    * (www.oldmapsonline.org) from Ministry of Culture of the Czech Republic      *


    Copyright (C) 2008-2013 Ruven Pillay.

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

#include <cmath>

#include "Task.h"
#include "Transforms.h"
#include "Tokenizer.h"



using namespace std;



void Zoomify::run( Session* session, const std::string& argument ){

  if( session->loglevel >= 3 ) (*session->logfile) << "Zoomify handler reached" << endl;

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
    if( floor(width / pow(2.0, (double) n)) <= tw  && floor(height / pow(2.0, (double) n)) <= tw ){
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

    if( session->loglevel >= 2 ){
      *(session->logfile) << "Zoomify :: ImageProperties.xml request" << endl;
      *(session->logfile) << "Zoomify :: Total resolutions: " << numResolutions << ", image width: " << width
			  << ", image height: " << height << endl;
    }

    int ntiles = (int) ceil( (double)width/tw ) * (int) ceil( (double)height/tw );

    char str[1024];
    snprintf( str, 1024,
	      "Server: iipsrv/%s\r\n"
	      "Content-Type: application/xml\r\n"
	      "Cache-Control: max-age=%d\r\n"
	      "Last-Modified: %s\r\n"
	      "\r\n"
	      "<IMAGE_PROPERTIES WIDTH=\"%d\" HEIGHT=\"%d\" NUMTILES=\"%d\" NUMIMAGES=\"1\" VERSION=\"1.8\" TILESIZE=\"%d\" />",
	      VERSION, MAX_AGE,(*session->image)->getTimestamp().c_str(), width, height, ntiles, tw );

    session->out->printf( (const char*) str );
    session->response->setImageSent();

    return;
  }


  // Get the tile coordinates. Zoomify requests are of the form r-x-y.jpg
  // where r is the resolution number and x and y are the tile coordinates
  Tokenizer izer( suffix, "-" );
  int resolution=0, x=0, y=0;
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
  TileManager tilemanager( session->tileCache, *session->image, session->watermark, session->jpeg, session->logfile, session->loglevel );

  CompressionType ct;
  if( (*session->image)->getColourSpace() == CIELAB ) ct = UNCOMPRESSED;
  else if( (*session->image)->getNumBitsPerPixel() == 16 ) ct = UNCOMPRESSED;
  else ct = JPEG;


  RawTile rawtile = tilemanager.getTile( resolution, tile, session->view->xangle,
					 session->view->yangle, session->view->getLayers(), ct );

  int len = rawtile.dataLength;

  if( session->loglevel >= 3 ){
    *(session->logfile) << "Zoomify :: Tile size: " << rawtile.width << " x " << rawtile.height << endl
			<< "Zoomify :: Channels per sample: " << rawtile.channels << endl
			<< "Zoomify :: Bits per channel: " << rawtile.bpc << endl
			<< "Zoomify :: Compressed tile size is " << len << endl;
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

  // Apply normalization and float conversion
  filter_normalize( rawtile, (*session->image)->max, (*session->image)->min );

  // Apply any contrast adjustments and/or clipping to 8bit
  filter_contrast( rawtile, session->view->getContrast() );


  // Compress to JPEG
  if( ct == UNCOMPRESSED ){
    if( session->loglevel >= 4 ) *(session->logfile) << "Zoomify :: Compressing UNCOMPRESSED to JPEG" << endl;
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

  session->out->printf( (const char*) str );
#endif


  if( session->out->putStr( (const char*) rawtile.data, len ) != len ){
    if( session->loglevel >= 1 ){
      *(session->logfile) << "Zoomify :: Error writing jpeg tile" << endl;
    }
  }


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
