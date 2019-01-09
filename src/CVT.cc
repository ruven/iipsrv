/*
    IIP CVT Command Handler Class Member Function

    Copyright (C) 2006-2019 Ruven Pillay.

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



void CVT::send( Session* session ){

  Timer function_timer;


  if( session->loglevel >= 2 ) *(session->logfile) << "CVT handler reached" << endl;


  // Make sure we have set our image
  this->session = session;
  checkImage();


  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // Set up our output format handler
  Compressor *compressor = NULL;
  if( session->view->output_format == JPEG ) compressor = session->jpeg;
  else return;


  // Reload info in case we are dealing with a sequence
  //(*session->image)->loadImageInfo( session->view->xangle, session->view->yangle );


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
    *(session->logfile) << "CVT :: Using resolution " << requested_res << " with size " << im_width << "x" << im_height << endl;
  }


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

    // Make sure our region fits within the image
    if( view_width + view_left > im_width ) view_width = im_width - view_left;
    if( view_height + view_top > im_height ) view_height = im_height - view_top;

    resampled_width = session->view->getRequestWidth();
    resampled_height = session->view->getRequestHeight();

    if( session->loglevel >= 3 ){
      *(session->logfile) << "CVT :: Region: " << view_left << "," << view_top
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
  }


  // If we have requested that upscaling of images be prevented adjust requested size accordingly
  // N.B. im_width and height here are from the requested resolution and not the max resolution
  if( !session->view->allow_upscaling ){
    if(resampled_width > im_width) resampled_width = im_width;
    if(resampled_height > im_height) resampled_height = im_height;
  }

  // If we have requested that the aspect ratio be maintained, make sure the final image fits *within* the requested size.
  // Don't adjust images if we have less than 0.1% difference as this is often due to rounding in resolution levels
  if( session->view->maintain_aspect ){
    float ratio = ((float)resampled_width/(float)view_width) / ((float)resampled_height/(float)view_height);
    if( ratio < 1.001 ){
      resampled_height = (unsigned int) round((((float)resampled_width/(float)view_width) * (float)view_height));
    }
    else if( ratio > 1.001 ){
      resampled_width = (unsigned int) round((((float)resampled_height/(float)view_height) * (float)view_width));
    }
  }


  if( session->loglevel >= 3 ){
    *(session->logfile) << "CVT :: Requested scaled region size is " << resampled_width << "x" << resampled_height
			<< ". Nearest existing resolution is " << requested_res
			<< " which has region with size " << view_width << "x" << view_height << endl;
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
  snprintf( str, 1024,
	    "Server: iipsrv/%s\r\n"
	    "X-Powered-By: IIPImage\r\n"
	    "%s\r\n"
	    "Last-Modified: %s\r\n"
	    "Content-Type: %s\r\n"
	    "Content-Disposition: inline;filename=\"%s.%s\"\r\n"
#ifdef CHUNKED
	    "Transfer-Encoding: chunked\r\n"
#endif
	    "\r\n",
	    VERSION, session->response->getCacheControl().c_str(),
	    (*session->image)->getTimestamp().c_str(),
	    compressor->getMimeType(), basename.c_str(), compressor->getSuffix() );

  session->out->printf( (const char*) str );
#endif



  // Set up our TileManager object
  TileManager tilemanager( session->tileCache, *session->image, session->watermark, compressor, session->logfile, session->loglevel );


  // First calculate histogram if we have asked for either binarization,
  //  histogram equalization or contrast stretching
  if( session->view->requireHistogram() && (*session->image)->histogram.size()==0 ){

    if( session->loglevel >= 5 ) function_timer.start();

    // Retrieve an uncompressed version of our smallest tile
    // which should be sufficient for calculating the histogram
    RawTile thumbnail = tilemanager.getTile( 0, 0, 0, session->view->yangle, session->view->getLayers(), UNCOMPRESSED );

    // Calculate histogram
    (*session->image)->histogram =
      session->processor->histogram( thumbnail, (*session->image)->max, (*session->image)->min );

    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Calculated histogram in "
			  << function_timer.getTime() << " microseconds" << endl;
    }

    // Insert the histogram into our image cache
    const string key = (*session->image)->getImagePath();
    imageCacheMapType::iterator i = session->imageCache->find(key);
    if( i != session->imageCache->end() ) (i->second).histogram = (*session->image)->histogram;
  }



  // Retrieve image region
  RawTile complete_image = tilemanager.getRegion( requested_res,
						  session->view->xangle, session->view->yangle,
						  session->view->getLayers(),
						  view_left, view_top, view_width, view_height );



  // Convert CIELAB to sRGB
  if( (*session->image)->getColourSpace() == CIELAB ){
    if( session->loglevel >= 5 ) function_timer.start();
    session->processor->LAB2sRGB( complete_image );
    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Converting from CIELAB->sRGB in "
			  << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Only use our floating point pipeline if necessary
  if( complete_image.bpc > 8 || session->view->floatProcessing() ){


    // Make a copy of our max and min as we may change these
    vector <float> min = (*session->image)->min;
    vector <float> max = (*session->image)->max;

    // Change our image max and min if we have asked for a contrast stretch
    if( session->view->contrast == -1 ){

      // Find first non-zero bin in histogram
      unsigned int n0 = 0;
      while( (*session->image)->histogram[n0] == 0 ) ++n0;

      // Find highest bin
      unsigned int n1 = (*session->image)->histogram.size() - 1;
      while( (*session->image)->histogram[n1] == 0 ) --n1;

      // Histogram has been calculated using 8 bits, so scale up to native bit depth
      if( complete_image.bpc > 8 && complete_image.sampleType == FIXEDPOINT ){
	n0 = n0 << (complete_image.bpc-8);
	n1 = n1 << (complete_image.bpc-8);
      }

      min.assign( complete_image.bpc, (float)n0 );
      max.assign( complete_image.bpc, (float)n1 );

      // Reset our contrast
      session->view->contrast = 1.0;

      if( session->loglevel >= 5 ){
	*(session->logfile) << "CVT :: Applying contrast stretch for image range of "
			    << n0 << " - " << n1 << endl;
      }
    }


    // Apply normalization and perform float conversion
    {
      if( session->loglevel >= 5 ) function_timer.start();
      session->processor->normalize( complete_image, max, min );
      if( session->loglevel >= 5 ){
	*(session->logfile) << "CVT :: Converting to floating point and normalizing in "
			    << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply hill shading if requested
    if( session->view->shaded ){
      if( session->loglevel >= 5 ) function_timer.start();
      session->processor->shade( complete_image, session->view->shade[0], session->view->shade[1] );
      if( session->loglevel >= 5 ){
	*(session->logfile) << "CVT :: Applying hill-shading in " << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply color twist if requested
    if( session->view->ctw.size() ){
      if( session->loglevel >= 5 ) function_timer.start();
      session->processor->twist( complete_image, session->view->ctw );
      if( session->loglevel >= 5 ){
	*(session->logfile) << "CVT :: Applying color twist in " << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply any gamma correction
    if( session->view->gamma != 1.0 ){
      float gamma = session->view->gamma;
      if( session->loglevel >= 5 ) function_timer.start();
      session->processor->gamma( complete_image, gamma );
      if( session->loglevel >= 5 ){
	*(session->logfile) << "CVT :: Applying gamma of " << gamma << " in "
			    << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply inversion if requested
    if( session->view->inverted ){
      if( session->loglevel >= 5 ) function_timer.start();
      session->processor->inv( complete_image );
      if( session->loglevel >= 5 ){
	*(session->logfile) << "CVT :: Applying inversion in " << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply color mapping if requested
    if( session->view->cmapped ){
      if( session->loglevel >= 5 ) function_timer.start();
      session->processor->cmap( complete_image, session->view->cmap );
      if( session->loglevel >= 5 ){
	*(session->logfile) << "CVT :: Applying color map in " << function_timer.getTime() << " microseconds" << endl;
      }
    }



    // Apply any contrast adjustments and/or clip from 16bit or 32bit to 8bit
    {
      if( session->loglevel >= 5 ) function_timer.start();
      session->processor->contrast( complete_image, session->view->contrast );
      if( session->loglevel >= 5 ){
	*(session->logfile) << "CVT :: Applying contrast of " << session->view->contrast
			    << " and converting to 8bit in " << function_timer.getTime() << " microseconds" << endl;
      }
    }
  }


  // Resize our image as requested. Use the interpolation method requested in the server configuration.
  //  - Use bilinear interpolation by default
  if( (view_width!=resampled_width) || (view_height!=resampled_height) ){

    string interpolation_type;
    if( session->loglevel >= 5 ) function_timer.start();

    unsigned int interpolation = Environment::getInterpolation();
    switch( interpolation ){
     case 0:
      interpolation_type = "nearest neighbour";
      session->processor->interpolate_nearestneighbour( complete_image, resampled_width, resampled_height );
      break;
     default:
      interpolation_type = "bilinear";
      session->processor->interpolate_bilinear( complete_image, resampled_width, resampled_height );
      break;
    }

    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Resizing using " << interpolation_type << " interpolation in "
			  << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Reduce to 1 or 3 bands if we have an alpha channel or a multi-band image
  if( (complete_image.channels==2) || (complete_image.channels>3 ) ){

    int output_channels = (complete_image.channels==2)? 1 : 3;
    if( session->loglevel >= 5 ) function_timer.start();

    session->processor->flatten( complete_image, output_channels );

    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Flattening to " << output_channels << " channel"
			  << ((output_channels>1) ? "s" : "") << " in "
			  << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Convert to greyscale if requested
  if( (*session->image)->getColourSpace() == sRGB && session->view->colourspace == GREYSCALE ){

    if( session->loglevel >= 5 ) function_timer.start();

    session->processor->greyscale( complete_image );

    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Converting to greyscale in "
			  << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Convert to binary (bi-level) if requested
  if( session->view->colourspace == BINARY ){

    if( session->loglevel >= 5 ) function_timer.start();

    // Calculate threshold from histogram
    unsigned char threshold = session->processor->threshold( (*session->image)->histogram );

    // Apply threshold to create binary (bi-level) image
    session->processor->binary( complete_image, threshold );

    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Converting to binary with threshold " << (unsigned int) threshold
                          << " in " << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Apply histogram equalization
  if( session->view->equalization ){

    if( session->loglevel >= 5 ) function_timer.start();

    // Perform histogram equalization
    session->processor->equalize( complete_image, (*session->image)->histogram );

    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Histogram equalization applied in "
                          << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Apply flip
  if( session->view->flip != 0 ){

    if( session->loglevel >= 5 ) function_timer.start();

    session->processor->flip( complete_image, session->view->flip  );

    if( session->loglevel >= 5 ){
      string direction = session->view->flip==1 ? "horizontally" : "vertically";
      *(session->logfile) << "JTL :: Flipping image " << direction << " in "
			  << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Apply rotation - can apply this safely after gamma and contrast adjustment
  if( session->view->getRotation() != 0.0 ){

    if( session->loglevel >= 5 ) function_timer.start();

    float rotation = session->view->getRotation();
    session->processor->rotate( complete_image, rotation );

    // For 90 and 270 rotation swap width and height
    resampled_width = complete_image.width;
    resampled_height = complete_image.height;

    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Rotating image by " << rotation << " degrees in "
			  << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Set ICC profile if of a reasonable size
  if( session->view->embedICC() && ((*session->image)->getMetadata("icc").size()>0) ){
    if( (*session->image)->getMetadata("icc").size() < 65536 ){
      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: Embedding ICC profile with size "
			    << (*session->image)->getMetadata("icc").size() << " bytes" << endl;
      }
      compressor->setICCProfile( (*session->image)->getMetadata("icc") );
    }
    else{
      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: ICC profile with size "
			    << (*session->image)->getMetadata("icc").size() << " bytes is too large: Not embedding" << endl;
      }
    }
  }

  // Add XMP metadata if this exists
  if( (*session->image)->getMetadata("xmp").size() > 0 ){
    if( session->loglevel >= 3 ){
      *(session->logfile) << "CVT :: Embedding XMP metadata with size "
			  << (*session->image)->getMetadata("xmp").size() << " bytes" << endl;
    }
    compressor->setXMPMetadata( (*session->image)->getMetadata("xmp") );
  }


  // Initialise our output compression object
  compressor->InitCompression( complete_image, resampled_height );


  len = compressor->getHeaderSize();

#ifdef CHUNKED
  snprintf( str, 1024, "%X\r\n", len );
  if( session->loglevel >= 4 ) *(session->logfile) << "CVT :: Output Header Chunk : " << str;
  session->out->printf( str );
#endif

  if( session->out->putStr( (const char*) compressor->getHeader(), len ) != len ){
    if( session->loglevel >= 1 ){
      *(session->logfile) << "CVT :: Error writing header" << endl;
    }
  }

#ifdef CHUNKED
  session->out->printf( "\r\n" );
#endif

  // Flush our block of data
  if( session->out->flush() == -1 ) {
    if( session->loglevel >= 1 ){
      *(session->logfile) << "CVT :: Error flushing output data" << endl;
    }
  }


  // Send out the data per strip of fixed height.
  // Allocate enough memory for this plus an extra 64k for instances where compressed
  // data is greater than uncompressed
  unsigned int strip_height = 128;
  unsigned int channels = complete_image.channels;
  unsigned char* output = new unsigned char[resampled_width*channels*strip_height+65536];
  int strips = (resampled_height/strip_height) + (resampled_height%strip_height == 0 ? 0 : 1);

  for( int n=0; n<strips; n++ ){

    // Get the starting index for this strip of data
    unsigned char* input = &((unsigned char*)complete_image.data)[n*strip_height*resampled_width*channels];

    // The last strip may have a different height
    if( (n==strips-1) && (resampled_height%strip_height!=0) ) strip_height = resampled_height % strip_height;

    if( session->loglevel >= 3 ){
      *(session->logfile) << "CVT :: About to compress strip with height " << strip_height << endl;
    }

    // Compress the strip
    len = compressor->CompressStrip( input, output, strip_height );

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
	*(session->logfile) << "CVT :: Error writing strip: " << len << endl;
      }
    }

#ifdef CHUNKED
    // Send closing chunk CRLF
    session->out->printf( "\r\n" );
#endif

    // Flush our block of data
    if( session->out->flush() == -1 ) {
      if( session->loglevel >= 1 ){
	*(session->logfile) << "CVT :: Error flushing data" << endl;
      }
    }

  }

  // Finish off the image compression
  len = compressor->Finish( output );

#ifdef CHUNKED
  snprintf( str, 1024, "%X\r\n", len );
  if( session->loglevel >= 4 ) *(session->logfile) << "CVT :: Final Data Chunk : " << str << endl;
  session->out->printf( str );
#endif

  if( session->out->putStr( (const char*) output, len ) != len ){
    if( session->loglevel >= 1 ){
      *(session->logfile) << "CVT :: Error writing output" << endl;
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
      *(session->logfile) << "CVT :: Error flushing output" << endl;
    }
  }

  // Inform our response object that we have sent something to the client
  session->response->setImageSent();



  // Total CVT response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "CVT :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }


}

