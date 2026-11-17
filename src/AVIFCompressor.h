/*  AVIF Compressor Class:
    Handles alpha channels, ICC profiles and XMP metadata

    Copyright (C) 2024-2026 Ruven Pillay

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


#ifndef _AVIFCOMPRESSOR_H
#define _AVIFCOMPRESSOR_H


#include "Compressor.h"
#include <avif/avif.h>



/// Wrapper class to AVIF library: Handles 8 and 16 bit input, alpha channels, ICC profiles and XMP metadata
class AVIFCompressor : public Compressor {

 private:
 
  /// AVIF structures
  avifEncoder *encoder;
  avifImage *avif;
  avifCodecChoice codec;


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
  /** @param quality AVIF compression level (range 0-100)
   */
  AVIFCompressor( int quality ) : Compressor(quality), codec(AVIF_CODEC_CHOICE_AUTO) {};

  /// Destructor
  ~AVIFCompressor() override {};


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


  /// Return the AVIF mime type
  inline const char* getMimeType() const override { return "image/avif"; }

  /// Return the image filename suffix
  inline const char* getSuffix() const override { return "avif"; }

  /// Get compression type
  inline ImageEncoding getImageEncoding() const override { return ImageEncoding::AVIF; };


  /// Determine whether encoder supports the given bit depth and sample type
  /** @param bpc bit depth
      @param type sample type (FIXEDPOINT or FLOATINGPOINT)
      @return whether or not bit depth and sample type is supported
  */
  bool supportsBitDepth( unsigned int bpc, SampleType type = SampleType::FIXEDPOINT ) const override {
    return type == SampleType::FIXEDPOINT && ( bpc == 8 || bpc == 16 );
  }


  /// Set the compression level
  /** @param quality compression level: 0-100 with (0 = highest compression). -1 = lossless
   */
  inline void setQuality( int quality ) override{

    // Flag that user has manually changed quality level
    default_quality = false;

    // AVIF quality ranges from 0 (best compression - smallest size) to 100 (worst compression - largest size)
    this->Q = quality;

    // AVIF compression level
    if( Q < -1 ) Q = -1;         // Lossless
    else if( Q > 100 ) Q = 100;

  }


  /// Determine whether encoder supports the given number of channels
  /** @param channels number of channels
      @return whether or not the channel count is supported
   */
  inline bool supportsChannels( unsigned int channels ) const override {
    return channels == 1 || channels == 3 || channels == 4;
  }


  /// Set codec for use during encoding - note that not all may be enabled in libavif
  /** @param codec IIPImage's codec option code (0=auto,1=aom,2=rav1e,3=svt)
   */
  inline void setCodec( unsigned int codec )
  {
    this->codec = AVIFCompressor::getCodecChoice( codec );
  }


  /// Static function: Convert from our option native system to libavif's codec choices
  /**
     @param codec IIPImage's codec option code (0=auto,1=aom,2=rav1e,3=svt)
     @return libavif codec choice enum
   */
  inline static avifCodecChoice getCodecChoice( unsigned int codec )
  {
    if( codec == 1 ) return AVIF_CODEC_CHOICE_AOM;
    else if( codec == 2 ) return AVIF_CODEC_CHOICE_RAV1E;
    else if( codec == 3 ) return AVIF_CODEC_CHOICE_SVT;
    else return AVIF_CODEC_CHOICE_AUTO;
  }


  /// Static function: Get codec name from IIPImage codec option code
  /**
     @param codec IIPImage's codec option code (0=auto,1=aom,2=rav1e,3=svt)
     @return code name
   */
  inline static const char* getCodecName( unsigned int codec )
  {
    avifCodecChoice choice = AVIFCompressor::getCodecChoice( codec );
    const char* name = avifCodecName( choice, AVIF_CODEC_FLAG_CAN_ENCODE );
    if( name == NULL ) return "unsupported codec - will not be able to encode to avif";
    else return name;
  }

};

#endif
