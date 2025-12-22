/*
    IIP CVT Command Handler Class Member Function

    Copyright (C) 2006-2025 Ruven Pillay.

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
#include <sstream>


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
  if( session->view->output_format == ImageEncoding::JPEG ) compressor = session->jpeg;
  else if( session->view->output_format == ImageEncoding::TIFF ) compressor = session->tiff;
#ifdef HAVE_PNG
  else if( session->view->output_format == ImageEncoding::PNG ) compressor = session->png;
#endif
#ifdef HAVE_WEBP
  else if( session->view->output_format == ImageEncoding::WEBP ) compressor = session->webp;
#endif
#ifdef HAVE_AVIF
  else if( session->view->output_format == ImageEncoding::AVIF ) compressor = session->avif;
#endif
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
  int requested_res = session->view->getResolution( (*session->image)->image_widths, (*session->image)->image_heights );
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

    vector<unsigned int> requestSize = session->view->getRequestSize();
    resampled_width = requestSize[0];
    resampled_height = requestSize[1];

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
    vector<unsigned int> requestSize = session->view->getRequestSize();
    resampled_width = requestSize[0];
    resampled_height = requestSize[1];
  }


  // If we have requested that upscaling of images be prevented adjust requested size accordingly
  // N.B. im_width and height here are from the requested resolution and not the max resolution
  if( !session->view->allow_upscaling ){
    if(resampled_width > im_width) resampled_width = im_width;
    if(resampled_height > im_height) resampled_height = im_height;
  }


  // Make sure we don't have zero sized dimensions
  if( resampled_width == 0 ) resampled_width = session->view->getMinSize();
  if( resampled_height == 0 ) resampled_height = session->view->getMinSize();

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

  // Add output size to name and change suffix to match requested format
  ostringstream basename;
  basename << filename.substr( pos, filename.rfind(".")-pos ) << "_" << resampled_width << "x" << resampled_height << "." << compressor->getSuffix();

  // Set our content disposition type: use "attachment" for POST requests which will make browser download rather than display image
  session->response->setContentDisposition( basename.str(), ((session->headers["REQUEST_METHOD"]=="POST")?"attachment":"inline") );
  string header = session->response->createHTTPHeader( compressor->getMimeType(), (*session->image)->getTimestamp() );

  if( session->out->putS( header.c_str() ) == -1 ){
    if( session->loglevel >= 1 ){
      *(session->logfile) << "CVT :: Error writing HTTP header" << endl;
    }
  }
#endif


  // Set up our TileManager object
  TileManager tilemanager( session->tileCache, *session->image, compressor, session->logfile, session->loglevel );


  // First calculate histogram if we have asked for either binarization,
  //  histogram equalization or contrast stretching
  if( session->view->requireHistogram() && (*session->image)->histogram.empty() &&
      (*session->image)->getColorSpace() != ColorSpace::BINARY ){

    if( session->loglevel >= 5 ) function_timer.start();

    // Retrieve an uncompressed version of our smallest tile
    // which should be sufficient for calculating the histogram
    RawTile thumbnail = tilemanager.getTile( 0, 0, 0, session->view->yangle, session->view->getLayers(), ImageEncoding::RAW );

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
  if( session->loglevel >= 2 ) function_timer.start();
  RawTile complete_image = tilemanager.getRegion( requested_res,
						  session->view->xangle, session->view->yangle,
						  session->view->getLayers(),
						  view_left, view_top, view_width, view_height );
  if( session->loglevel >= 2 ){
    *(session->logfile) << "CVT :: Region decoding time: "
			<< function_timer.getTime() << " microseconds" << endl;
  }


  // Convert CIELAB to sRGB
  if( (*session->image)->getColorSpace() == ColorSpace::CIELAB ){
    if( session->loglevel >= 5 ) function_timer.start();
    session->processor->LAB2sRGB( complete_image );
    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Converting from CIELAB->sRGB in "
			  << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Only use our floating point image processing pipeline if necessary
  if( complete_image.sampleType == SampleType::FLOATINGPOINT || session->view->floatProcessing() ){

    // Make a copy of our max and min as we may change these
    vector <float> min = (*session->image)->min;
    vector <float> max = (*session->image)->max;

    // If we have converted from CIELAB, data will already be normalized and in floating point
    if( (*session->image)->getColorSpace() == ColorSpace::CIELAB ){
      min = vector<float>( complete_image.channels, 0.0 );
      max = vector<float>( complete_image.channels, 1.0 );
    }

    // Change our image max and min if we have asked for a contrast stretch
    if( session->view->contrast == -1 ){

      // Find first non-zero bin in histogram
      unsigned int n0 = 0;
      while( (*session->image)->histogram[n0] == 0 ) ++n0;

      // Find highest bin
      unsigned int n1 = (*session->image)->histogram.size() - 1;
      while( (*session->image)->histogram[n1] == 0 ) --n1;

      // Histogram has been calculated using 8 bits, so scale up to native bit depth
      if( complete_image.bpc > 8 && complete_image.sampleType == SampleType::FIXEDPOINT ){
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


    // Apply any gamma or log transform
    if( session->view->gamma != 1.0 ){
      float gamma = session->view->gamma;
      if( session->loglevel >= 5 ) function_timer.start();

      // Check whether we have asked for logarithm
      if( gamma == -1 ) session->processor->log( complete_image );
      else session->processor->gamma( complete_image, gamma );

      if( session->loglevel >= 5 ){
	if( gamma == -1 ) *(session->logfile) << "CVT :: Applying logarithm transform in ";
	else *(session->logfile) << "CVT :: Applying gamma of " << gamma << " in ";
	*(session->logfile) << function_timer.getTime() << " microseconds" << endl;
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


    // Apply convolution
    if( session->view->convolution.size() ){

      if( session->loglevel >= 5 ) function_timer.start();

      // Apply convolution
      session->processor->convolution( complete_image, session->view->convolution );

      if( session->loglevel >= 5 ){
	*(session->logfile) << "CVT :: Convolution applied in "
			    << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply any contrast adjustments and scale to 8 bit quantization
    {
      if( session->loglevel >= 5 ) function_timer.start();
      session->processor->contrast( complete_image, session->view->contrast );
      if( session->loglevel >= 5 ){
	*(session->logfile) << "CVT :: Applying contrast of " << session->view->contrast
			    << " and converting to 8bit in " << function_timer.getTime() << " microseconds" << endl;
      }
    }
  }

  // If no image processing is being done, but we have a 32 or 16 bit fixed point image, do a fast rescale to 8 bit
  else if( complete_image.bpc > 8 ){
    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Scaling from " << complete_image.bpc << " to 8 bits per channel in ";
      function_timer.start();
    }
    session->processor->scale_to_8bit( complete_image );
    if( session->loglevel >= 5 ) *(session->logfile) << function_timer.getTime() << " microseconds" << endl;
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


  // Reduce to 1 or 3 bands if we have an alpha channel or a multi-band image and have requested a JPEG tile
  // For PNG and WebP, strip extra bands if we have more than 4 present
  if( ( (session->view->output_format == ImageEncoding::JPEG) && (complete_image.channels == 2 || complete_image.channels > 3) ) ||
      ( (session->view->output_format == ImageEncoding::PNG)  && (complete_image.channels  > 4) ) ||
      ( (session->view->output_format == ImageEncoding::WEBP) && (complete_image.channels  > 4) ) ||
      ( (session->view->output_format == ImageEncoding::AVIF) && (complete_image.channels  > 4) ) ){

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
  if( (*session->image)->getColorSpace() == ColorSpace::sRGB && session->view->colorspace == ColorSpace::GREYSCALE ){

    if( session->loglevel >= 5 ) function_timer.start();

    session->processor->greyscale( complete_image );

    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Converting to greyscale in "
			  << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Convert to binary (bi-level) if requested
  if( (*session->image)->getColorSpace() != ColorSpace::BINARY && session->view->colorspace == ColorSpace::BINARY ){

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
      *(session->logfile) << "CVT :: Flipping image " << direction << " in "
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


  // Apply the watermark if we have one. This should always be applied last
  if( session->watermark && (session->watermark)->isSet() ){

    if( session->loglevel >= 5 ) function_timer.start();

    unsigned int tw = (*session->image)->getTileWidth();
    unsigned int th = (*session->image)->getTileHeight();

    // Use a watermark block size of 2x the tile size of the image
    session->watermark->apply( complete_image.data, complete_image.width, complete_image.height,
			       complete_image.channels, complete_image.bpc, (tw>th ? tw : th)*2 );

    if( session->loglevel >= 5 ) *(session->logfile) << "CVT :: Watermark applied in " << function_timer.getTime()
						     << " microseconds" << endl;
  }


  // Add metadata
  compressor->setMetadata( (*session->image)->metadata );


  // Set the physical output resolution for this particular view and zoom level
  if( (*session->image)->dpi_x > 0 && (*session->image)->dpi_y > 0 ){
    float dpi_x = (*session->image)->dpi_x * (float) im_width / (float) (*session->image)->getImageWidth();
    float dpi_y = (*session->image)->dpi_y * (float) im_height / (float) (*session->image)->getImageHeight();
    compressor->setResolution( dpi_x, dpi_y, (*session->image)->dpi_units );
    if( session->loglevel >= 5 ){
      *(session->logfile) << "CVT :: Setting physical resolution of this view to " <<  dpi_x << " x " << dpi_y
			  << ( ((*session->image)->dpi_units==1) ? " pixels/inch" : " pixels/cm" ) << endl;
    }
  }


  // Embed ICC profile if we have one and if embedding has been enabled at start-up
  if( ( session->view->maxICC() != 0 ) && ( (*session->image)->getMetadata("icc").size() > 0 ) ){
    // Only embed if profile is of an acceptable size or if acceptable size is unlimited (-1)
    if( ( session->view->maxICC() == -1 ) || ( (*session->image)->getMetadata("icc").size() < (unsigned long)session->view->maxICC() ) ){
      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: Embedding ICC profile with size "
			    << (*session->image)->getMetadata("icc").size() << " bytes" << endl;
      }
      compressor->embedICCProfile( true );
    }
    else{
      if( session->loglevel >= 3 ){
	*(session->logfile) << "CVT :: ICC profile with size "
			    << (*session->image)->getMetadata("icc").size() << " bytes is too large: Not embedding" << endl;
      }
    }
  }


  // Always embed XMP metadata in CVT function
  if( (*session->image)->getMetadata("xmp").size() > 0 ){
    if( session->loglevel >= 3 ){
      *(session->logfile) << "CVT :: Embedding XMP metadata with size "
			  << (*session->image)->getMetadata("xmp").size() << " bytes" << endl;
    }
    compressor->embedXMPMetadata( true );
  }

  // Always embed EXIF metadata in CVT function
  if( (*session->image)->getMetadata("exif").size() > 0 ){
    if( session->loglevel >= 3 ){
      *(session->logfile) << "CVT :: Embedding EXIF metadata with size "
			  << (*session->image)->getMetadata("exif").size() << " bytes" << endl;
    }
    compressor->embedExifMetadata( true );
  }


  // Initialise our output compression object
  compressor->InitCompression( complete_image, resampled_height );


  len = compressor->getHeaderSize();

  if( session->out->putStr( (const char*) compressor->getHeader(), len ) != len ){
    if( session->loglevel >= 1 ){
      *(session->logfile) << "CVT :: Error writing header" << endl;
    }
  }


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

    // Send this strip out to the client
    if( len != session->out->putStr( (const char*) output, len ) ){
      if( session->loglevel >= 1 ){
	*(session->logfile) << "CVT :: Error writing strip: " << len << endl;
      }
    }

    // Flush our block of data
    if( session->out->flush() == -1 ) {
      if( session->loglevel >= 1 ){
	*(session->logfile) << "CVT :: Error flushing data" << endl;
      }
    }

  }

  // Finish off the image compression
  len = compressor->Finish( output );

  if( session->out->putStr( (const char*) output, len ) != len ){
    if( session->loglevel >= 1 ){
      *(session->logfile) << "CVT :: Error writing output" << endl;
    }
  }

  // Delete our output buffer
  delete[] output;


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

