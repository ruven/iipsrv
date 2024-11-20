/*  IIP TIFF Compressor Class:
    Handles alpha channels, 8 or 16 bit data, ICC profiles and XMP metadata

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


#ifndef _TIFFCOMPRESSOR_H
#define _TIFFCOMPRESSOR_H


#include "Compressor.h"
#include <tiffio.h>


/// Structure to handle memory-based TIFF writing
typedef struct {
  unsigned char *buffer;   /// Data buffer
  toff_t current;          /// Current byte position within stream
  toff_t end;              /// Last written byte
  toff_t capacity;         /// Allocated buffer size
} tiff_mem;

typedef tiff_mem* tiff_mem_ptr;


class TIFFCompressor: public Compressor {

 private:
  unsigned int width, height, channels, bpc;
  int tiff_compression;
  tiff_mem dest;
  TIFF *tiff;

  unsigned int chunk_size;        ///< Number of bytes to output per strip
  size_t current_chunk;           ///< Index of current byte

  /// TIFF compression type
  uint16_t compression;
  
  /// Write metadata
  void writeMetadata();

  /// Write DPI information
  void writeResolution();

  /// Write ICC profile
  void writeICCProfile();

  /// Write XMP metadata
  void writeXMPMetadata();

  /// Configure encoding parameters
  void configure( const RawTile& );


 public:

  /// Constructor
  /** @param enc compression type
      @param quality compression level
  */
  TIFFCompressor( int compression, int quality ) : Compressor(quality), tiff(NULL)
  {
    this->setCompression( compression );

    // Limit our quality level to the max allowed for Deflate and ZStandard
    if( compression == 2 && Q > 9 ) Q = 9;
    else if( compression == 5 && Q > 19 ) Q = 19;
  };



  /// Set compression type: 0: None, 1: LZW, 2: Deflate, 3: JPEG, 4: WebP
  /** @param compression compression type */
  inline void setCompression( int compression )
  {
    if      ( compression == 1 ) this->compression = COMPRESSION_LZW;
    else if ( compression == 2 ) this->compression = COMPRESSION_ADOBE_DEFLATE;
    else if ( compression == 3 ) this->compression = COMPRESSION_JPEG;
    else if ( compression == 4 ) this->compression = COMPRESSION_WEBP;
    else if ( compression == 5 ) this->compression = COMPRESSION_ZSTD;
    else                         this->compression = COMPRESSION_NONE;
  };


  /// Set the compression level
  /** @param quality compression level for JPEG or WebP (both 0-100)
   */
  inline void setQuality( int quality )
  {
    // Flag that user has manually changed quality level
    default_quality = false;

    Q = quality;

    // Set max depending on compression type    
    int max;
    if( compression == 2 ) max = 9;        // Max for deflate is 9
    else if( compression == 5 ) max = 19;  // Max for zstd is 19
    else max = 100;                        // Max for JPEG or WebP is 100

    // Limit quality range
    if     ( Q < 0 )   Q = 0;
    else if( Q > max ) Q = max;
  };


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


  /// Return the TIFF mime type
  inline const char* getMimeType() const { return "image/tiff"; };

  /// Return the image filename suffix
  inline const char* getSuffix() const { return "tif"; };

  /// Get compression type
  inline ImageEncoding getImageEncoding() const { return ImageEncoding::TIFF; };


  /// Static utility function to get compression in human-readable form
  /** @param comp compression code
      @return human-readable compression name
  */
  static inline std::string getCompressionName( int code )
  {
    std::string name;
    if     ( code == 1 ) name = "LZW";
    else if( code == 2 ) name = "Deflate";
    else if( code == 3 ) name = "JPEG";
    else if( code == 4 ) name = "WebP";
    else if( code == 5 ) name = "ZSTD";
    else                 name = "None";
    return name;
  };

};

#endif
