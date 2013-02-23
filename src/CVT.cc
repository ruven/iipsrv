/*
    IIP CVT Command Handler Class Member Function

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
    along with this program; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include "Task.h"
#include "Transforms.h"
#include "Environment.h"
#include <cmath>
#include <algorithm>

//#define CHUNKED 1

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


    if( session->loglevel >= 3 ) *(session->logfile) << "CVT :: JPEG output handler reached" << endl;


    // Reload info in case we are dealing with a sequence
    (*session->image)->loadImageInfo( session->view->xangle, session->view->yangle );

    // Calculate the number of tiles at the requested resolution
    unsigned int im_width = (*session->image)->getImageWidth();
    unsigned int im_height = (*session->image)->getImageHeight();
    int num_res = (*session->image)->getNumResolutions();

    // Setup our view with some basic info
    session->view->setImageSize( im_width, im_height );
    session->view->setMaxResolutions( num_res );

    // Get the resolution, width and height for this view
    int requested_res = session->view->getResolution();
    im_width = (*session->image)->image_widths[num_res-requested_res-1];
    im_height = (*session->image)->image_heights[num_res-requested_res-1];


    if( session->loglevel >= 3 ){
      *(session->logfile) << "CVT :: image set to " << im_width << "x" << im_height
			  << " using resolution " << requested_res << endl;
    }


    // Number of channels
    unsigned int channels = (*session->image)->getNumChannels();

    // Data length
    int len;


    // Set up our final image sizes and if we have a region defined,
    // calculate our viewport
    unsigned int resampled_width, resampled_height;
    unsigned int view_left, view_top, view_width, view_height;

    if( session->view->viewPortSet() ){

      // Set the absolute viewport size and extract the co-ordinates
      view_left = session->view->getViewLeft();
      view_top = session->view->getViewTop();
      view_width = session->view->getViewWidth();
      view_height = session->view->getViewHeight();
      resampled_width = view_width;
      resampled_height = view_height;

      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: view port is set: image: " << im_width << "x" << im_height
			    << ". View Port: " << view_left << "," << view_top
			    << "," << view_width << "," << view_height << endl;
      }
    }
    else{
      if( session->loglevel >= 4 ) *(session->logfile) << "CVT :: No view port set" << endl;
      view_left = 0;
      view_top = 0;
      view_width = im_width;
      view_height = im_height;
      resampled_width = session->view->getRequestWidth();
      resampled_height = session->view->getRequestHeight();

      // Make sure our requested width and height don't modify the aspect ratio. Make sure the final image fits *within* the requested size
      if( ((double)resampled_height/(double)view_height) > ((double)resampled_width/(double)view_width) ){
	resampled_height = (unsigned int) (((double)resampled_width/(double)view_width) * view_height);
      }
      else if( ((double)resampled_width/(double)view_width) > ((double)resampled_height/(double)view_height) ){
	resampled_width = (unsigned int) (((double)resampled_height/(double)view_height) * view_width);
      }
    }


    if( session->loglevel >= 3 ){
      *(session->logfile) << "CVT :: Requested scaled region size is " << resampled_width << "x" << resampled_height
			  << ". Nearest pyramid region size is " << view_width << "x" << view_height << endl;
    }


#ifndef DEBUG

    // Define our separator depending on the OS
#ifdef WIN32
    const string separator = "\\";
#else
    const string separator = "/";
#endif


    // Get our image file name and strip of the directory path and any suffix
    string filename = (*session->image)->getImagePath();
    int pos = filename.rfind(separator)+1;
    string basename = filename.substr( pos, filename.rfind(".")-pos );

    char str[1024];
    snprintf( str, 1024, "Server: iipsrv/%s\r\n"
	                 "Cache-Control: max-age=%d\r\n"
			 "Last-Modified: %s\r\n"
 			 "Content-Type: image/jpeg\r\n"
			 "Content-Disposition: inline;filename=\"%s.jpg\"\r\n"
#ifdef CHUNKED
	                 "Transfer-Encoding: chunked\r\n"
#endif
	                 "\r\n",
	                 VERSION, MAX_AGE, (*session->image)->getTimestamp().c_str(), basename.c_str() );

    session->out->printf( (const char*) str );
#endif

    // Get our requested region from our TileManager
    TileManager tilemanager( session->tileCache, *session->image, session->watermark, session->jpeg, session->logfile, session->loglevel );
    RawTile complete_image = tilemanager.getRegion( requested_res,
						    session->view->xangle, session->view->yangle,
						    session->view->getLayers(),
						    view_left, view_top, view_width, view_height );

    if( session->loglevel >= 4 ){
      if( session->view->getContrast() != 1.0 ){
	*(session->logfile) << "CVT :: Applying contrast of: " << session->view->getContrast() << endl;
      }
      if( complete_image.bpc > 8 ) *(session->logfile) << "CVT :: Rescaling " <<  complete_image.bpc
						       << " bit data to 8" << endl;
    }


    // Convert CIELAB to sRGB
    if( (*session->image)->getColourSpace() == CIELAB ){
      Timer cielab_timer;
      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: Converting from CIELAB->sRGB" << endl;
	cielab_timer.start();
      }
      filter_LAB2sRGB( complete_image );
      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: CIELAB->sRGB conversion in " << cielab_timer.getTime()
			    << " microseconds" << endl;
      }
    }


    // Apply color mapping if requested
    if( session->view->cmapped ){
      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: Applying color map" << endl;
      }
      filter_cmap( complete_image, session->view->cmap, (*session->image)->min[0], (*session->image)->max[0]);
      // Don't forget to reset our channels variable as this is used later
      channels = 3;
    }

    // Apply hill shading if requested
    if( session->view->shaded ){
      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: Applying hill-shading" << endl;
      }
      filter_shade( complete_image, session->view->shade[0], session->view->shade[1] );
      // Don't forget to reset our channels variable as hill shades are greyscale and this variable is used later
      channels = 1;
    }


    // Apply any gamma correction
    if( session->view->getGamma() != 1.0 ){
      float gamma = session->view->getGamma();
      if( session->loglevel >= 3 ){
        *(session->logfile) << "CVT :: Applying gamma of " << gamma << endl; 
      }
      filter_gamma( complete_image, gamma, (*session->image)->max, (*session->image)->min );
    }

    // Resize our image as requested. Use the interpolation method requested in the server configuration.
    //  - Use bilinear interpolation by default
    if( (view_width!=resampled_width) && (view_height!=resampled_height) ){
      Timer interpolation_timer;
      string interpolation_type;
      if( session->loglevel >= 5 ){
	interpolation_timer.start();
      }
      unsigned int interpolation = Environment::getInterpolation();
      switch( interpolation ){
        case 0:
	  interpolation_type = "nearest neighbour";
	  filter_interpolate_nearestneighbour( complete_image, resampled_width, resampled_height );
	  break;
        default:
	  interpolation_type = "bilinear";
	  filter_interpolate_bilinear( complete_image, resampled_width, resampled_height );
	  break;
      }
      if( session->loglevel >= 5 ){
	*(session->logfile) << "CVT :: Resizing using " << interpolation_type << " interpolation in "
			    << interpolation_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply rotation
    if( session->view->getRotation() != 0.0 ){
      Timer rotation_timer;
      if( session->loglevel >= 5 ){
	rotation_timer.start();
      }

      float rotation = session->view->getRotation();
      filter_rotate( complete_image, rotation );

      // For 90 and 270 rotation swap width and height
      resampled_width = complete_image.width;
      resampled_height = complete_image.height;

      if( session->loglevel >= 5 ){
        *(session->logfile) << "CVT :: Rotating image by " << rotation << " degrees in "
			    << rotation_timer.getTime() << " microseconds" << endl; 
      }
    }

    // Apply any contrast adjustments and/or clipping to 8bit from 16bit or 32bit
    filter_contrast( complete_image, session->view->getContrast(), (*session->image)->max, (*session->image)->min );

    // Initialise our JPEG compression object
    session->jpeg->InitCompression( complete_image, resampled_height );

    // Add XMP metadata if this exists
    if( (*session->image)->getMetadata("xmp").size() > 0 ){
      if( session->loglevel >= 4 ) *(session->logfile) << "CVT :: Adding XMP metadata" << endl;
      session->jpeg->addMetadata( (*session->image)->getMetadata("xmp") );
    }

    len = session->jpeg->getHeaderSize();

#ifdef CHUNKED
    snprintf( str, 1024, "%X\r\n", len );
    if( session->loglevel >= 4 ) *(session->logfile) << "CVT :: JPEG Header Chunk : " << str;
    session->out->printf( str );
#endif

    if( session->out->putStr( (const char*) session->jpeg->getHeader(), len ) != len ){
      if( session->loglevel >= 1 ){
	*(session->logfile) << "CVT :: Error writing jpeg header" << endl;
      }
    }

#ifdef CHUNKED
    session->out->printf( "\r\n" );
#endif

    // Flush our block of data
    if( session->out->flush() == -1 ) {
      if( session->loglevel >= 1 ){
	*(session->logfile) << "CVT :: Error flushing jpeg data" << endl;
      }
    }


    // Send out the data per strip of fixed height.
    // Allocate enough memory for this plus an extra 16k for instances where compressed
    // data is greater than uncompressed
    unsigned int strip_height = 128;
    unsigned char* output = new unsigned char[resampled_width*channels*strip_height+16536];
    int strips = (resampled_height/strip_height) + (resampled_height%strip_height == 0 ? 0 : 1);

    for( int n=0; n<strips; n++ ){

      // Get the starting index for this strip of data
      unsigned char* input = &((unsigned char*)complete_image.data)[n*strip_height*resampled_width*channels];

      // The last strip may have a different height
      if( (n==strips-1) && (resampled_height%strip_height!=0) ) strip_height = resampled_height % strip_height;

      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: About to JPEG compress strip with height " << strip_height << endl;
      }

      // Compress the strip
      len = session->jpeg->CompressStrip( input, output, strip_height );

      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: Compressed data strip length is " << len << endl;
      }

#ifdef CHUNKED
      // Send chunk length in hex
      snprintf( str, 1024, "%X\r\n", len );
      if( session->loglevel >= 4 ) *(session->logfile) << "CVT :: Chunk : " << str;
      session->out->printf( str );
#endif

      // Send this strip out to the client
      if( len != session->out->putStr( (const char*) output, len ) ){
	if( session->loglevel >= 1 ){
	  *(session->logfile) << "CVT :: Error writing jpeg strip data: " << len << endl;
	}
      }

#ifdef CHUNKED
      // Send closing chunk CRLF
      session->out->printf( "\r\n" );
#endif

      // Flush our block of data
      if( session->out->flush() == -1 ) {
	if( session->loglevel >= 1 ){
	  *(session->logfile) << "CVT :: Error flushing jpeg data" << endl;
	}
      }

    }

    // Finish off the image compression
    len = session->jpeg->Finish( output );

#ifdef CHUNKED
    snprintf( str, 1024, "%X\r\n", len );
    if( session->loglevel >= 4 ) *(session->logfile) << "CVT :: Final Data Chunk : " << str << endl;
    session->out->printf( str );
#endif

    if( session->out->putStr( (const char*) output, len ) != len ){
      if( session->loglevel >= 1 ){
	*(session->logfile) << "CVT :: Error writing jpeg EOI markers" << endl;
      }
    }

    delete[] output;


#ifdef CHUNKED
    // Send closing chunk CRLF
    session->out->printf( "\r\n" );
    // Send closing blank chunk
    session->out->printf( "0\r\n\r\n" );
#endif

    if( session->out->flush()  == -1 ) {
      if( session->loglevel >= 1 ){
	*(session->logfile) << "CVT :: Error flushing jpeg tile" << endl;
      }
    }

    // Inform our response object that we have sent something to the client
    session->response->setImageSent();


  } // End of if( argument == "jpeg" )


  // Total CVT response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "CVT :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }


}  
