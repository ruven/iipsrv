/*  Generic compressor class - extended by JPEG and PNG Compressor classes

    Copyright (C) 2017-2023 Ruven Pillay

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


#ifndef _COMPRESSOR_H
#define _COMPRESSOR_H



#include <string>
#include "RawTile.h"



/// Base class for IIP output images
class Compressor {

 protected:

  /// Quality or compression level for all image types
  int Q;

  /// Whether compression level is default or has been set manually
  bool default_quality;

  /// Pointer to the header data for the output image
  unsigned char *header;

  /// Size of the header data
  unsigned int header_size;

  /// Physical resolution for X and Y directions
  float dpi_x, dpi_y;

  /// Resolution units
  /** Units can be 0 for unknown, 1 for dots/inch or 2 for dots/cm */
  int dpi_units;

  /// ICC Profile
  std::string icc;

  /// XMP metadata
  std::string xmp;

  /// Write ICC profile
  virtual void writeICCProfile() {};

  /// Write XMP metadata
  virtual void writeXMPMetadata() {};


 public:

  /// Constructor
  /** @param compressionLevel default compression level for codec */
  Compressor( int compressionLevel ) :
    Q( compressionLevel ),
    default_quality( true ),
    header( NULL ),
    header_size( 0 ),
    dpi_x( 0 ),
    dpi_y( 0 ),
    dpi_units( 0 ) {};


  virtual ~Compressor() {};


  /// Get the current quality level
  inline int getQuality() const { return Q; }


  /// Check whether we are using the default or whether user has requested a specific quality level
  inline bool defaultQuality() const { return default_quality; }


  /// Set the physical output resolution
  inline void setResolution( float x, float y, int units ){ dpi_x = x; dpi_y = y; dpi_units = units; };


  /// Set the ICC profile
  /** @param profile ICC profile string */
  inline void setICCProfile( const std::string& profile ){ icc = profile; }


  /// Set XMP metadata
  /** @param x XMP metadata string */
  inline void setXMPMetadata( const std::string& x ){ xmp = x; }


  /// Return the image header size
  /** @return header size in bytes */
  virtual unsigned int getHeaderSize() const { return 0; };


  /// Return a pointer to the image header itself
  /** @return binary header blob */
  virtual unsigned char* getHeader() { return NULL; };


  /// Initialise strip based compression
  /** If we are doing a strip based encoding, we need to first initialise
      with InitCompression, then compress a single strip at a time using
      CompressStrip and finally clean up using Finish
      @param rawtile tile containing the image to be compressed
      @param strip_height pixel height of the strip we want to compress
  */
  virtual void InitCompression( const RawTile& rawtile, unsigned int strip_height ) {};


  /// Compress a strip of image data
  /** @param s source image data
      @param o output buffer
      @param tile_height pixel height of the tile we are compressing
      @return number of bytes used for strip
  */
  virtual unsigned int CompressStrip( unsigned char* s, unsigned char* o, unsigned int tile_height ) { return 0; };


  /// Finish the strip based compression and free memory
  /** @param output output buffer
      @return size of output generated in bytes
  */
  virtual unsigned int Finish( unsigned char* output ) { return 0; };


  /// Compress an entire buffer of image data at once in one command
  /** @param t tile of image data
      @return number of bytes used
   */
  virtual unsigned int Compress( RawTile& t ) { return 0; };


  /// Add metadata to the image header
  /** @param m metadata */
  virtual void addXMPMetadata( const std::string& m ) {};


  /// Get mime type
  /** @return IANA mime type as const char* */
  virtual const char* getMimeType() const { return "image/example"; };


  /// Get file suffix
  /** @return suffix as const char* */
  virtual const char* getSuffix() const { return "img"; };


  /// Get compression type
  /** @return compressionType */
  virtual ImageEncoding getImageEncoding() const { return ImageEncoding::RAW; };

};

#endif
