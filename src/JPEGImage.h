/*  IIP Server: JPEG input handler: Efficient decoding of tiles and regions from JPEG images

    Copyright (C) 2024 Ruven Pillay.

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

#ifndef _JPEGIMAGE_H
#define _JPEGIMAGE_H

#include "IIPImage.h"
#include <jpeglib.h>



#define TILESIZE 256


/// Image class for JPEG Images:
/// Inherits from IIPImage. Uses the libjpeg API
class JPEGImage : public IIPImage {

 private:

  /// Input file
  FILE *_input;

  /// JPEG library objects
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;


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



 public:

  /// Constructor
  JPEGImage() : IIPImage(){
    _input = NULL;
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE);
  };


  /// Constructor
  /** @param path image path
   */
  JPEGImage( const std::string& path )  : IIPImage(path){
    _input = NULL;
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE);
  };


  /// Copy Constructor
  /** @param image JPEG object
   */
  JPEGImage( const JPEGImage& image ): IIPImage( image ) {};


  /// Copy Constructor
  /** @param image IIPImage object
   */
  JPEGImage( const IIPImage& image ) : IIPImage(image){
    _input = NULL;
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE);
  };


  /// Destructor
  ~JPEGImage(){ closeImage(); };


  /// Overloaded function for opening a TIFF image
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
#ifdef LIBJPEG_TURBO_VERSION
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
    return "libjpeg-turbo " TOSTRING(LIBJPEG_TURBO_VERSION);
#else
    return "libjpeg-" JPEG_LIB_VERSION;
#endif
  };

};

#endif
