// Tiled Pyramidal Tiff class interface

/*  IIP Tiled Pyramidal TIFF Class

    Copyright (C) 2000-2006 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifndef _TPTIMAGE_H
#define _TPTIMAGE_H


#include "IIPImage.h"
#include <tiff.h>
#include <tiffio.h>


/// Image class for Tiled Pyramidal Images: Inherits from IIPImage. Uses libtiff.


class TPTImage : public IIPImage {

 private:

  /// Pointer to the TIFF library struct
  TIFF *tiff;

  /// Tile data buffer
  /** The buffer for reading tiles from the
      image - assuming a constant tile size,
      then this could probably be shared for
      all images, but keep here now for
      simplicity.
   */
  tdata_t tile_buf;


 public:

  /// Constructor
  TPTImage():IIPImage() { 
    tiff = NULL; tile_buf = NULL; 
    tile_width = 0; tile_height = 0;
    numResolutions = 0;
  };

  /// Constructor
  /** \param path image path
   */
  TPTImage( const std::string& path ): IIPImage( path ) { 
    tiff = NULL; tile_buf = NULL; 
    tile_width = 0; tile_height = 0;
    numResolutions = 0;
  };

  /// Copy Constructor
  /** \param image IIPImage object
   */
  TPTImage( const IIPImage& image ): IIPImage( image ) {
    tiff = NULL; tile_buf = NULL; 
    tile_width = 0; tile_height = 0;
    numResolutions = 0;
  };

  /// Destructor
  ~TPTImage() { closeImage(); };

  /// Overloaded function for opening a TIFF image
  void openImage() throw (std::string);

  /// Overloaded function for loading TIFF image information
  /** \param x horizontal sequence angle
      \param y vertical sequence angle
   */
  void loadImageInfo( int x, int y ) throw (std::string);

  /// Overloaded function for closing a TIFF image
  void closeImage();

  /// Overloaded function for getting a particular tile
  /** \param x horizontal sequence angle
      \param y vertical sequence angle
      \param r resolution
      \param t tile number
   */
  RawTile getTile( int x, int y, unsigned int r, unsigned int t ) throw (std::string);


};



#endif
