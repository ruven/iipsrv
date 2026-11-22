/*  WebP Compressor Class:
    Handles alpha channels, ICC profiles and XMP metadata

    Copyright (C) 2022-2026 Ruven Pillay

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
  void writeICCProfile() override;

  /// Write XMP metadata
  void writeXMPMetadata() override;

  /// Write EXIF metadata
  void writeExifMetadata() override;


 public:

  /// Constructor
  /** @param compressionLevel WebP compression level (range 0-100)
   */
  WebPCompressor( int compressionLevel ) : Compressor(compressionLevel) {

    // Initialize our configuration structure with default values
    WebPConfigInit( &config );
    config.method = 0;         // Fastest encoding
    config.thread_level = 1;   // Enable threading

    // Update our WebP config structure depending on whether lossless or lossy compression requested
    if( compressionLevel == -1 ){
      // -1 indicates lossless
      config.lossless = 1;
      config.quality = 0;      // Zero means fastest for lossless
    }
    // WebP's lossy quality range is 0-100
    else config.quality = this->Q;

    // Create our muxer object
    mux = WebPMuxNew();

  };


  ~WebPCompressor() override {
    // Delete our muxer object
    WebPMuxDelete( mux );
  };


  /// Initialize strip based compression
  /** For strip based encoding, we need to first initialize with InitCompression,
      then compress a single strip at a time using CompressStrip and finally clean up using Finish
      @param rawtile RawTile object containing the image to be compressed
      @param strip_height height in pixels of the strip we want to compress
   */
  void InitCompression( const RawTile& rawtile, unsigned int strip_height ) override;


  /// Compress a strip of image data
  /** @param source source image data
      @param tile_height pixel height of the tile we are compressing
      @param output output_buffer
      @return size of compressed strip
   */
  unsigned int CompressStrip( unsigned char* source, unsigned char* output, unsigned int tile_height ) override;


  /// Finish the strip based compression and free memory
  /** @param output Output buffer
      @return size of output generated
   */
  unsigned int Finish( unsigned char* output ) override;


  /// Compress an entire buffer of image data at once in one command
  /** @param t tile of image data
      @return size of compressed data
   */
  unsigned int Compress( RawTile& t ) override;


  /// Return the WebP mime type
  inline const char* getMimeType() const override { return "image/webp"; }

  /// Return the image filename suffix
  inline const char* getSuffix() const override { return "webp"; }

  /// Get compression type
  inline ImageEncoding getImageEncoding() const override { return ImageEncoding::WEBP; };


  /// Set the compression level
  /** @param quality compression level: 0-100 with (0 = highest compression, -1 = lossless)
   */
  inline void setQuality( int quality ) override {

    // Flag that user has manually changed quality level
    default_quality = false;

    // Lossless compression
    if( quality < 0 ){
      this->Q = -1;
      config.lossless = 1;
      config.quality = 0;      // Fastest for lossless
    }
    // Lossy: 0-100, clamped (0 is lossy minimum, not lossless)
    else {
      this->Q = (quality > 100) ? 100 : quality;
      config.lossless = 0;
      config.quality = this->Q;
    }

  }


  /// Determine whether encoder supports the given number of channels
  /** @param channels number of channels
      @return whether or not the channel count is supported
   */
  inline bool supportsChannels( unsigned int channels ) const override {
    return channels == 1 || channels == 3 || channels == 4;
  }


  /// Inject metadata into raw WebP bitstream
  /** @param r image tile
   */
  void injectMetadata( RawTile& r ) override;


};

#endif
