/*  IIP Server: OpenJPEG JPEG2000 handler

    Copyright (C) 2019-2023 Ruven Pillay.

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

#ifndef _OPENJPEGIMAGE_H
#define _OPENJPEGIMAGE_H

#include "IIPImage.h"
#include <openjpeg.h>


#define TILESIZE 256


/// Image class for JPEG 2000 Images:
/// Inherits from IIPImage. Uses the OpenJPEG library.
class OpenJPEGImage : public IIPImage {

 private:

  opj_stream_t* _stream;  /// file stream
  opj_codec_t*  _codec;   /// codec
  opj_image_t*  _image;   /// image


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
  OpenJPEGImage() : IIPImage(){
    _stream = NULL; _codec = NULL; _image = NULL;
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE);
  };


  /// Constructor
  /** @param path image path
   */
  OpenJPEGImage( const std::string& path)  : IIPImage(path){
    _stream = NULL; _codec = NULL; _image = NULL;
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE);
  };


  /// Copy Constructor
  /** @param image OpenJPEG object
   */
  OpenJPEGImage( const OpenJPEGImage& image ): IIPImage( image ) {};


  /// Copy Constructor
  /** @param image IIPImage object
   */
  OpenJPEGImage( const IIPImage& image ) : IIPImage(image){
    _stream = NULL; _codec = NULL; _image = NULL;
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE);
  };


  /// Destructor
  ~OpenJPEGImage(){ closeImage(); };


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
   */
  RawTile getTile( int x, int y, unsigned int r, int l, unsigned int t );


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

};

#endif
