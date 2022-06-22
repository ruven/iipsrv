/*
    IIP JTL Command Handler Class Member Function: Export a single tile

    Copyright (C) 2006-2022 Ruven Pillay.

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


  // If we have requested a rotation, remap the tile index to rotated coordinates
  if( (int)((session->view)->getRotation()) % 360 == 90 ){

  }
  else if( (int)((session->view)->getRotation()) % 360 == 270 ){

  }
  else if( (int)((session->view)->getRotation()) % 360 == 180 ){
    unsigned int im_width = (*session->image)->image_widths[num_res-resolution-1];
    unsigned int im_height = (*session->image)->image_heights[num_res-resolution-1];
    unsigned int tw = (*session->image)->getTileWidth();
    //    unsigned int th = (*session->image)->getTileHeight();
    int ntiles = (int) ceil( (double)im_width/tw ) * (int) ceil( (double)im_height/tw );
    tile = ntiles - tile - 1;
  }


  // Sanity check
  if( (resolution<0) || (tile<0) || (resolution>=num_res) ){
    ostringstream error;
    error << "JTL :: Invalid resolution/tile number: " << resolution << "," << tile;
    throw error.str();
  }


  // Determine which output encoding to use
  CompressionType ct = session->view->output_format;
  Compressor *compressor;
  if( session->view->output_format == JPEG ) compressor = session->jpeg;
#ifdef HAVE_PNG
  else if( session->view->output_format == PNG ) compressor = session->png;
#endif
#ifdef HAVE_WEBP
  else if( session->view->output_format == WEBP ) compressor = session->webp;
#endif
  else compressor = session->jpeg;


  TileManager tilemanager( session->tileCache, *session->image, session->watermark, compressor, session->logfile, session->loglevel );


  // First calculate histogram if we have asked for either binarization,
  //  histogram equalization or contrast stretching
  if( session->view->requireHistogram() && (*session->image)->histogram.empty() ){

    if( session->loglevel >= 4 ) function_timer.start();

    // Retrieve an uncompressed version of our smallest tile
    // which should be sufficient for calculating the histogram
    RawTile thumbnail = tilemanager.getTile( 0, 0, 0, session->view->yangle, session->view->getLayers(), UNCOMPRESSED );

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



  // Request uncompressed tile if raw pixel data is required for processing
  if( (*session->image)->getNumBitsPerPixel() > 8 || (*session->image)->getColourSpace() == CIELAB
      || (*session->image)->getNumChannels() == 2 || (*session->image)->getNumChannels() > 3
      || ( (session->view->colourspace==GREYSCALE || session->view->colourspace==BINARY) && (*session->image)->getNumChannels()==3 &&
	   (*session->image)->getNumBitsPerPixel()==8 )
      || session->view->floatProcessing() || session->view->equalization
      || session->view->getRotation() != 0.0 || session->view->flip != 0
      ) ct = UNCOMPRESSED;


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


  // Embed ICC profile
  if( session->view->embedICC() && ((*session->image)->getMetadata("icc").size()>0) ){
    if( session->loglevel >= 3 ){
      *(session->logfile) << "JTL :: Embedding ICC profile with size "
			  << (*session->image)->getMetadata("icc").size() << " bytes" << endl;
    }
    compressor->setICCProfile( (*session->image)->getMetadata("icc") );
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
  if( rawtile.sampleType == FLOATINGPOINT || session->view->floatProcessing() ){

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
      if( rawtile.bpc > 8 && rawtile.sampleType == FIXEDPOINT ){
	n0 = n0 << (rawtile.bpc-8);
	n1 = n1 << (rawtile.bpc-8);
      }

      min.assign( rawtile.bpc, (float)n0 );
      max.assign( rawtile.bpc, (float)n1 );

      // Reset our contrast
      session->view->contrast = 1.0;

      if( session->loglevel >= 5 ){
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
  if( ( (session->view->output_format == JPEG) && (rawtile.channels == 2 || rawtile.channels > 3) ) ||
      ( (session->view->output_format == PNG) && (rawtile.channels > 4) ) ||
      ( (session->view->output_format == WEBP) && (rawtile.channels > 4) ) ){

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
  if( (*session->image)->getColourSpace() == sRGB && session->view->colourspace == GREYSCALE ){
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
  if( (*session->image)->getColourSpace() != BINARY && session->view->colourspace == BINARY ){
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
    if( session->loglevel >= 5 ){
      flip_timer.start();
    }

    session->processor->flip( rawtile, session->view->flip  );

    if( session->loglevel >= 5 ){
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


  // Compress to requested output format
  if( rawtile.compressionType == UNCOMPRESSED ){
    if( session->loglevel >= 4 ){
      *(session->logfile) << "JTL :: Encoding UNCOMPRESSED tile";
      function_timer.start();
    }
    len = compressor->Compress( rawtile );
    if( session->loglevel >= 4 ){
      *(session->logfile) << " in " << function_timer.getTime() << " microseconds to "
                          << rawtile.dataLength << " bytes" << endl;

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
     *(session->logfile) << "JTL :: Error writing JPEG tile" << endl;
   }
  }


  if( session->out->flush() == -1 ) {
    if( session->loglevel >= 1 ){
      *(session->logfile) << "JTL :: Error flushing JPEG tile" << endl;
    }
  }


  // Inform our response object that we have sent something to the client
  session->response->setImageSent();

  // Total JTL response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "JTL :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
