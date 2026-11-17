/*  JPEG class wrapper to ijg libjpeg library

    Copyright (C) 2000-2026 Ruven Pillay.

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



#ifndef _JPEGCOMPRESSOR_H
#define _JPEGCOMPRESSOR_H


#include "Compressor.h"
#include <memory>


// Fix missing snprintf in Windows
#if _MSC_VER
#define snprintf _snprintf
#endif


extern "C"{
/* Undefine this to prevent compiler warning
 */
#undef HAVE_STDLIB_H
#include <jpeglib.h>
}



/// Expanded data destination object for buffered output used by IJG JPEG library
typedef struct {

  struct jpeg_destination_mgr pub;   ///< public fields

  unsigned char* source;             ///< output data buffer pointer
  size_t source_size;                ///< size of output buffer
  bool owns_source;                  ///< whether source is owned and may be grown
  size_t written;                    ///< number of bytes written to buffer
  unsigned int strip_height;         ///< used for stream-based encoding

} iip_destination_mgr;




/// Wrapper class to the IJG JPEG library
class JPEGCompressor: public Compressor{

 private:

  /// the width, height and number of channels per sample for the image
  unsigned int width, height, channels;

  /// JPEG library objects
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  iip_destination_mgr dest_mgr;
  iip_destination_mgr* dest;

  /// Reusable buffer for assembling JPEG marker payloads (max 65533 bytes)
  std::unique_ptr<unsigned char[]> marker_buffer;

  /// Return the shared marker payload buffer, allocating it on first use
  /** @return shared buffer for markers */
  unsigned char* markerBuffer(){
    if( !marker_buffer ) marker_buffer.reset( new unsigned char[65536] );
    return marker_buffer.get();
  }

  /// Size in bytes of the metadata segments that will be embedded
  /** @return total size in bytes of metadata */
  unsigned long metadataSize() const;

  /// Write DPI
  void writeResolution() override;

  /// Write ICC profile
  void writeICCProfile() override;

  /// Write XMP metadata
  void writeXMPMetadata() override;

  /// Write EXIF metadata
  void writeExifMetadata() override;


 public:

  /// Constructor
  /** @param quality JPEG Quality factor (0-100) */
  JPEGCompressor( int quality ) : Compressor(quality), cinfo{}, dest_mgr{}, dest(NULL) {
    dest = &dest_mgr;
  };


  /// Destructor
  ~JPEGCompressor(){
    // Free an owned scratch/whole-tile buffer only if a throw bypassed the
    // normal free. Never frees a borrowed strip buffer (owns_source == false).
    if( dest->owns_source && dest->source ){
      delete[] dest->source;
      dest->source = NULL;
      dest->owns_source = false;
    }

    // Safe if never created or already destroyed (jpeg_destroy is re-callable)
    jpeg_destroy_compress( &cinfo );
  };


  /// Set the compression quality
  /** @param factor Quality factor (0-100) */
  inline void setQuality( int factor ) override {

    // Flag that user has manually changed quality level
    default_quality = false;

    if( factor < 0 ) Q = 0;
    else if( factor > 100 ) Q = 100;
    else Q = factor;
  };


  /// Determine whether encoder supports the given number of channels
  /** @param channels number of channels
      @return whether or not the channel count is supported
  */
  inline bool supportsChannels( unsigned int channels ) const override {
    return channels == 1 || channels == 3;
  }


  /// Initialise strip based compression
  /** If we are doing a strip based encoding, we need to first initialise
      with InitCompression, then compress a single strip at a time using
      CompressStrip and finally clean up using Finish
      @param rawtile tile containing the image to be compressed
      @param strip_height pixel height of the strip we want to compress
   */
  void InitCompression( const RawTile& rawtile, unsigned int strip_height ) override;

  /// Compress a strip of image data
  /** @param s source image data
      @param o output buffer
      @param tile_height pixel height of the tile we are compressing
   */
  unsigned int CompressStrip( unsigned char* s, unsigned char* o, unsigned int tile_height ) override;

  /// Finish the strip based compression and free memory
  /** @param output output buffer
      @return size of output generated
   */
  unsigned int Finish( unsigned char* output ) override;

  /// Compress an entire buffer of image data at once in one command
  /** @param t tile of image data */
  unsigned int Compress( RawTile& t ) override;

  /// Return the JPEG mime type
  inline const char* getMimeType() const override { return "image/jpeg"; }

  /// Return the image filename suffix
  inline const char* getSuffix() const override { return "jpg"; }

  /// Get compression type
  inline ImageEncoding getImageEncoding() const override { return ImageEncoding::JPEG; };

  /// Inject metadata into raw JPEG bitstream
  /** @param r image tile
   */
  void injectMetadata( RawTile& r ) override;

};


#endif
