/*  JPEG-XL Compressor Class:
    Handles alpha channels, ICC profiles and XMP metadata

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


#ifndef _JXLCOMPRESSOR_H
#define _JXLCOMPRESSOR_H


#include "Compressor.h"
#include <jxl/encode.h>
#include <jxl/thread_parallel_runner.h>


/// Wrapper class to JPEG-XL library: Handles 8 and 16 bit fixed point, 32 bit float and alpha channels
class JXLCompressor : public Compressor {

 private:

  /// JXL structures
  JxlEncoder *jxl;                    ///< Encoder
  JxlEncoderFrameSettings *settings;  ///< Encoding settings
  void *runner;                       ///< Thread runner for use on images larger than 256x256 tiles

  /// Data for simulated strip-based output
  RawTile tile;                       ///< Compressed output data for strip-based output
  uint64_t chunk_size;                ///< Number of bytes to output per strip
  uint64_t current_chunk;             ///< Index of current byte

  /// Write ICC profile
  void writeICCProfile();

  /// Write XMP metadata
  void writeXMPMetadata();

  /// Write EXIF metadata
  void writeExifMetadata();

  /// Configure encoding parameters
  void configure( const RawTile& );

  /// Pixel format descriptor derived from the RawTile during configure()
  JxlPixelFormat pixel_format;

  /// Encode a single frame and drain the complete compressed output
  /** JXL encodes the whole image as a single frame, so we feed all input at once
      and drain the full compressed codestream. The input buffer is not modified.
      @param rawtile image to encode
      @param dataLength set to the number of valid bytes in the returned buffer
      @param capacity set to the allocated size of the returned buffer
      @return newly allocated buffer containing the compressed frame (caller owns)
   */
  unsigned char* encode( const RawTile& rawtile, size_t& dataLength, size_t& capacity );


 public:

  /// Constructor
  /** @param quality JPEG-XL compression level (range 0-100, -1 for lossless)
   */
  JXLCompressor( int quality );


  /// Destructor
  ~JXLCompressor();


  /// Initialize strip based compression
  /** For strip based encoding, we need to first initialize with InitCompression,
      then compress a single strip at a time using CompressStrip and finally clean up using Finish
      @param rawtile RawTile object containing the image to be compressed
      @param strip_height height in pixels of the strip we want to compress
   */
  void InitCompression( const RawTile& rawtile, unsigned int strip_height );


  /// Compress a strip of image data
  /** @param source source image data
      @param tile_height pixel height of the tile we are compressing
      @param output output_buffer
      @return size of compressed strip
   */
  unsigned int CompressStrip( unsigned char* source, unsigned char* output, unsigned int tile_height );


  /// Finish the strip based compression and free memory
  /** @param output Output buffer
      @return size of output generated
   */
  unsigned int Finish( unsigned char* output );


  /// Compress an entire buffer of image data at once in one command
  /** @param t tile of image data
      @return size of compressed data
   */
  unsigned int Compress( RawTile& t );


  /// Return the JXL header size
  /** @return header size */
  inline unsigned int getHeaderSize() const { return header_size; }

  /// Return a pointer to the header itself
  /** @return pointer to header buffer */
  inline unsigned char* getHeader() { return header; }

  /// Return the JXL mime type
  /** @return mime type */
  inline const char* getMimeType() const { return "image/jxl"; }

  /// Return the image filename suffix
  /** @return file suffix */
  inline const char* getSuffix() const { return "jxl"; }

  /// Get compression type
  /** @return compression type */
  inline ImageEncoding getImageEncoding() const { return ImageEncoding::JXL; };


  /// Get the current compression level
  /** @return compresson level */
  inline int getQuality() const { return Q; }


  /// Set the compression level
  /** @param quality compression level: 0-100 with (0 = highest compression). -1 = lossless
   */
  inline void setQuality( int quality ){

    // Flag that user has manually changed quality level
    default_quality = false;

    // JXL quality ranges from 0 (best compression - smallest size) to 100 (worst compression - largest size)
    this->Q = quality;

    // JXL compression level
    if( Q < -1 ) Q = -1;         // Lossless
    else if( Q > 100 ) Q = 100;

  }

};

#endif
