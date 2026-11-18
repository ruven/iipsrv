/*  IIP Server: Grok JPEG2000 handler

    Copyright (C) 2019-2026 Ruven Pillay.
    Modified for Grok support

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef IIP_GROKIMAGE_H
#define IIP_GROKIMAGE_H

#include "IIPImage.h"
#include <memory>
#include <mutex>
#include <grok.h>


/// Image class for JPEG 2000 Images:
/// Inherits from IIPImage. Uses the Grok library.
class GrokImage final : public IIPImage {

 private:

  static constexpr unsigned int DEFAULT_TILE_SIZE = 256;

  struct CodecDeleter {
    void operator()( grk_object* codec ) const noexcept;
  };

  struct TileGeometry {
    unsigned int width;
    unsigned int height;
    int x;
    int y;
  };

  using CodecPtr = std::unique_ptr<grk_object, CodecDeleter>;

  CodecPtr _codec;   /// owning codec handle
  grk_stream_params _stream_params{}; /// stream parameters
  grk_decompress_parameters _decompress_params{}; /// decompress parameters
  grk_header_info _header{}; /// header info cache
  grk_image* _image = nullptr;   /// image owned by the codec
  bool _header_read = false; /// flag indicating header has been read
  std::mutex _decode_mutex;

  /// Main processing function
  /** @param r resolution
      @param l number of quality levels to decode
      @param x x coordinate
      @param y y coordinate
      @param w width of region
      @param h height of region
      @param output tile whose buffer will be filled
   */
  void process( unsigned int r, int l, int x, int y, unsigned int w,
                unsigned int h, RawTile& output );

  /// Initialize decompression parameters
  void initDecompressParams();

  /// Initialize default tile dimensions when the base image has none
  void initTileDimensions();

  /// Populate native and virtual resolution dimensions
  void populateResolutionLevels( unsigned int width, unsigned int height );

  /// Return geometry for a requested tile
  TileGeometry getTileGeometry( unsigned int resolution, unsigned int tile ) const;

  /// Normalize source precision to the RawTile storage precision
  unsigned int getOutputBitsPerChannel() const;


 public:

  /// Constructor
  GrokImage() : IIPImage(){
    initTileDimensions();
    initDecompressParams();
  }


  /// Constructor
  /** @param path image path
   */
  explicit GrokImage( const std::string& path ) : IIPImage(path) {
    initTileDimensions();
    initDecompressParams();
  }


  /// Copy Constructor
  /** @param image Grok object
   */
  GrokImage( const GrokImage& image ): IIPImage( image ) {
    initTileDimensions();
    initDecompressParams();
  }


  /// Copy Constructor
  /** @param image IIPImage object
   */
  explicit GrokImage( const IIPImage& image ) : IIPImage(image){
    initTileDimensions();
    initDecompressParams();
  }


  /// Destructor
  ~GrokImage() override = default;


  /// Overloaded function for opening a JPEG2000 image
  void openImage() override;


  /// Overloaded function for loading JP2 image information
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
  */
  void loadImageInfo( int x, int y ) override;


  /// Overloaded function for closing a JP2 image
  void closeImage() override;


  /// Return whether this image type directly handles region decoding
  bool regionDecoding() override { return true; }


  /// Overloaded function for getting a particular tile
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
      @param r resolution
      @param l number of quality layers to decode
      @param t tile number
      @param e image encoding
   */
  RawTile getTile( int x, int y, unsigned int r, int l, unsigned int t,
                   ImageEncoding e = ImageEncoding::RAW ) override;


  /// Overloaded function for returning a region from image
  /**
    @param ha       horizontal angle
    @param va       vertical angle
    @param res      resolution
    @param layers   number of quality layers to decode
    @param x        x coordinate
    @param y        y coordinate
    @param w        width of region
    @param h        height of region
    @return         a RawTile object
  */
  RawTile getRegion( int ha, int va, unsigned int res, int layers, int x,
                     int y, unsigned int w, unsigned int h ) override;


  /// Get codec version
  /** @return codec version */
  static std::string getCodecVersion(){
    return grk_version();
  }


};

#endif
