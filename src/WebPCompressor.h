/*  WebP Compressor Class:
    Handles alpha channels, ICC profiles and XMP metadata

    Copyright (C) 2022 Ruven Pillay

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


#ifndef _WEBPCOMPRESSOR_H
#define _WEBPCOMPRESSOR_H


#include "Compressor.h"
#include <webp/encode.h>
#include <webp/mux.h>



/// Wrapper class to WebP library: Handles 8 and 16 bit PNG as well as alpha transparency
class WebPCompressor : public Compressor {

 private:
 
  /// WebP structures
  WebPConfig config;
  WebPPicture pic;
  WebPMemoryWriter writer;
  WebPMux* mux;

  /// Data for simulated strip-based output
  RawTile tile;                   ///< Output data for strip-based output
  unsigned int chunk_size;        ///< Number of bytes to output per strip
  size_t current_chunk;           ///< Index of current byte


  /// Write ICC profile
  void writeICCProfile();

  /// Write XMP metadata
  void writeXMPMetadata();


 public:

  /// Constructor
  WebPCompressor( int compressionLevel ){

    header = NULL;
    header_size = 0;

    // WebP compression range is 0-100
    this->Q = compressionLevel;

    // Initialize our configuration structure with default values
    WebPConfigInit( &config );
    config.method = 0;         // Fastest encoding
    config.thread_level = 1;   // Enable threading
    config.quality = this->Q;  // WebP's quality range is 0-100

    // Create our muxer object
    mux = WebPMuxNew();

  };


  ~WebPCompressor(){
    // Delete our muxer object
    WebPMuxDelete( mux );
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


  /// Return the WebP header size
  inline unsigned int getHeaderSize() const { return header_size; }

  /// Return a pointer to the header itself
  inline unsigned char* getHeader() { return header; }

  /// Return the WebP mime type
  inline const char* getMimeType() const { return "image/webp"; }

  /// Return the image filename suffix
  inline const char* getSuffix() const { return "webp"; }

  /// Get compression type
  inline CompressionType getCompressionType() const { return WEBP; };


  /// Get the current compression level
  /** @return compresson level */
  inline int getQuality() const { return Q; }


  /// Set the compression level
  /** @param compression level: 0-100 with (0 = highest compression)
   */
  inline void setQuality( int quality ){

    // WebP quality ranges from 0 (best compression - smallest size) to 100 (worst compression - largest size)
    this->Q = quality;

    // WebP compression level
    if( Q < 0 ) Q = 0;
    else if( Q > 100 ) Q = 100;

    // Update our WebP config structure
    config.quality = Q;
  }


};

#endif
