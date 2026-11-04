/*  IIP JPEG-XL Compressor Class:
    Handles alpha channels, ICC profiles, XMP and EXIF metadata

    Copyright (C) 2026 Ruven Pillay

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


#include "JXLCompressor.h"
#include <cstdint>
#include <climits>
#include <new>


using namespace std;


/// Constructor
JXLCompressor::JXLCompressor( int quality )
  : Compressor(quality), jxl(NULL), settings(NULL), runner(NULL), chunk_size(0), current_chunk(0) {

  try {

    // Create re-usable global encoder object
    jxl = JxlEncoderCreate( NULL );
    if( jxl == NULL ) throw string( "JXLCompressor :: Unable to create encoder" );

    // Setup parallelized threaded encoding
    runner = JxlThreadParallelRunnerCreate( NULL, JxlThreadParallelRunnerDefaultNumWorkerThreads() );
    if( runner == NULL ) throw string( "JXLCompressor :: Unable to create parallel runner" );

    if( JxlEncoderSetParallelRunner( jxl, JxlThreadParallelRunner, runner ) != JXL_ENC_SUCCESS ){
      throw string( "JXLCompressor :: Unable to set parallel runner" );
    }

    settings = JxlEncoderFrameSettingsCreate( jxl, NULL );
    if( settings == NULL ) throw string( "JXLCompressor :: Error creating frame settings" );

    // Set parameters for fastest encoding
    if( JxlEncoderFrameSettingsSetOption( settings, JXL_ENC_FRAME_SETTING_EFFORT, 1 ) != JXL_ENC_SUCCESS )
      throw string( "JXLCompressor :: Unable to set effort" );
    if( JxlEncoderFrameSettingsSetOption( settings, JXL_ENC_FRAME_SETTING_RESPONSIVE, 0 ) != JXL_ENC_SUCCESS )
      throw string( "JXLCompressor :: Unable to set responsive option" );
    if( JxlEncoderFrameSettingsSetOption( settings, JXL_ENC_FRAME_SETTING_BROTLI_EFFORT, 0 ) != JXL_ENC_SUCCESS )
      throw string( "JXLCompressor :: Unable to set brotli effort" );
    if( JxlEncoderFrameSettingsSetOption( settings, JXL_ENC_FRAME_SETTING_BUFFERING, 1 ) != JXL_ENC_SUCCESS )
      throw string( "JXLCompressor :: Unable to set buffering option" );

  }
  catch( const std::string& ){
    // Clean up partially-created resources before rethrowing the original error
    if( jxl ) JxlEncoderDestroy( jxl );
    if( runner ) JxlThreadParallelRunnerDestroy( runner );
    throw;
  }

}



/// Destructor
JXLCompressor::~JXLCompressor(){
  // Destroy the encoder (which also frees the frame settings) before the runner
  // it uses, so the runner is never accessed after being destroyed
  JxlEncoderDestroy( jxl );
  JxlThreadParallelRunnerDestroy( runner );
}



void JXLCompressor::configure( const RawTile& rawtile ){

  if( rawtile.channels < 1 || rawtile.channels > 4 ){
    throw string( "JXLCompressor :: Only 1 to 4 channels are supported" );
  }

  JxlBasicInfo info;

  // Set some basic information
  JxlEncoderInitBasicInfo( &info );
  info.xsize = rawtile.width;
  info.ysize = rawtile.height;
  info.bits_per_sample = rawtile.bpc;
  info.exponent_bits_per_sample = ( rawtile.bpc == 32 && rawtile.sampleType == SampleType::FLOATINGPOINT ) ? 8 : 0;
  info.num_color_channels     = (rawtile.channels >= 3) ? 3 : 1;
  info.alpha_bits             = (rawtile.channels == 4 || rawtile.channels == 2) ? rawtile.bpc : 0;
  info.alpha_exponent_bits    = (rawtile.bpc == 32 && rawtile.sampleType == SampleType::FLOATINGPOINT && (rawtile.channels == 4 || rawtile.channels == 2)) ? 8 : 0;
  info.num_extra_channels     = (rawtile.channels == 4 || rawtile.channels == 2) ? 1 : 0;
  // uses_original_profile must be true for lossless and whenever an ICC profile
  // is embedded, so the encoder treats it as the authoritative colour description
  info.uses_original_profile  = (this->Q == -1 || (embedICC && !icc.empty())) ? JXL_TRUE : JXL_FALSE;

  if( JxlEncoderSetBasicInfo( jxl, &info) != JXL_ENC_SUCCESS ){
    throw string( "JXLCompressor :: JxlEncoderSetBasicInfo() error" );
  }

  // The API requires JxlEncoderSetExtraChannelInfo() to be called for every extra
  // channel (including alpha) between JxlEncoderSetBasicInfo and JxlEncoderAddImageFrame
  if( rawtile.channels == 2 || rawtile.channels == 4 ){
    JxlExtraChannelInfo ec_info;
    JxlEncoderInitExtraChannelInfo( JXL_CHANNEL_ALPHA, &ec_info );
    ec_info.bits_per_sample          = rawtile.bpc;
    ec_info.exponent_bits_per_sample = info.exponent_bits_per_sample;
    if( JxlEncoderSetExtraChannelInfo( jxl, 0, &ec_info ) != JXL_ENC_SUCCESS ){
      throw string( "JXLCompressor :: JxlEncoderSetExtraChannelInfo() error" );
    }
  }


  // Define the image pixel format
  JxlDataType sample_type;
  if( rawtile.bpc == 8 ) sample_type = JXL_TYPE_UINT8;
  else if( rawtile.bpc == 16 ) sample_type = JXL_TYPE_UINT16;
  else if( rawtile.bpc == 32 && rawtile.sampleType == SampleType::FLOATINGPOINT ) sample_type = JXL_TYPE_FLOAT;
  else throw string( "JXLCompressor :: Unsupported sample type and bit depth. JXL supports 8 and 16 bit integers and 32 bit floating point" );

  this->pixel_format.num_channels = rawtile.channels;
  this->pixel_format.data_type = sample_type;
  this->pixel_format.endianness = JXL_NATIVE_ENDIAN;
  this->pixel_format.align = 0;                        // align (0 = tightly packed rows)


  // Set lossless encoding if requested
  if( this->Q == -1 ){
    if( JxlEncoderSetFrameLossless( this->settings, JXL_TRUE ) != JXL_ENC_SUCCESS ){
      throw string( "JXLCompressor :: JxlEncoderSetFrameLossless() error" );
    }
  }
  // Otherwise lossy encoding
  else {
    // Convert JPEG-style quality (0-100) to JPEG-XL distance measure where 0.0 = visually lossless and max compression = 25.0
    float distance = JxlEncoderDistanceFromQuality( Q );
    // Set encoder distance value
    if( JxlEncoderSetFrameDistance( this->settings, distance ) != JXL_ENC_SUCCESS ){
      throw string( "JXLCompressor :: JxlEncoderSetFrameDistance() error" );
    }
  }


  // Set color space
  if( embedICC && !icc.empty() ){
    writeICCProfile();
  }
  else{
    // If no ICC profile or embedding disabled, set to sRGB or grayscale
    JxlColorEncoding color_encoding;
    JXL_BOOL is_gray = (rawtile.channels < 3) ? JXL_TRUE : JXL_FALSE;
    JxlColorEncodingSetToSRGB( &color_encoding, is_gray );
    if( JxlEncoderSetColorEncoding( jxl, &color_encoding ) != JXL_ENC_SUCCESS ){
      throw string( "JXLCompressor :: Error setting color encoding" );
    }
  }


  // Add XMP and EXIF metadata
  bool has_exif = embedEXIF && !exif.empty();
  bool has_xmp = embedXMP && !xmp.empty();
  if( has_exif || has_xmp ){
    if( JxlEncoderUseBoxes( jxl ) != JXL_ENC_SUCCESS ) throw string( "JXLCompressor :: Error enabling metadata boxes" );
    if( has_exif ) writeExifMetadata();
    if( has_xmp ) writeXMPMetadata();
    JxlEncoderCloseBoxes( jxl );  // Returns void
  }

}



/// Encode a single frame and drain the complete compressed output
/** JXL encodes the whole image as a single frame (it is not a line or strip-based
    codec like JPEG or PNG), so we feed the entire input in one go and then drain
    the full compressed codestream into a buffer. The input buffer is not modified.
 */
unsigned char* JXLCompressor::encode( const RawTile& rawtile, size_t& dataLength, size_t& capacity ){

  // Validate input before reading from the buffer to avoid out-of-bounds access
  if( rawtile.data == NULL ) throw string( "JXLCompressor :: NULL input data" );
  if( rawtile.width == 0 || rawtile.height == 0 ) throw string( "JXLCompressor :: Invalid image dimensions" );

  // Total byte size of input pixel data (overflow-checked)
  unsigned int bytes_per_pixel = (unsigned int) rawtile.channels * (rawtile.bpc / 8);
  if( bytes_per_pixel == 0 ) throw string( "JXLCompressor :: Invalid bit depth" );
  size_t bytes = (size_t) rawtile.width * (size_t) rawtile.height;
  if( bytes > SIZE_MAX / bytes_per_pixel ) throw string( "JXLCompressor :: Input too large" );
  bytes *= bytes_per_pixel;

  // Make sure we don't read beyond the valid data
  if( bytes > rawtile.dataLength ) throw string( "JXLCompressor :: Input data length smaller than image dimensions" );

  // Setup and configure encoder
  this->configure( rawtile );

  // Add image data
  if( JxlEncoderAddImageFrame( settings, &this->pixel_format, (const void*) rawtile.data, bytes ) != JXL_ENC_SUCCESS ){
    throw string( "JXLCompressor :: JxlEncoderAddImageFrame() error" );
  }

  // Signal that there is no more input. For a single-frame image this is called immediately after adding the frame
  JxlEncoderCloseInput( jxl );

  // Drain the complete compressed frame from the encoder. Use a temporary dynamic buffer,
  // then allocate exact size to avoid heap memory waste in cache
  size_t buf_capacity = 65536;
  if( buf_capacity < bytes / 2 ) buf_capacity = bytes / 2;
  unsigned char* temp_buffer = (unsigned char*) malloc( buf_capacity );
  if( !temp_buffer ) throw string( "JXLCompressor :: Out of memory" );

  unsigned char* next_out  = temp_buffer;
  size_t avail_out = buf_capacity;
  JxlEncoderStatus status = JXL_ENC_NEED_MORE_OUTPUT;

  while( status == JXL_ENC_NEED_MORE_OUTPUT ){

    status = JxlEncoderProcessOutput( jxl, &next_out, &avail_out );

    if( status == JXL_ENC_NEED_MORE_OUTPUT ){
      size_t offset = next_out - temp_buffer;
      size_t new_capacity = buf_capacity + buf_capacity / 2;
      unsigned char* new_buf = (unsigned char*) realloc( temp_buffer, new_capacity );
      if( !new_buf ){
        free( temp_buffer );
        throw string( "JXLCompressor :: Out of memory reallocating output buffer" );
      }
      temp_buffer = new_buf;
      buf_capacity = new_capacity;
      next_out   = temp_buffer + offset;
      avail_out  = buf_capacity - offset;
    }
  }

  if( status != JXL_ENC_SUCCESS ){
    free( temp_buffer );
    throw string( "JXLCompressor :: JxlEncoderProcessOutput failed: status " + std::to_string(status) );
  }

  dataLength = next_out - temp_buffer;

  // Allocate exact output buffer
  unsigned char* buffer = NULL;
  try {
    buffer = new unsigned char[dataLength];
  }
  catch( const std::bad_alloc& ){
    free( temp_buffer );
    throw string( "JXLCompressor :: Out of memory allocating output buffer" );
  }
  memcpy( buffer, temp_buffer, dataLength );
  free( temp_buffer );

  capacity = dataLength;

  return buffer;
}



/// Initialize chunk-based encoding for the CVT handler
void JXLCompressor::InitCompression( const RawTile& rawtile, unsigned int strip_height ){

  // Free any previously compressed output buffer from an earlier request
  // Note: RawTile::deallocate() does not clear the data pointer, so set it
  // to NULL ourselves to avoid a dangling pointer (and double-free) if
  // encode() throws before we reassign tile.data below
  if( tile.data != NULL ) tile.deallocate( tile.data );
  tile.data = NULL;

  // JXL encodes the whole region as a single frame, so compress the entire buffer in one go
  size_t dataLength, capacity;
  unsigned char* buffer = this->encode( rawtile, dataLength, capacity );

  // Store the compressed data for chunked output. Set memoryManaged so the buffer
  // is freed when the local tile object is destroyed
  tile.width = rawtile.width;
  tile.height = rawtile.height;
  tile.channels = rawtile.channels;
  tile.bpc = 8;
  tile.sampleType = SampleType::FIXEDPOINT;
  tile.data = buffer;
  tile.capacity = capacity;
  tile.dataLength = dataLength;
  tile.memoryManaged = 1;

  current_chunk = 0;
}



// JXL cannot handle line or region-based encoding, so simulate strip-based output using byte chunks
unsigned int JXLCompressor::CompressStrip( unsigned char* source, unsigned char* output, unsigned int tile_height ){

  // Initialize our chunk size only once at the start of the sequence
  if( current_chunk == 0 && tile.height > 0 ){
    chunk_size = ( ( (uint64_t) tile.dataLength * tile_height ) + (tile_height / 2) ) / tile.height;
  }

  // Make sure we don't over-run our allocated memory
  if( (current_chunk + chunk_size) > tile.dataLength ) chunk_size = tile.dataLength - current_chunk;

  // Copy our chunk of data to the given output buffer
  if( chunk_size > 0 ){
    unsigned char* data = (unsigned char*) tile.data;
    memcpy( output, &data[current_chunk], chunk_size );
    current_chunk += chunk_size;
  }

  return chunk_size;
}



unsigned int JXLCompressor::Finish( unsigned char* output ){

  // Output any remaining bytes
  if( current_chunk < tile.dataLength ){
    unsigned char* data = (unsigned char*) tile.data;
    chunk_size = tile.dataLength - current_chunk;
    memcpy( output, &data[current_chunk], chunk_size );
    current_chunk += chunk_size;
    return chunk_size;
  }

  return 0;
}



/// Oneshot encoding of a single tile of data
unsigned int JXLCompressor::Compress( RawTile& rawtile ){

  // Encode the entire tile into a newly allocated buffer without touching the input
  size_t dataLength, capacity;
  unsigned char* buffer = this->encode( rawtile, dataLength, capacity );

  // Swap encoded buffer with Rawtile buffer
  // Only free the original input buffer if we own it, otherwise
  // leave the caller's externally-managed buffer intact
  if( rawtile.memoryManaged ) rawtile.deallocate( rawtile.data );

  rawtile.capacity = capacity;
  rawtile.dataLength = dataLength;
  rawtile.data = buffer;
  rawtile.bpc = 8;
  rawtile.sampleType = SampleType::FIXEDPOINT;
  rawtile.memoryManaged = 1;

  rawtile.quality = this->Q;
  rawtile.compressionType = ImageEncoding::JXL;

  return rawtile.dataLength;
}



/// Write ICC profile
void JXLCompressor::writeICCProfile()
{
  if( !embedICC || icc.empty() ) return;

  size_t len = icc.size();
  if( JxlEncoderSetICCProfile( jxl, (const uint8_t*) icc.c_str(), len ) != JXL_ENC_SUCCESS ){
    throw string( "JXLCompressor :: Error adding ICC profile" );
  }
}



/// Write XMP metadata
void JXLCompressor::writeXMPMetadata()
{
  if( !embedXMP || xmp.empty() ) return;

  size_t len = xmp.size();
  if( JxlEncoderAddBox( jxl, "xml ", (const uint8_t*) xmp.c_str(), len, JXL_FALSE ) != JXL_ENC_SUCCESS ){
    throw string( "JXLCompressor :: Error adding XMP metadata" );
  }
}



/// Write EXIF metadata
void JXLCompressor::writeExifMetadata()
{
  if( !embedEXIF || exif.empty() ) return;

  // In JPEG XL container format, the Exif box content must be prefixed with a 4-byte
  // TIFF header offset (big-endian uint32). A value of 0 means the TIFF header
  // starts immediately after these 4 bytes.
  std::string exif_box( 4, '\0' );
  exif_box.append( exif );

  if( JxlEncoderAddBox( jxl, "Exif", (const uint8_t*) exif_box.data(), exif_box.size(), JXL_FALSE ) != JXL_ENC_SUCCESS ){
    throw string( "JXLCompressor :: Error adding EXIF metadata" );
  }
}
