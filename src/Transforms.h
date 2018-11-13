/*
    Image Transforms

    Copyright (C) 2004-2018 Ruven Pillay.

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


#ifndef _TRANSFORMS_H
#define _TRANSFORMS_H

#include <vector>
#include "RawTile.h"

enum interpolation { NEAREST, BILINEAR, CUBIC, LANCZOS2, LANCZOS3 };
enum cmap_type { HOT, COLD, JET, BLUE, GREEN, RED };


/// Image Processing Transforms
struct Transform {

 private:

  /// Private function to convert single pixel of CIELAB to sRGB
  /** @param in input buffer
      @param out output buffer
  */
  void LAB2sRGB( unsigned char *in, unsigned char *out );


 public:

  /// Get description of processing engine
  std::string getDescription(){ return "CPU processor"; };


  /// Function to create normalized array
  /** @param in tile data to be adjusted
      @param min : vector of minima
      @param max : vector of maxima
  */
  void normalize( RawTile& in, std::vector<float>& max, std::vector<float>& min );


  /// Function to apply colormap to gray images
  /** @param in tile data to be converted
      @param cmap color map to apply.
  */
  void cmap( RawTile& in, enum cmap_type cmap );


  /// Function to invert colormaps
  /** @param in tile data to be adjusted
   */
  void inv( RawTile& in );


  /// Hillshading function to simulate raking light images
  /** @param in tile input data containing normal vectors at each point
      @param h_angle angle in the horizontal plane from  12 o'clock in degrees
      @param v_angle angle in the vertical plane in degrees. 0 is flat, 90 pointing directly down.
  */
  void shade( RawTile& in, int h_angle, int v_angle );


  /// Convert from CIELAB to sRGB colour space
  /** @param in tile data to be converted */
  void LAB2sRGB( RawTile& in );


  /// Function to apply a contrast adjustment and clip to 8 bit
  /** @param in tile data to be adjusted
      @param c contrast value
  */
  void contrast( RawTile& in, float c );


  /// Apply a gamma correction
  /** @param in tile input data
      @param g gamma
  */
  void gamma( RawTile& in, float g );


  /// Resize image using nearest neighbour interpolation
  /** @param in tile input data
      @param w target width
      @param h target height
  */
  void interpolate_nearestneighbour( RawTile& in, unsigned int w, unsigned int h );


  /// Resize image using bilinear interpolation
  /** @param in tile input data
      @param w target width
      @param h target height
  */
  void interpolate_bilinear( RawTile& in, unsigned int w, unsigned int h );


  /// Rotate image - currently only by 90, 180 or 270 degrees, other values will do nothing
  /** @param in tile input data
      @param angle angle of rotation - currently only rotations by 90, 180 and 270 degrees
      are suported, for other values, no rotation will occur
  */
  void rotate( RawTile& in, float angle );


  /// Convert image to grayscale
  /** @param in input image */
  void greyscale( RawTile& in );


  /// Apply a color twist
  /** @param in input image
      @param ctw 2D color twist matrix
  */
  void twist( RawTile& in, const std::vector< std::vector<float> >& ctw );


  /// Extract bands
  /** @param in input image
      @param bands number of bands
  */
  void flatten( RawTile& in, int bands );


  ///Flip image
  /** @param in input image
      @param o orientation (0=horizontal,1=vertical)
  */
  void flip( RawTile& in, int o );

};

#endif
