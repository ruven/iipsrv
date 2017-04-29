/*
    Image Transforms

    Copyright (C) 2004-2013 Ruven Pillay.

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

/// Function to create normalized array
/** @param in tile data to be adjusted
    @param min : vector of minima
    @param max : vector of maxima
*/
void filter_normalize( RawTile& in, std::vector<float>& max, std::vector<float>& min );

/// Function to apply colormap to gray images
///   based on the routine colormap.cpp in Imagin Raytracer by Olivier Ferrand
///   http://www.imagin-raytracer.org
/** @param in tile data to be converted
    @param cmap color map to apply.
*/
enum cmap_type { HOT, COLD, JET, BLUE, GREEN, RED };
void filter_cmap( RawTile& in, enum cmap_type cmap );

/// Function to invert colormaps
/** @param in tile data to be adjusted
*/
void filter_inv( RawTile& in );


/// Hillshading function to simulate raking light images
/** @param in tile input data containing normal vectors at each point
    @param h_angle angle in the horizontal plane from  12 o'clock in degrees
    @param v_angle angle in the vertical plane in degrees. 0 is flat, 90 pointing directly down.
*/
void filter_shade( RawTile& in, int h_angle, int v_angle );


/// Convert from CIELAB to sRGB colour space
/** @param in tile data to be converted */
void filter_LAB2sRGB( RawTile& in );


/// Function to apply a contrast adjustment and clip to 8 bit
/** @param in tile data to be adjusted
    @param c contrast value
*/
void filter_contrast( RawTile& in, float c );


/// Apply a gamma correction
/** @param in tile input data
    @param g gamma
*/
void filter_gamma( RawTile& in, float g );


/// Resize image using nearest neighbour interpolation
/** @param in tile input data
    @param w target width
    @param h target height
*/
void filter_interpolate_nearestneighbour( RawTile& in, unsigned int w, unsigned int h );


/// Resize image using bilinear interpolation
/** @param in tile input data
    @param w target width
    @param h target height
*/
void filter_interpolate_bilinear( RawTile& in, unsigned int w, unsigned int h );

/// Resize image using Lanczos filter
/** @param in tile input data
    @param w target width
    @param h target height
*/
void filter_interpolate_lanczos( RawTile& in, int w, int h );

/// Rotate image - currently only by 90, 180 or 270 degrees, other values will do nothing
/** @param in tile input data
    @param angle angle of rotation - currently only rotations by 90, 180 and 270 degrees
    are suported, for other values, no rotation will occur
*/
void filter_rotate( RawTile& in, float angle );


/// Convert image to grayscale
/** @param in input image */
void filter_greyscale( RawTile& in );


/// Apply a color twist
/** @param in input image
    @param ctw 2D color twist matrix
*/
void filter_twist( RawTile& in, const std::vector< std::vector<float> >& ctw );


/// Extract bands
/** @param in input image
    @param bands number of bands
*/
void filter_flatten( RawTile& in, int bands );


///Flip image
/** @param in input image
    @param o orientation (0=horizontal,1=vertical)
*/
void filter_flip( RawTile& in, int o );


#endif
