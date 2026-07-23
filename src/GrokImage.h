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
#include <mutex>
#include <grok.h>


#define TILESIZE 256


/// Image class for JPEG 2000 Images:
/// Inherits from IIPImage. Uses the Grok library.
class GrokImage : public IIPImage {

 private:

  grk_object* _codec = nullptr;   /// codec (opaque object)
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
      @param d buffer to fill
   */
  void process( unsigned int r, int l, int x, int y, unsigned int w, unsigned int h, void* d );

  /// Helper to convert Grok planar data to interleaved
  void planarToInterleaved( const grk_image* img, void* interleaved_data,
                           unsigned int width, unsigned int height,
                           unsigned int channels, unsigned int out_bpc,
                           unsigned int factor);

  /// Initialize decompression parameters
  void initDecompressParams();


 public:

  /// Constructor
  GrokImage() : IIPImage(){
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE);
    initDecompressParams();
  };


  /// Constructor
  /** @param path image path
   */
  GrokImage( const std::string& path)  : IIPImage(path) {
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE);
    initDecompressParams();
  };


  /// Copy Constructor
  /** @param image Grok object
   */
  GrokImage( const GrokImage& image ): IIPImage( image ) {
    if( tile_widths.empty() ){
      tile_widths.push_back(TILESIZE);
      tile_heights.push_back(TILESIZE);
    }
    initDecompressParams();
  };


  /// Copy Constructor
  /** @param image IIPImage object
   */
  GrokImage( const IIPImage& image ) : IIPImage(image){
    if( tile_widths.empty() ){
      tile_widths.push_back(TILESIZE);
      tile_heights.push_back(TILESIZE);
    }
    initDecompressParams();
  };


  /// Destructor
  ~GrokImage(){ closeImage(); };


  /// Overloaded function for opening a JPEG2000 image
  void openImage();


  /// Overloaded function for loading JP2 image information
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
  */
  void loadImageInfo( int x, int y );


  /// Overloaded function for closing a JP2 image
  void closeImage();


  /// Return whether this image type directly handles region decoding
  bool regionDecoding(){ return true; };


  /// Overloaded function for getting a particular tile
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
      @param r resolution
      @param l number of quality layers to decode
      @param t tile number
      @param e image encoding
   */
  RawTile getTile( int x, int y, unsigned int r, int l, unsigned int t, ImageEncoding e = ImageEncoding::RAW );


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
  RawTile getRegion( int ha, int va, unsigned int res, int layers, int x, int y, unsigned int w, unsigned int h );


  /// Get codec version
  /** @return codec version */
  static std::string getCodecVersion(){
    return grk_version();
  }


};

#endif
