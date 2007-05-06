/*
    Image Transforms

    Copyright (C) 2004 Ruven Pillay.

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


#ifndef _COLOUR_TRANSFORMS_H
#define _COLOUR_TRANSFORMS_H


/// Utility function to convert between CIELAB and sRGB colour spaces
/** \param in pointer to input CIELAB data
    \param out pointer to buffer in which to put sRGB output
*/
void iip_LAB2sRGB( unsigned char *in, unsigned char *out );


/// Hillshading function to simulate raking light images
/** \param in pointer to input data of the normal vector at each point
    \param out pointer to buffer in which to put the hillshaded greyscale output
    \param h_angle angle in the horizontal plane from  12 o'clock in degrees
    \param v_angle angle in the vertical plane in degrees. 0 is flat, 90 pointing directly down.
    \param contrast contrast adjustment
*/
void shade( unsigned char *in, unsigned char *out, int h_angle, int v_angle, float contrast );

#endif
