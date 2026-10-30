/*
    IIP JTL Command Handler Class Member Function: Export a single tile

    Copyright (C) 2006-2026 Ruven Pillay.

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
#include <iomanip>

using namespace std;


void JTL::send( Session* session, int resolution, int tile ){

  Timer function_timer;

  if( session->loglevel >= 3 ) (*session->logfile) << "JTL handler reached" << endl;


  // Make sure we have set our image
  this->session = session;
  checkImage();


  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // Need to know the number of resolutions
  int num_res = (*session->image)->getNumResolutions();

  // Sanity check the resolution and tile number
  if( (resolution<0) || (tile<0) || (resolution>=num_res) ){
    ostringstream error;
    error << "JTL :: Invalid resolution/tile number: " << resolution << "," << tile;
    throw error.str();
  }


  // If we have requested a rotation, remap the tile index to rotated coordinates
  if( (int)((session->view)->getRotation()) % 360 == 90 ){

  }
  else if( (int)((session->view)->getRotation()) % 360 == 270 ){

  }
  else if( (int)((session->view)->getRotation()) % 360 == 180 ){
    int vipsres = (*session->image)->getNativeResolution( resolution );
    unsigned int im_width = (*session->image)->image_widths[vipsres];
    unsigned int im_height = (*session->image)->image_heights[vipsres];
    unsigned int tw = (*session->image)->tile_widths[vipsres];
    unsigned int th = (*session->image)->tile_heights[vipsres];
    int ntiles = (int) ceil( (double)im_width/tw ) * (int) ceil( (double)im_height/th );
    tile = ntiles - tile - 1;
  }


  // Determine which output encoding to use
  ImageEncoding ct = session->view->output_format;
  Compressor *compressor;
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
  else compressor = session->jpeg;


  TileManager tilemanager( session->tileCache, *session->image, compressor, session->logfile, session->loglevel );


  // First calculate histogram if we have asked for either binarization,
  //  histogram equalization or contrast stretching
  if( session->view->requireHistogram() && (*session->image)->histogram.empty() ){

    if( session->loglevel >= 4 ) function_timer.start();

    // Retrieve an uncompressed version of our smallest tile
    // which should be sufficient for calculating the histogram
    RawTile thumbnail = tilemanager.getTile( 0, 0, 0, session->view->yangle, session->view->getLayers(), ImageEncoding::RAW );

    // Calculate histogram
    (*session->image)->histogram =
      session->processor->histogram( thumbnail, (*session->image)->max, (*session->image)->min );

    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Calculated histogram in "
			  << function_timer.getTime() << " microseconds" << endl;
    }

    // Insert the histogram into our image cache
    const string key = (*session->image)->getImagePath();
    imageCacheMapType::iterator i = session->imageCache->find(key);
    if( i != session->imageCache->end() ) (i->second).histogram = (*session->image)->histogram;
  }


  // Calculate Eigenvectors if required
  if( session->view->requireEigenvectors() && (*session->image)->eigenvectors.empty() ){

#ifdef HAVE_EIGEN3
    if( session->loglevel >= 5 ) function_timer.start();

    // Retrieve an uncompressed version of our smallest tile
    // which should be sufficient for calculating the covariance matrix
    RawTile thumbnail = tilemanager.getTile( 0, 0, 0, session->view->yangle, session->view->getLayers(), ImageEncoding::RAW );

    // Normalize tile to 0->1 and convert to floating point
    session->processor->normalize( thumbnail, (*session->image)->max, (*session->image)->min );

    // Calculate eigenvectors
    (*session->image)->mean = session->processor->mean( thumbnail );
    (*session->image)->covariance = session->processor->covariance( thumbnail, (*session->image)->mean );
    vector<vector<float>> eigen = session->processor->eigenvectors( (*session->image)->covariance );
    (*session->image)->eigenvectors = eigen;

    if( session->loglevel >= 5 ){
      *(session->logfile) << "JTL :: Calculated eigenvectors in "
                          << function_timer.getTime() << " microseconds" << endl;

      *(session->logfile) << "JTL :: Eigenvalues: [" << eigen[0][0] << "," << eigen[0][1] << "," << eigen[0][2] << "]" << endl;
      *(session->logfile) << "JTL :: Eigenvectors: ";
      int n = 0;
      for( int i=0; i<thumbnail.channels; i++ ){
        if( i > 0 ) *(session->logfile) << "                     ";
        for( int j=0; j<thumbnail.channels; j++ ){
          *(session->logfile) << setprecision(6) << eigen[1][n++] << " ";
        }
        *(session->logfile) << endl;
      }
    }

    // Insert the mean, covariance and eigenvectors into our image cache
    const string key = (*session->image)->getImagePath();
    imageCacheMapType::iterator i = session->imageCache->find(key);
    if( i != session->imageCache->end() ){
      (i->second).mean = (*session->image)->mean;
      (i->second).covariance = (*session->image)->covariance;
      (i->second).eigenvectors = (*session->image)->eigenvectors;
    }
#else
    if( session->loglevel >= 1 ){
      *(session->logfile) << "JTL :: Eigen library not enabled: unable to calculate eigenvectors. Disabling PCA and decorrelation stretch" << endl;
    }
#endif
  }


  // Request uncompressed tile if raw pixel data is required for processing
  if( (*session->image)->getNumBitsPerPixel() > 8 || (*session->image)->getColorSpace() == ColorSpace::CIELAB
      || (*session->image)->getNumChannels() == 2 || (*session->image)->getNumChannels() > 3
      || ( (session->view->colorspace==ColorSpace::GREYSCALE || session->view->colorspace==ColorSpace::BINARY) &&
	   (*session->image)->getNumChannels()==3 && (*session->image)->getNumBitsPerPixel()==8 )
      || session->view->floatProcessing() || session->view->equalization
      || session->view->getRotation() != 0.0 || session->view->flip != 0
      || ( session->watermark && (session->watermark)->isSet() )
      ) ct = ImageEncoding::RAW;


  // Set the physical output resolution for this particular view and zoom level
  if( (*session->image)->dpi_x > 0 && (*session->image)->dpi_y > 0 ){
    unsigned int im_width = (*session->image)->image_widths[num_res-resolution-1];
    unsigned int im_height = (*session->image)->image_heights[num_res-resolution-1];
    float dpi_x = (*session->image)->dpi_x * ( (float)im_width / (float)(*session->image)->getImageWidth() );
    float dpi_y = (*session->image)->dpi_y * ( (float)im_height / (float)(*session->image)->getImageHeight() );
    compressor->setResolution( dpi_x, dpi_y, (*session->image)->dpi_units );

    if( session->loglevel >= 5 ){
      *(session->logfile) << "JTL :: Setting physical resolution of tile to " <<  dpi_x << " x " << dpi_y
			  << ( ((*session->image)->dpi_units==1) ? " pixels/inch" : " pixels/cm" ) << endl;
    }
  }


  // Add metadata
  compressor->setMetadata( (*session->image)->metadata );


  // Embed ICC profile if we have one and if embedding has been enabled at start-up
  if( ( session->view->maxICC() != 0 ) && ( (*session->image)->getMetadata("icc").size() > 0 ) ){
    // Only embed if profile is of an acceptable size or if acceptable size is unlimited (-1)
    if( ( session->view->maxICC() == -1 ) || ( (*session->image)->getMetadata("icc").size() < (unsigned long)session->view->maxICC() ) ){
      if( session->loglevel >= 3 ){
	*(session->logfile) << "JTL :: Embedding ICC profile with size "
			    << (*session->image)->getMetadata("icc").size() << " bytes" << endl;
      }
      compressor->embedICCProfile( true );
    }
    else{
      if( session->loglevel >= 3 ){
	*(session->logfile) << "JTL :: ICC profile with size "
			    << (*session->image)->getMetadata("icc").size() << " bytes is too large: Not embedding" << endl;
      }
    }
  }


  // Embed EXIF metadata if we have one and if embedding has been enabled at start-up
  if( ( session->view->maxEXIF() != 0 ) != 0 && ( (*session->image)->getMetadata("exif").size() > 0 ) ){
    // Only embed if EXIF is of an acceptable size or if acceptable size is unlimited (-1)
    if( ( session->view->maxEXIF() == -1 ) || ( (*session->image)->getMetadata("exif").size() < (unsigned long)session->view->maxEXIF() ) ){
      if( session->loglevel >= 3 ){
	*(session->logfile) << "JTL :: Embedding EXIF metadata with size "
			    << (*session->image)->getMetadata("exif").size() << " bytes" << endl;
      }
      compressor->embedExifMetadata( true );
    }
    else{
      if( session->loglevel >= 3 ){
	*(session->logfile) << "JTL :: EXIF metadata with size "
			    << (*session->image)->getMetadata("exif").size() << " bytes is too large: Not embedding" << endl;
      }
    }
  }


  RawTile rawtile = tilemanager.getTile( resolution, tile, session->view->xangle,
					 session->view->yangle, session->view->getLayers(), ct );


  int len = rawtile.dataLength;

  if( session->loglevel >= 2 ){
    *(session->logfile) << "JTL :: Tile size: " << rawtile.width << " x " << rawtile.height << endl
			<< "JTL :: Channels per sample: " << rawtile.channels << endl
			<< "JTL :: Bits per channel: " << rawtile.bpc << endl
			<< "JTL :: Data size: " << len << " bytes" << endl;
  }


  // Convert CIELAB to sRGB
  if( (*session->image)->getColorSpace() == ColorSpace::CIELAB ){

    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Converting from CIELAB->sRGB";
      function_timer.start();
    }
    session->processor->LAB2sRGB( rawtile );
    if( session->loglevel >= 4 ){
      *(session->logfile) << " in " << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Only use our floating point image processing pipeline if necessary
  if( rawtile.sampleType == SampleType::FLOATINGPOINT || session->view->floatProcessing() ){

    // Make a copy of our max and min as we may change these
    vector <float> min = (*session->image)->min;
    vector <float> max = (*session->image)->max;

    // If we have converted from CIELAB, data will already be normalized and in floating point
    if( (*session->image)->getColorSpace() == ColorSpace::CIELAB ){
      min = vector<float>( rawtile.channels, 0.0 );
      max = vector<float>( rawtile.channels, 1.0 );
    }


    // Change our image max and min if we have asked for a contrast stretch
    if( session->view->contrast == -1 && !(*session->image)->histogram.empty() ){

      const vector<unsigned int>& hist = (*session->image)->histogram;

      // Exact pixel count from the smallest image in pyramid
      uint32_t pixels = (*session->image)->image_widths[num_res-1] * (*session->image)->image_heights[num_res-1];

      // Clip is expressed as a percentile: 100.0 for unclipped full range, 98.0 for 2% clipping etc. Calculate number of pixels to clip
      // Note we divide by 200 as clipping is from both top and bottom
      uint32_t clip_count = (uint32_t)( (float)pixels * ( (100.0 - session->view->clip) / 200.0 ) );

      // Find lower bound
      unsigned int n0 = 0;
      uint32_t accum = 0;
      while( n0 < hist.size() - 1 && accum + hist[n0] < clip_count ){
	accum += hist[n0++];
      }

      // Find upper bound
      unsigned int n1 = hist.size() - 1;
      accum = 0;
      while( n1 > n0 && accum + hist[n1] < clip_count ){
	accum += hist[n1--];
      }

      // Safeguard against completely flat/uniform images
      if( n1 <= n0 ){
	n0 = 0;
	n1 = hist.size() - 1;
      }

      // Histogram has been calculated using 8 bits, so scale up to native bit depth
      if( rawtile.bpc > 8 && rawtile.sampleType == SampleType::FIXEDPOINT ){
	uint64_t scale = ((1ULL << rawtile.bpc) - 1) / 255;
	n0 = (unsigned int)(n0 * scale);
	n1 = (unsigned int)(n1 * scale);
      }

      min.assign( rawtile.channels, (float)n0 );
      max.assign( rawtile.channels, (float)n1 );

      // Reset our contrast
      session->view->contrast = 1.0;

      if( session->loglevel >= 4 ){
	*(session->logfile) << "JTL :: Applying contrast stretch for image range of "
			    << n0 << " - " << n1 << endl;
      }
    }


    // Apply normalization and float conversion
    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Normalizing and converting to float";
      function_timer.start();
    }
    session->processor->normalize( rawtile, max, min );
    if( session->loglevel >= 4 ){
      *(session->logfile) << " in " << function_timer.getTime() << " microseconds" << endl;
    }


    // Apply decorrelation stretch
    if( session->view->decorrelation ){
      if( session->loglevel >= 5 ) function_timer.start();
      session->processor->decorrelation( rawtile, (*session->image)->eigenvectors, (*session->image)->mean, (*session->image)->covariance );
      if( session->loglevel >= 5 ){
	*(session->logfile) << "JTL :: Decorrelation stretch applied in "
			    << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply PCA
    if( session->view->pca ){
      if( session->loglevel >= 5 ) function_timer.start();
      session->processor->pca( rawtile, (*session->image)->eigenvectors, (*session->image)->mean );
      if( session->loglevel >= 5 ){
	*(session->logfile) << "JTL :: PCA applied in "
			    << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply hill shading if requested
    if( session->view->shaded ){
      if( session->loglevel >= 4 ){
	*(session->logfile) << "JTL :: Applying hill-shading";
	function_timer.start();
      }
      session->processor->shade( rawtile, session->view->shade[0], session->view->shade[1] );
      if( session->loglevel >= 4 ){
	*(session->logfile) << " in " << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply color twist if requested
    if( session->view->ctw.size() ){
      if( session->loglevel >= 4 ){
	*(session->logfile) << "JTL :: Applying color twist";
	function_timer.start();
      }
      session->processor->twist( rawtile, session->view->ctw );
      if( session->loglevel >= 4 ){
	*(session->logfile) << " in " << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply any gamma or log transform
    if( session->view->gamma != 1.0 ){

      float gamma = session->view->gamma;
      if( session->loglevel >= 4 ) function_timer.start();

      // Check whether we have asked for logarithm
      if( gamma == -1 ) session->processor->log( rawtile );
      else session->processor->gamma( rawtile, gamma );

      if( session->loglevel >= 4 ){
	if( gamma == -1 ) *(session->logfile) << "JTL :: Applying logarithm transform in ";
	else *(session->logfile) << "JTL :: Applying gamma of " << gamma << " in ";
	*(session->logfile) << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply inversion if requested
    if( session->view->inverted ){
      if( session->loglevel >= 4 ){
	*(session->logfile) << "JTL :: Applying inversion";
	function_timer.start();
      }
      session->processor->inv( rawtile );
      if( session->loglevel >= 4 ){
	*(session->logfile) << " in " << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply color mapping if requested
    if( session->view->cmapped ){
      if( session->loglevel >= 4 ){
	*(session->logfile) << "JTL :: Applying color map";
	function_timer.start();
      }
      session->processor->cmap( rawtile, session->view->cmap );
      if( session->loglevel >= 4 ){
	*(session->logfile) << " in " << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply a convolution if requested
    if( session->view->convolution.size() ){
      if( session->loglevel >= 4 ){
	*(session->logfile) << "JTL :: Applying convolution";
	function_timer.start();
      }
      session->processor->convolution( rawtile, session->view->convolution );
      if( session->loglevel >= 4 ){
	*(session->logfile) << " in " << function_timer.getTime() << " microseconds" << endl;
      }
    }


    // Apply any contrast adjustments and/or clip to 8bit from 16 or 32 bit
    float contrast = session->view->contrast;
    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Applying contrast of " << contrast << " and converting to 8 bit";
      function_timer.start();
    }
    session->processor->contrast( rawtile, contrast );
    if( session->loglevel >= 4 ){
      *(session->logfile) << " in " << function_timer.getTime() << " microseconds" << endl;
    }

  }

  // If no image processing is being done, but we have a 32 or 16 bit fixed point image, do a fast rescale to 8 bit
  else if( rawtile.bpc > 8 ){
    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Scaling from " << rawtile.bpc << " to 8 bits per channel in ";
      function_timer.start();
    }
    session->processor->scale_to_8bit( rawtile );
    if( session->loglevel >= 4 ) *(session->logfile) << function_timer.getTime() << " microseconds" << endl;
  } 


  // Reduce to 1 or 3 bands if we have an alpha channel or a multi-band image and have requested a JPEG tile
  // For PNG and WebP, strip extra bands if we have more than 4 present
  if( ( (session->view->output_format == ImageEncoding::JPEG) && (rawtile.channels == 2 || rawtile.channels > 3) ) ||
      ( (session->view->output_format == ImageEncoding::PNG)  && (rawtile.channels  > 4) ) ||
      ( (session->view->output_format == ImageEncoding::WEBP) && (rawtile.channels  > 4) ) ||
      ( (session->view->output_format == ImageEncoding::AVIF) && (rawtile.channels  > 4) ) ){

    unsigned int bands = (rawtile.channels==2) ? 1 : 3;
    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Flattening channels to " << bands;
      function_timer.start();
    }
    session->processor->flatten( rawtile, bands );
    if( session->loglevel >= 4 ){
      *(session->logfile) << " in " << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Convert to greyscale if requested
  if( (*session->image)->getColorSpace() == ColorSpace::sRGB && session->view->colorspace == ColorSpace::GREYSCALE ){
    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Converting to greyscale";
      function_timer.start();
    }
    session->processor->greyscale( rawtile );
    if( session->loglevel >= 4 ){
      *(session->logfile) << " in " << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Convert to binary (bi-level) if requested
  if( (*session->image)->getColorSpace() != ColorSpace::BINARY && session->view->colorspace == ColorSpace::BINARY ){
    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Converting to binary with threshold ";
      function_timer.start();
    }
    unsigned int threshold = session->processor->threshold( (*session->image)->histogram );
    session->processor->binary( rawtile, threshold );
    if( session->loglevel >= 4 ){
      *(session->logfile) << threshold << " in " << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Apply histogram equalization
  if( session->view->equalization ){
    if( session->loglevel >= 4 ) function_timer.start();
    // Perform histogram equalization
    session->processor->equalize( rawtile, (*session->image)->histogram );
    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Applying histogram equalization in "
                          << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Apply flip
  if( session->view->flip != 0 ){
    Timer flip_timer;
    if( session->loglevel >= 4 ){
      flip_timer.start();
    }

    session->processor->flip( rawtile, session->view->flip  );

    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Flipping image ";
      if( session->view->flip == 1 ) *(session->logfile) << "horizontally";
      else *(session->logfile) << "vertically";
      *(session->logfile) << " in " << flip_timer.getTime() << " microseconds" << endl;
    }
  }


  // Apply rotation - can apply this safely after gamma and contrast adjustment
  if( session->view->getRotation() != 0.0 ){
    float rotation = session->view->getRotation();
    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Rotating image by " << rotation << " degrees";
      function_timer.start();
    }
    session->processor->rotate( rawtile, rotation );
    if( session->loglevel >= 4 ){
      *(session->logfile) << " in " << function_timer.getTime() << " microseconds" << endl;
    }
  }


  // Apply the watermark if we have one. This should always be applied last
  if( session->watermark && (session->watermark)->isSet() ){
    if( session->loglevel >= 4 ) function_timer.start();
    session->watermark->apply( rawtile.data, rawtile.width, rawtile.height, rawtile.channels, rawtile.bpc );
    if( session->loglevel >= 4 ) *(session->logfile) << "JTL :: Watermark applied in " << function_timer.getTime()
				 << " microseconds" << endl;
  }


  // Compress to requested output format
  if( rawtile.compressionType == ImageEncoding::RAW ){
    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Compressing raw tile data";
      function_timer.start();
    }
    len = compressor->Compress( rawtile );
    if( session->loglevel >= 4 ){
      *(session->logfile) << " to " << rawtile.dataLength << " bytes in "
			  << function_timer.getTime() << " microseconds" << endl;
    }
  }


#ifndef DEBUG

  // Send HTTP header
  stringstream header;
  header << session->response->createHTTPHeader( compressor->getMimeType(), (*session->image)->getTimestamp(), len );
  if( session->out->putStr( header.str().c_str(), (int) header.tellp() ) == -1 ){
    if( session->loglevel >= 1 ){
      *(session->logfile) << "JTL :: Error writing HTTP header" << endl;
    }
  }

#endif


  if( session->out->putStr( static_cast<const char*>(rawtile.data), len ) != len ){
   if( session->loglevel >= 1 ){
     *(session->logfile) << "JTL :: Error writing tile" << endl;
   }
  }


  if( session->out->flush() == -1 ) {
    if( session->loglevel >= 1 ){
      *(session->logfile) << "JTL :: Error flushing tile" << endl;
    }
  }


  // Inform our response object that we have sent something to the client
  session->response->setImageSent();

  // Total JTL response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "JTL :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
