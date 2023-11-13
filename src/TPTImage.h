// Tiled Pyramidal Tiff class interface

/*  IIPImage Tiled Pyramidal TIFF Class

    Copyright (C) 2000-2023 Ruven Pillay.

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


#ifndef _TPTIMAGE_H
#define _TPTIMAGE_H


#include "IIPImage.h"
//#include <tiff.h>
#include <tiffio.h>




/// Image class for Tiled Pyramidal Images: Inherits from IIPImage. Uses libtiff
class TPTImage : public IIPImage {

 private:

  /// Pointer to the TIFF library struct
  TIFF *tiff;

  /// List of SubIFD sub-resolutions
  std::vector<uint32_t> subifds;

  /// To which IFD do these SubIFDs belong
  tdir_t subifd_ifd;

  /// Load any SubIFD offsets
  void loadSubIFDs();

  /// Load any stack metadata - name and scale
  void loadStackInfo();


 public:

  /// Constructor
  TPTImage():IIPImage(), tiff( NULL ) {};

  /// Constructor
  /** @param path image path
   */
  TPTImage( const std::string& path ): IIPImage(path), tiff(NULL), subifd_ifd(0) {};

  /// Copy Constructor
  /** @param image IIPImage object
   */
  TPTImage( const TPTImage& image ): IIPImage(image), tiff(NULL), subifd_ifd(0) {};

  /// Assignment Operator
  /** @param image TPTImage object
   */
  TPTImage& operator = ( TPTImage image ) {
    if( this != &image ){
      closeImage();
      IIPImage::operator=(image);
      tiff = image.tiff;
    }
    return *this;
  }

  /// Construct from an IIPImage object
  /** @param image IIPImage object
   */
  TPTImage( const IIPImage& image ): IIPImage(image), tiff(NULL), subifd_ifd(0) {};

  /// Destructor
  ~TPTImage() { closeImage(); };

  /// Overloaded static function for seting up logging for codec library
  static void setupLogging();

  /// Overloaded function for opening a TIFF image
  void openImage();

  /// Overloaded function for loading TIFF image information
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
   */
  void loadImageInfo( int x, int y );

  /// Overloaded function for closing a TIFF image
  void closeImage();

  /// Overloaded function for getting a particular tile
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
      @param r resolution
      @param l quality layers
      @param t tile number
   */
  RawTile getTile( int x, int y, unsigned int r, int l, unsigned int t );

};


#endif
