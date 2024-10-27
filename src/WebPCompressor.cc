/*  IIP WebP Compressor Class:
    Handles alpha channels, ICC profiles and XMP metadata

    Copyright (C) 2022-2024 Ruven Pillay

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


#include "WebPCompressor.h"

using namespace std;



/// Initialize chunk-based encoding for the CVT handler
void WebPCompressor::InitCompression( const RawTile& rawtile, unsigned int strip_height ){

  // Manually set up the correct width and height for this particular tile and point to the existing data buffer
  tile.width = rawtile.width;
  tile.height = rawtile.height;
  tile.channels = rawtile.channels;
  tile.bpc = rawtile.bpc;
  tile.data = rawtile.data;
  tile.dataLength = rawtile.dataLength;
  tile.memoryManaged = 0;   // We don't want the RawTile destructor to free this memory

  // libwebp cannot handle strip or region-based encoding, so compress the entire image in one go
  this->Compress( tile );

  current_chunk = 0;
}



/// libwebp cannot handle line or region-based encoding, so simulate strip-based output using byte chunks
unsigned int WebPCompressor::CompressStrip( unsigned char* source, unsigned char* output, unsigned int tile_height ){

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



unsigned int WebPCompressor::Finish( unsigned char* output ){

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
unsigned int WebPCompressor::Compress( RawTile& rawtile ){

  // WebP cannot handle greyscale, so duplicate our data to 3 bands
  if( rawtile.channels == 1 ) rawtile.triplicate();

  // Import data from our RawTile structure
  const uint8_t* rgb = (uint8_t*) rawtile.data;

  // Create WebP input data structure
  WebPPicture pic;
  WebPPictureInit( &pic );

  pic.use_argb = false;
  pic.width = rawtile.width;
  pic.height = rawtile.height;
  //  WebPPictureAlloc( &pic );       // WebPPictureImport calls this itself

  WebPMemoryWriter writer;
  WebPMemoryWriterInit( &writer );
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &writer;


  if( rawtile.channels == 4 ){
    if( WebPPictureImportRGBA( &pic, rgb, rawtile.width * rawtile.channels ) == 0 ){
      throw string( "WebPCompressor :: WebPPictureImportRGBA() error" );
    }
  }
  else{
    if( WebPPictureImportRGB( &pic, rgb, rawtile.width * rawtile.channels ) == 0 ){
      throw string( "WebPCompressor :: WebPPictureImportRGB() error" );
    }
  }


  // Encode our image buffer
  WebPEncode( &config, &pic );

  const uint8_t* buffer;
  size_t size = 0;
  WebPData output;

  // Use the WebP muxer only if we need to
  if( icc.size() > 0 || xmp.size() > 0 ){

    // Add ICC profile and XMP metadata to our output bitstream
    writeICCProfile();
    writeXMPMetadata();
  
    // Add our image data chunk
    WebPData chunk;
    chunk.bytes = writer.mem;
    chunk.size = writer.size;
    WebPMuxSetImage( mux, &chunk, 0 );

    // Assemble our chunks
    if( WebPMuxAssemble( mux, &output ) != WEBP_MUX_OK ){
      throw string( "WebPCompressor :: WebPMuxAssemble() error" );
    }

    buffer = output.bytes;
    size = output.size;
  }
  else{
    buffer = writer.mem;
    size = writer.size;
  }


  // Allocate the appropriate amount of memory if the encoded WebP is larger than the raw image buffer
  if( size > rawtile.capacity ){
    if( rawtile.memoryManaged ) delete[] (unsigned char*) rawtile.data;
    rawtile.data = new unsigned char[size];
    rawtile.capacity = size;
  }


  // Copy our data back into our rawtile buffer
  memcpy( rawtile.data, buffer, size );
  rawtile.dataLength = size;


  // Free allocated structures
  WebPPictureFree( &pic );
  WebPMemoryWriterClear( &writer );
  if( icc.size() > 0 || xmp.size() > 0 ) WebPDataClear( &output );


  // Return our compressed tile
  rawtile.quality = this->Q;
  rawtile.compressionType = ImageEncoding::WEBP;
  return rawtile.dataLength;
}



/// Write ICC profile
void WebPCompressor::writeICCProfile()
{
  // Skip if profile embedding disabled or no profile exists
  if( !embedICC || icc.empty() ) return;

  WebPData chunk;
  chunk.bytes = (const uint8_t*) icc.c_str();
  chunk.size = icc.size();

  if( WebPMuxSetChunk( mux, "ICCP", &chunk, 0 ) != WEBP_MUX_OK ){
    throw string( "WebPCompressor :: Error setting ICC profile chunk" );
  }                                               ;
}



/// Write XMP metadata
void WebPCompressor::writeXMPMetadata()
{
  // Skip if XMP embedding disabled or no XMP chunk exists
  if( !embedXMP || xmp.empty() ) return;

  WebPData chunk;
  chunk.bytes = (const uint8_t*) xmp.c_str();
  chunk.size = xmp.size();

  if( WebPMuxSetChunk( mux, "XMP ", &chunk, 0 ) != WEBP_MUX_OK ){
    throw string( "WebPCompressor :: Error setting XMP chunk" );
  }
}



void WebPCompressor::injectMetadata( RawTile& rawtile )
{
  if( (!embedICC && !embedXMP) || (icc.empty() && xmp.empty()) ) return;

  WebPData input;
  input.bytes = (const uint8_t*) rawtile.data;
  input.size = rawtile.dataLength;

  // Only add ICC or metadata if we have a raw WebP stream
  // Bytes 8-16 should be exactly "WEBPVP8 " (lossy) or "WEBPVP8L" (lossless)
  static const unsigned char lossy_header[8] = {0x57,0x45,0x42,0x50,0x56,0x50,0x38,0x20};
  static const unsigned char lossless_header[8] = {0x57,0x45,0x42,0x50,0x56,0x50,0x38,0x4c};

  if( (memcmp( &input.bytes[8], lossy_header, 8 ) == 0) ||
      (memcmp( &input.bytes[8], lossless_header, 8 ) == 0) ){

    WebPData output;

    // Add ICC profile and XMP metadata to our output bitstream
    writeICCProfile();
    writeXMPMetadata();

    // Add our raw image bitstream data
    if( WebPMuxSetImage( mux, &input, 0 ) != WEBP_MUX_OK ){
      throw string( "WebPCompressor :: WebPMuxSetImage() error" );
    }

    // Assemble our chunks
    if( WebPMuxAssemble( mux, &output ) != WEBP_MUX_OK ){
      throw string( "WebPCompressor :: WebPMuxAssemble() error" );
    }

    // Allocate the appropriate amount of memory for the final muxed data
    unsigned char* data = new unsigned char[output.size];
    rawtile.capacity = output.size;

    // Copy our output data into our rawtile buffer
    memcpy( data, output.bytes, output.size );
    rawtile.dataLength = output.size;

    // Assign our buffer
    if( rawtile.memoryManaged ) delete[] (unsigned char*) rawtile.data;

    // Delete no longer needed memory
    rawtile.data = data;
    WebPDataClear( &output );
  }
}
