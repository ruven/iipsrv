/*  IIP PNG Compressor Class:
    Handles alpha channels, 8 or 16 bit data, ICC profiles and XMP metadata

    Copyright (C) 2012-2024 Ruven Pillay

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


#ifndef _PNGCOMPRESSOR_H
#define _PNGCOMPRESSOR_H


#include "Compressor.h"
#include <png.h>


// Define ourselves a set of fast filters if necessary
#ifndef PNG_FAST_FILTERS // libpng < 1.6
#define PNG_FAST_FILTERS ( PNG_FILTER_NONE | PNG_FILTER_SUB | PNG_FILTER_UP )
#endif



/// Expanded data destination object for buffered output used by PNG library
typedef struct {

  png_structp png_ptr;               ///< png data pointer
  png_infop info_ptr;                ///< png info pointer

  unsigned char* output;             ///< output buffer pointer
  size_t output_size;                ///< size of output buffer
  size_t written;                    ///< number of bytes written to buffer
  unsigned int strip_height;         ///< strip height: used for stream-based encoding
  unsigned int bytes_per_pixel;      ///< bytes per pixel (1 for 8 bit and 2 for 16 bit images)

} png_destination_mgr;

typedef png_destination_mgr *png_destination_ptr;



/// Wrapper class to PNG library: Handles 8 and 16 bit PNG as well as alpha transparency
class PNGCompressor : public Compressor {

 private:

  /// the width, height and number of channels per sample for the image
  unsigned int width;         ///< the width of the image
  unsigned int height;        ///< the height of the image
  png_uint_32 channels;       ///< the channels per sample for the image

  png_destination_mgr dest;   ///< destination data structure

  // The Compressor class Q parameter stores the zlib compression level (0-9)
  int filterType;             ///< PNG compression filter type - see png.h
  
  /// Write metadata
  void writeMetadata();

  /// Write DPI
  void writeResolution();

  /// Write ICC profile
  void writeICCProfile();

  /// Write XMP metadata
  void writeXMPMetadata();

  
 public:

  /// Constructor
  /** @param compressionLevel PNG compression level (zlib range is 0-9)
   */
  PNGCompressor( int compressionLevel ) : Compressor(compressionLevel) {

    dest.png_ptr = NULL;
    dest.info_ptr = NULL;

    // Buffer for the data written by PNG library
    dest.output_size = 0;

    width = 0;
    height = 0;
    channels = 0;

    // Filters are an optional pre-processing step before Deflate compression
    //  - set this to the fastest set of filters
    filterType = PNG_FAST_FILTERS;

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


  /// Return the PNG mime type
  inline const char* getMimeType() const { return "image/png"; }

  /// Return the image filename suffix
  inline const char* getSuffix() const { return "png"; }

  /// Get compression type
  inline ImageEncoding getImageEncoding() const { return ImageEncoding::PNG; };


  /// Set the compression level
  /** @param quality Deflate compression level: 0-9 with (0 = no compression)
      Note that compression to 1 (Z_BEST_SPEED) results in a ~3x slowdown wrt to no compression
   */
  inline void setQuality( int quality ){

    // Flag that user has manually changed quality level
    default_quality = false;

    // Deflate compression level
    if( quality < 0 ) Q = 0;
    else if( quality > 9 ) Q = 9;
    else Q = quality;
  }


};

#endif
