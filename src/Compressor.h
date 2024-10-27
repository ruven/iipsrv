/*  Generic compressor class - extended by JPEG and PNG Compressor classes

    Copyright (C) 2017-2024 Ruven Pillay

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



#include "RawTile.h"
#include <map>


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

  /// Metadata
  std::map <const std::string, const std::string> metadata;

  /// ICC Profile
  bool embedICC;
  std::string icc;

  /// XMP metadata
  bool embedXMP;
  std::string xmp;

  /// Write metadata
  virtual void writeMetadata() {};

  /// Write DPI
  virtual void writeResolution() {};

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
    dpi_units( 0 ),
    embedICC( false ),
    embedXMP( false ) {};


  virtual ~Compressor() {};


  /// Return the image header size
  /** @return header size in bytes */
  unsigned int getHeaderSize() const { return header_size; };


  /// Return a pointer to the image header itself
  /** @return binary header blob */
  unsigned char* getHeader() { return header; };


  /// Get the current quality level
  inline int getQuality() const { return Q; }


  /// Check whether we are using the default or whether user has requested a specific quality level
  inline bool defaultQuality() const { return default_quality; }


  /// Set the physical output resolution
  inline void setResolution( float x, float y, int units ){ dpi_x = x; dpi_y = y; dpi_units = units; };


  /// Embed ICC profile
  /** @param embed Whether ICC profile should be embedded */
  inline void embedICCProfile( const bool embed ){ this->embedICC = embed; }


  /// Embed XMP metadata
  /** @param embed Whether XMP metadata should be embedded */
  inline void embedXMPMetadata( const bool embed ){ this->embedXMP = embed; }


  /// Set general metadata
  /** @param metadata Metadata list */
  inline void setMetadata( const std::map <const std::string, const std::string>& metadata )
  {
    this->metadata = std::map <const std::string, const std::string>( metadata );

    // Extract ICC profile if it exists and remove from list
    std::map<const std::string, const std::string> :: const_iterator it;
    it = this->metadata.find("icc");
    if( it != this->metadata.end() ){
      icc = it->second;
      this->metadata.erase( it );
    }

    // Extract XMP chunk if it exists and remove from list
    it = this->metadata.find("xmp");
    if( it != this->metadata.end() ){
      xmp = it->second;
      this->metadata.erase( it );
    }
  };



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


  /// Get mime type
  /** @return IANA mime type as const char* */
  virtual const char* getMimeType() const { return "image/example"; };


  /// Get file suffix
  /** @return suffix as const char* */
  virtual const char* getSuffix() const { return "img"; };


  /// Get compression type
  /** @return compressionType */
  virtual ImageEncoding getImageEncoding() const { return ImageEncoding::RAW; };


  /// Inject metadata into raw bitstream
  /** @param  t image tile containing raw bitstream */
  virtual void injectMetadata( RawTile& t ) {};

};

#endif
