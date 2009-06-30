/*
    IIP CVT Command Handler Class Member Function

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


using namespace std;



void CVT::run( Session* session, const std::string& a ){

  Timer tile_timer;
  this->session = session;

  if( session->loglevel >= 2 ) *(session->logfile) << "CVT handler reached" << endl;

  checkImage();


  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // Put the argument into lower case
  string argument = a;
  transform( argument.begin(), argument.end(), argument.begin(), ::tolower );


  // For the moment, only deal with JPEG. If we have specified something else, give a warning
  // and send JPEG anyway
  if( argument != "jpeg" ){
    if( session->loglevel >= 1 ) *(session->logfile) << "CVT :: Unsupported request: '" << argument << "'. Sending JPEG." << endl;
    argument = "jpeg";
  }



  if( argument == "jpeg" ){

    int cielab = 0;
    unsigned int n;

    if( session->loglevel >= 3 ) *(session->logfile) << "CVT :: JPEG output handler reached" << endl;


    // Get a fake tile in case we are dealing with a sequence
    (*session->image)->loadImageInfo( session->view->xangle, session->view->yangle );

    // Calculate the number of tiles at the requested resolution
    unsigned int im_width = (*session->image)->getImageWidth();
    unsigned int im_height = (*session->image)->getImageHeight();
    int num_res = (*session->image)->getNumResolutions();

    session->view->setImageSize( im_width, im_height );
    session->view->setMaxResolutions( num_res );


    int requested_res = session->view->getResolution();
    im_width = (*session->image)->image_widths[num_res-requested_res-1];
    im_height = (*session->image)->image_heights[num_res-requested_res-1];


    if( session->loglevel >= 3 ){
      *(session->logfile) << "CVT :: image set to " << im_width << " x " << im_height
			  << " using resolution " << requested_res << endl;
    }

    // The tile size of the source tile
    unsigned int src_tile_width = (*session->image)->getTileWidth();
    unsigned int src_tile_height = (*session->image)->getTileHeight();

    // The tile size of the destination tile
    unsigned int dst_tile_width = src_tile_width;
    unsigned int dst_tile_height = src_tile_height;

    // The basic tile size ie. not the current tile
    unsigned int basic_tile_width = src_tile_width;

    unsigned int rem_x = im_width % src_tile_width;
    unsigned int rem_y = im_height % src_tile_height;

    unsigned int channels = (*session->image)->getNumChannels();
    

    // The number of tiles in each direction
    unsigned int ntlx = (im_width / src_tile_width) + (rem_x == 0 ? 0 : 1);
    unsigned int ntly = (im_height / src_tile_height) + (rem_y == 0 ? 0 : 1);
    int len;


    // If we have a region defined, calculate our viewport
    unsigned int view_left, view_top, view_width, view_height;
    unsigned int startx, endx, starty, endy, xoffset, yoffset;

    if( session->view->viewPortSet() ){

      // Set the absolute viewport size and extract the co-ordinates
      view_left = session->view->getViewLeft();
      view_top = session->view->getViewTop();
      view_width = session->view->getViewWidth();
      view_height = session->view->getViewHeight();

      // Calculate the start tiles
      startx = (unsigned int) ( view_left / src_tile_width );
      starty = (unsigned int) ( view_top / src_tile_height );
      xoffset = view_left % src_tile_width;
      yoffset = view_top % src_tile_height;
      endx = (unsigned int) ( (view_width + view_left) / src_tile_width ) + 1;
      endy = (unsigned int) ( (view_height + view_top) / src_tile_height ) + 1;

      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: view port is set: image: " << im_width << "x" << im_height
			    << ". View Port: " << view_left << "," << view_top
			    << "," << view_width << "," << view_height << endl
			    << "CVT :: Pixel Offsets: " << startx << "," << starty << ","
			    << xoffset << "," << yoffset << endl
			    << "CVT :: End Tiles: " << endx << "," << endy << endl;
		
      }
    }
    else{
      if( session->loglevel >= 4 ) *(session->logfile) << "CVT :: No view port set" << endl;
      view_left = 0;
      view_top = 0;
      view_width = im_width;
      view_height = im_height;
      startx = starty = xoffset = yoffset = 0;
      endx = ntlx;
      endy = ntly;
    }


    // Allocate memory for a strip (tile height x image width)
    unsigned int o_channels = channels;
    if( session->view->shaded ) o_channels = 1;
    unsigned char* buf = new unsigned char[view_width * src_tile_height * o_channels];


    // Create a RawTile for the entire image
    RawTile complete_image( 0, 0, 0, 0, view_width, view_height, o_channels, 8 );
    complete_image.dataLength = view_width * view_height * o_channels;
    complete_image.data = buf;
    complete_image.memoryManaged = 0; // We will handle memory ourselves


    // Initialise our JPEG compression object
    session->jpeg->InitCompression( complete_image, src_tile_height );

#ifndef DEBUG
    session->out->printf("Cache-Control: max-age=86400\r\n"
			 "Last-Modified: Mon, 1 Jan 2000 00:00:00 GMT\r\n"
			 "ETag: \"CVT\"\r\n"
 			 "Content-Type: image/jpeg\r\n"
			 "Content-Disposition: inline;filename=\"cvt.jpg\"\r\n"
			 "\r\n" );
#endif

    // Send the JPEG header to the client
    len = session->jpeg->getHeaderSize();
    if( session->out->putStr( (const char*) session->jpeg->getHeader(), len ) != len ){
      if( session->loglevel >= 1 ){
	*(session->logfile) << "CVT :: Error writing jpeg header" << endl;
      }
    }


    // Decode the image strip by strip and dynamically compress with JPEG

    for( unsigned int i=starty; i<endy; i++ ){

      unsigned int buffer_index = 0;

      // Keep track of the current pixel boundary horizontally. ie. only up
      //  to the beginning of the current tile boundary.
      int current_width = 0;

      for( unsigned int j=startx; j<endx; j++ ){

	// Time the tile retrieval
	if( session->loglevel >= 2 ) tile_timer.start();

	// Get an uncompressed tile from our TileManager
	TileManager tilemanager( session->tileCache, *session->image, session->jpeg, session->logfile, session->loglevel );
	RawTile rawtile = tilemanager.getTile( requested_res, (i*ntlx) + j, session->view->xangle, session->view->yangle, UNCOMPRESSED );

	if( session->loglevel >= 2 ){
	  *(session->logfile) << "CVT :: Tile access time " << tile_timer.getTime() << " microseconds for tile " << (i*ntlx) + j << " at resolution " << requested_res << endl;
	}


	// Check the colour space - CIELAB images will need to be converted
	if( (*session->image)->getColourSpace() == CIELAB ){
	  cielab = 1;
	  if( session->loglevel >= 3 ){
	    *(session->logfile) << "CVT :: Converting from CIELAB->sRGB" << endl;
	  }
	}

	// Only print this out once per image
	if( (session->loglevel >= 4) && (i==starty) && (j==starty) ){
	  *(session->logfile) << "CVT :: Tile data is " << rawtile.channels << " channels, "
			      << rawtile.bpc << " bits per channel" << endl;
	}

	// Set the tile width and height to be that of the source tile
	// - Use the rawtile data because if we take a tile from cache
	//   the image pointer will not necessarily be pointing to the
	//   the current tile
	//	src_tile_width = (*session->image)->getTileWidth();
	//	src_tile_height = (*session->image)->getTileHeight();
	src_tile_width = rawtile.width;
	src_tile_height = rawtile.height;
	dst_tile_width = src_tile_width;
	dst_tile_height = src_tile_height;

	// Variables for the pixel offset within the current tile
	unsigned int xf = 0;
	unsigned int yf = 0;

	// If our viewport has been set, we need to modify our start
	// and end points on the source image
	if( session->view->viewPortSet() ){

	  if( j == startx ){
	    // Calculate the width used in the current tile
	    // If there is only 1 tile, the width is just the view width
	    if( j < endx - 1 ) dst_tile_width = src_tile_width - xoffset;
	    else dst_tile_width = view_width;
	    xf = xoffset;
	  }
	  else if( j == endx-1 ){
	    dst_tile_width = (view_width+view_left) % basic_tile_width;
	  }

	  if( i == starty ){
	    // Calculate the height used in the current row of tiles
	    // If there is only 1 row the height is just the view height
	    if( i < endy - 1 ) dst_tile_height = src_tile_height - yoffset;
	    else dst_tile_height = view_height;
	    yf = yoffset;
	  }
	  else if( i == endy-1 ){
	    dst_tile_height = (view_height+view_top) % basic_tile_width;
	  }

	  if( session->loglevel >= 4 ){
	    *(session->logfile) << "CVT :: destination tile height: " << dst_tile_height
				<< ", tile width: " << dst_tile_width << endl;
	  }
	}


	// Copy our tile data into the appropriate part of the strip memory
	// one whole tile width at a time
	if( !rawtile.padded ){
	  if( session->loglevel >= 4 ) *(session->logfile) << "CVT :: unpadded tile" << endl;
	  basic_tile_width = rawtile.width;
	}
	for( unsigned int k=0; k<dst_tile_height; k++ ){

	  buffer_index = (current_width*channels) + (k*view_width*channels);
	  unsigned int inx = ((k+yf)*basic_tile_width*channels) + (xf*channels);
	  unsigned char* ptr = (unsigned char*) rawtile.data;

	  // If we have a CIELAB image, convert each pixel to sRGB first
	  // Otherwise just do a fast memcpy
	  if( cielab ){
	    for( n=0; n<dst_tile_width*channels; n+=channels ){
	      iip_LAB2sRGB( &ptr[inx + n], &buf[buffer_index + n] );
	    }
	  }
	  else if( session->view->shaded ){
	    int m;
	    for( n=0, m=0; n<dst_tile_width*channels; n+=channels, m++ ){
	      shade( &ptr[inx + n], &buf[current_width + (k*view_width) + m],
		     session->view->shade[0], session->view->shade[1],
		     session->view->getContrast() );
	    }
	  }
	  // If we have a 16 bit image, multiply by the contrast adjustment if it exists
	  // and scale to 8 bits.
	  else if( rawtile.bpc == 16 ){
	    unsigned short* sptr = (unsigned short*) rawtile.data;
	    for( n=0; n<dst_tile_width*channels; n++ ){
	      float v = (float)sptr[inx+n] * (session->view->getContrast() / 256.0);
	      if( v > 255.0 ) v = 255.0;
	      buf[buffer_index + n] = (unsigned char) v;
	    }
	  }
	  else if( (rawtile.bpc == 8) && (session->view->getContrast() != 1.0) ){
	    unsigned char* sptr = (unsigned char*) rawtile.data;
	    for( n=0; n<dst_tile_width*channels; n++ ){
	      float v = (float)sptr[inx+n] * session->view->getContrast();
	      if( v > 255.0 ) v = 255.0;
	      buf[buffer_index + n] = (unsigned char) v;
	    }
	  }
	  else{
	    memcpy( &buf[buffer_index], &ptr[inx], dst_tile_width*channels );
	  }
	}

	current_width += dst_tile_width;

      }


      // Compress the strip
      len = session->jpeg->CompressStrip( buf, dst_tile_height );


      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: Compressed data strip length is " << len << endl;
      }


      // Send this strip out to the client
      if( len != session->out->putStr( (const char*) complete_image.data, len ) ){
	if( session->loglevel >= 1 ){
	  *(session->logfile) << "CVT :: Error writing jpeg strip data: " << len << endl;
	}
      }

      if( session->out->flush() == -1 ) {
	if( session->loglevel >= 1 ){
	  *(session->logfile) << "CVT :: Error flushing jpeg tile" << endl;
	}
      }
    }

    // Finish off the image compression
    len = session->jpeg->Finish();
    if( session->out->putStr( (const char*) complete_image.data, len ) != len ){
      if( session->loglevel >= 1 ){
	*(session->logfile) << "CVT :: Error writing jpeg EOI markers" << endl;
      }
    }

    // Finish off the flush the buffer
    session->out->printf( "\r\n" );

    if( session->out->flush()  == -1 ) {
      if( session->loglevel >= 1 ){
	*(session->logfile) << "CVT :: Error flushing jpeg tile" << endl;
      }
    }

    // Inform our response object that we have sent something to the client
    session->response->setImageSent();

    // Don't forget to delete our strip of memory
    delete[] buf;

  } // End of if( argument == "jpeg" )


  // Total CVT response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "CVT :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }


}  
