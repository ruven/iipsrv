/*  IIP AVIF Compressor Class:
    Handles alpha channels, ICC profiles and XMP metadata

    Copyright (C) 2024 Ruven Pillay

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


#include "AVIFCompressor.h"

#if HAVE_STL_THREAD
#include <thread>
#endif

using namespace std;


/// Initialize chunk-based encoding for the CVT handler
void AVIFCompressor::InitCompression( const RawTile& rawtile, unsigned int strip_height ){

  // Manually set up the correct width and height for this particular tile and point to the existing data buffer
  tile.width = rawtile.width;
  tile.height = rawtile.height;
  tile.channels = rawtile.channels;
  tile.bpc = rawtile.bpc;
  tile.data = rawtile.data;
  tile.dataLength = rawtile.dataLength;
  tile.capacity = rawtile.capacity;
  tile.memoryManaged = 0;   // We don't want the RawTile destructor to free this memory

  // libavif cannot handle strip or region-based encoding, so compress the entire image in one go
  this->Compress( tile );

  current_chunk = 0;
}



/// libwebp cannot handle line or region-based encoding, so simulate strip-based output using byte chunks
unsigned int AVIFCompressor::CompressStrip( unsigned char* source, unsigned char* output, unsigned int tile_height ){

  // Initialize our chunk size only once at the start of the sequence
  if( current_chunk == 0 ) chunk_size = (unsigned int)( ( (tile.dataLength*tile_height) + (tile_height/2) ) / tile.height );

  // Make sure we don't over-run our allocated memory
  if( (current_chunk + chunk_size) > (tile.dataLength - 1) ) chunk_size = tile.dataLength - current_chunk;

  // Copy our chunk of data to the given output buffer
  if( chunk_size > 0 ){
    unsigned char* data = (unsigned char*) tile.data;
    memcpy( output, &data[current_chunk], chunk_size );
    current_chunk += chunk_size;
  }

  return chunk_size;
}



unsigned int AVIFCompressor::Finish( unsigned char* output ){

  // Output any remaining bytes
  if( current_chunk < tile.dataLength-1 ){
    unsigned char* data = (unsigned char*) tile.data;
    chunk_size = tile.dataLength - current_chunk - 1;
    memcpy( output, &data[current_chunk], chunk_size );
    return chunk_size;
  }

  return 0;
}



/// Compress a single tile of data
unsigned int AVIFCompressor::Compress( RawTile& rawtile ){

  avifResult OK;
  avifRWData output = AVIF_DATA_EMPTY;

  // Initialize image structure
  avifPixelFormat format = AVIF_PIXEL_FORMAT_YUV420;

#if AVIF_VERSION_MAJOR >= 1
  // Use full 4:4:4 sampling for lossless
  if( this->Q == -1 ) format = AVIF_PIXEL_FORMAT_YUV444;
#endif

  if( rawtile.channels == 1 ) format = AVIF_PIXEL_FORMAT_YUV400;

  // Create our image structure
  avif = avifImageCreate( (uint32_t) rawtile.width, (uint32_t) rawtile.height, (uint32_t) rawtile.bpc, format );
  if( !avif ){
    throw string( "AVIFCompressor :: avifImageCreate() error" );
  }

  avifRGBImage rgb;
  avifRGBImageSetDefaults( &rgb, avif );


  // Set channel layout
  rgb.format = (rawtile.channels==4) ? AVIF_RGB_FORMAT_RGBA : AVIF_RGB_FORMAT_RGB;


  // Monochrome single band input not directly supported - duplicate to 3 identical bands
  if( rawtile.channels == 1 ) rawtile.triplicate();

#if AVIF_VERSION_MAJOR >= 1
  rgb.chromaDownsampling = AVIF_CHROMA_DOWNSAMPLING_FASTEST;
#endif
  rgb.rowBytes = rawtile.width * rawtile.channels * (rawtile.bpc/8);
  rgb.pixels = (uint8_t*) rawtile.data;  // rgb.pixels type is uint8_t even for 10 bit AVIF


  // Initialize encoder
  encoder = avifEncoderCreate();
  if( !encoder ){
    throw string( "AVIFCompressor :: avifEncoderCreate() error" );
  }

  // Set our encoder options
  encoder->codecChoice = this->codec;
  encoder->speed = AVIF_SPEED_FASTEST;


  // Auto-tiling and Quality parameter only exists in version 1 onwards
#if AVIF_VERSION_MAJOR >= 1
  encoder->autoTiling = true;
  if( this->Q == -1 ){
    encoder->quality = AVIF_QUALITY_LOSSLESS;
    encoder->maxQuantizer = AVIF_QUANTIZER_LOSSLESS;
  }
  else encoder->quality = this->Q;
#else
  encoder->maxQuantizer = AVIF_QUANTIZER_WORST_QUALITY;
#endif

  // Set threading concurrency
#if AVIF_VERSION_MAJOR >= 1 && HAVE_STL_THREAD
  rgb.maxThreads = std::thread::hardware_concurrency();
  encoder->maxThreads = rgb.maxThreads;
#endif


  if( (OK=avifImageRGBToYUV( avif, &rgb )) != AVIF_RESULT_OK ){
    throw string( "Failed to convert to YUV(A) " + string(avifResultToString(OK)) );
  }


  // Add ICC profile and XMP metadata to our image
  writeICCProfile();
  writeXMPMetadata();


  if( (OK=avifEncoderAddImage( encoder, avif, 1, AVIF_ADD_IMAGE_FLAG_SINGLE )) != AVIF_RESULT_OK ){
    throw string( "AVIFCompressor :: Failed to add image to encoder: " + string(avifResultToString(OK)) );
  }

  if( (OK=avifEncoderFinish( encoder, &output )) != AVIF_RESULT_OK ){
    throw string( "AVIFCompressor :: Failed to finish encode: " + string(avifResultToString(OK)) );
  }


  // Allocate the appropriate amount of memory if the encoded AVIF is larger than the raw image buffer
  if( output.size > rawtile.capacity ){
    if( rawtile.memoryManaged ) delete[] (unsigned char*) rawtile.data;
    rawtile.data = new unsigned char[output.size];
    rawtile.capacity = output.size;
  }

  // Copy the encoded data back into our rawtile buffer
  memcpy( rawtile.data, output.data, output.size );

  if( avif ){
    avifImageDestroy( avif );
  }
  if( encoder ){
    avifEncoderDestroy( encoder );
  }

  // Return our compressed tile
  rawtile.dataLength = output.size;

  // Free our output structure
  avifRWDataFree( &output );

  rawtile.quality = this->Q;
  rawtile.compressionType = ImageEncoding::AVIF;
  return rawtile.dataLength;
}



/// Write ICC profile
void AVIFCompressor::writeICCProfile()
{
  size_t len = icc.size();
  if( len == 0 ) return;

#if AVIF_VERSION_MAJOR < 1
  // No return from version < 1
  avifImageSetProfileICC( avif, (const uint8_t*) icc.c_str(), len );
#else
  if( avifImageSetProfileICC( avif, (const uint8_t*) icc.c_str(), len ) != AVIF_RESULT_OK ){
    throw string( "AVIFCompressor :: Error adding ICC profile" );
  }
#endif
}



/// Write XMP metadata
void AVIFCompressor::writeXMPMetadata()
{
  size_t len = xmp.size();
  if( len == 0 ) return;

#if AVIF_VERSION_MAJOR < 1
  // No return from version < 1
  avifImageSetMetadataXMP( avif, (const uint8_t*) xmp.c_str(), len );
#else  
  if( avifImageSetMetadataXMP( avif, (const uint8_t*) xmp.c_str(), len ) != AVIF_RESULT_OK ){
    throw string( "AVIFCompressor :: Error adding XMP metadata" );
  }
#endif
}
