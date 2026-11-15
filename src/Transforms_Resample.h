/*  IIPImage image processing routines: internal resampling helpers shared
    by Transform interpolation wrappers. Provide mamespaced, templated,
    optimized and threaded implementations of nearest neighbour, bilinear,
    bicubic, lanczos 2 and lanczos 3 resampling.

    Copyright (C) 2026 Ruven Pillay.

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


#ifndef _TRANSFORMS_RESAMPLE_H
#define _TRANSFORMS_RESAMPLE_H

#include "RawTile.h"


/// Internal image resizing functions: supports 8, 16 or 32-bit fixed point unsigned integer or 32-bit floating point
namespace resample {

  /// Nearest neighbour interpolation
  /** @param in input image
      @param resampled_width resized width
      @param resampled_height resized height
   */
  void resize_nearest( RawTile& in, unsigned int resampled_width, unsigned int resampled_height );


  /// Resize an 8-, 16- or 32-bit fixed-point or 32-bit floating-point tile,
  /// replacing its buffer with the result.
  /// Bilinear interpolation
  /** @param in input image
      @param resampled_width resized width
      @param resampled_height resized height
   */
  void resize_bilinear( RawTile& in, unsigned int resampled_width, unsigned int resampled_height );


  /// Bicubic interpolation
  /** @param in input image
      @param resampled_width resized width
      @param resampled_height resized height
   */
  void resize_bicubic( RawTile& in, unsigned int resampled_width, unsigned int resampled_height );


  /// Lanczos 2/3 interpolation
  /** @param in input image
      @param resampled_width resized width
      @param resampled_height resized height
      @param support Lanczos 2/3 switch: 2 = lanczos 2, 3 = lanczos 3
  */
  void resize_lanczos( RawTile& in, unsigned int resampled_width, unsigned int resampled_height, int support );

}

#endif  // _TRANSFORMS_RESAMPLE_H
