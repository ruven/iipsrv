/*  IIP PNG Compressor Class

    Copyright (C) 2012-2013 Ruven Pillay with extensions by Dave Beaudet

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

///* get internal access to png.h */

#include <cstdio>
#include <string>
#include <iostream>
#include <stdexcept>
#include <png.h>
#include "RawTile.h"
#include "Compressor.h"

using namespace std;

/// Expanded data destination object for buffered output used by PNG library

typedef struct {
  unsigned char* data;              /**< data buffer */
  size_t size;                      /**< size of data */
  size_t mx;                        /**< allocated size of data buffer - might not be used */
  png_structp png_ptr;              /**< png data pointer */
  png_infop info_ptr;               /**< png info pointer */
} png_destination_mgr;

typedef png_destination_mgr * png_destination_ptr;


/// Wrapper class to PNG library

class PNGCompressor : public Compressor {

private:

  /// the width, height and number of channels per sample for the image
  unsigned int width;        /**< the width of the image */
  unsigned int height;       /**< the height of the image */
  png_uint_32 channels;      /**< the channels per sample for the image */

  png_destination_mgr dest;  /**< destination data structure */

  int compressionLevel;      /**< png compression level - see zlib.h */
  int filterType;            /**< png compression filter type - see png.h */

  /// Set output buffer to the given byte pointer with preallocated size 
  void setOutputBuffer(unsigned char* output, unsigned int allocatedSize) {
    dest.data = output;
    dest.mx = allocatedSize;
    dest.size = 0;
  }

public:

  /// Constructor
  PNGCompressor( int compressionLevel, int filterType ) { 
    dest.png_ptr = NULL;
    dest.info_ptr = NULL;

    // buffer for the data written by PNG library
    dest.data = NULL; 
    dest.size = 0;
    dest.mx = 0;

    width = 0;
    height = 0;
    channels = 0;

    this->compressionLevel = compressionLevel;
    setQuality(filterType);
  };


  /// Initialise strip based compression
  /** If we are doing a strip based encoding, we need to first initialise
      with InitCompression, then compress a single strip at a time using
      CompressStrip and finally clean up using Finish
      @param rawtile tile containing the image to be compressed
      @param strip_height pixel height of the strip we want to compress
   */
  void InitCompression( const RawTile& rawtile, unsigned int strip_height ) throw (std::string) OVERRIDE;


  /// Compress a strip of image data
  /** @param s source image data
      @param tile_height pixel height of the tile we are compressing
      @param o output_buffer // unused with PNG
   */
  unsigned int CompressStrip( unsigned char* s, unsigned char* o, unsigned long olen, unsigned int tile_height ) throw (std::string) OVERRIDE;


  /// Finish the strip based compression and free memory
  /** @param output output buffer
      @return size of output generated
   */
  unsigned int Finish(unsigned char* output, unsigned long outputlen) throw (std::string) OVERRIDE;

  /// Compress an entire buffer of image data at once in one command
  /** @param t tile of image data */
  int Compress( RawTile& t ) throw (std::string) OVERRIDE;

  /// Add metadata to the JPEG header
  /** @param m metadata */
  void addXMPMetadata( const std::string& m ) OVERRIDE;

  /// get mime type for this compressor
  std::string getMimeType() OVERRIDE { 
    return "image/png"; 
  }

  /// Return the image header size - returns the header length if called at the right time
  unsigned int getHeaderSize() OVERRIDE { 
    return dest.size; 
  };

  /// Return a pointer to the image header itself - returns the header if called at the right time
  unsigned char* getHeader() OVERRIDE { 
    return (unsigned char*) dest.data; 
  };

  /// Dump any header data
  void finishHeader() OVERRIDE { 
    if ( dest.data != NULL )
      delete[] dest.data;
    dest.data = NULL;
    dest.size = 0;
    dest.mx = 0;
  };

  int getQuality( ) OVERRIDE {
    return filterType;
  }

  void setQuality( int quality ) OVERRIDE {
    filterType = quality;
    if ( quality != PNG_FILTER_SUB   && 
         quality != PNG_FILTER_UP    && 
         quality != PNG_FILTER_AVG   && 
         quality != PNG_FILTER_PAETH && 
         quality != PNG_ALL_FILTERS )
      filterType = PNG_FILTER_NONE;
  }


};

#endif
