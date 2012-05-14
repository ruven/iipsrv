// Image Transform Functions

/*  IIP fcgi server module - image processing routines

    Copyright (C) 2004-2012 Ruven Pillay.

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


#include <cmath>
#include "Transforms.h"

/* D65 temp 6504.
 */
#define D65_X0 95.0470
#define D65_Y0 100.0
#define D65_Z0 108.8827


static const float _sRGB[3][3] = { {  3.240479, -1.537150, -0.498535 },
		                  { -0.969256, 1.875992, 0.041556 },
		                  { 0.055648, -0.204043, 1.057311 } };



// Hillshading function
void filter_shade( RawTile& in, int h_angle, int v_angle ){

  unsigned char* buffer = new unsigned char[in.width*in.height];

  // Incident light angle
  float a = (h_angle * 2 * 3.14159) / 360.0;
  // We assume a hypotenous of 1.0
  float s_y = cos(a);
  float s_x = sqrt( 1.0 - s_y*s_y );
  if( h_angle > 180 ){
    s_x = -s_x;
  }

  a = (v_angle * 2 * 3.14159) / 360.0;
  float s_z = - sin(a);

  float s_norm = sqrt( s_x*s_x + s_y*s_y + s_z*s_z );
  s_x = s_x / s_norm;
  s_y = s_y / s_norm;
  s_z = s_z / s_norm;

  unsigned char* ptr = (unsigned char*) in.data;
  unsigned int k = 0;

  for( int n=0; n<in.dataLength; n+=3 ){

    float o_x = (float) - (ptr[n]-128.0) / 128.0;
    float o_y = (float) - (ptr[n+1]-128.0) / 128.0;
    float o_z = (float) - (ptr[n+2]-128.0) / 128.0;

    float dot_product;
    if( ptr[n] == 0 && ptr[n+1] == 0 && ptr[n+2] == 0 ) dot_product = 0.0;
    else dot_product = (s_x*o_x) + (s_y*o_y) + (s_z*o_z);

    dot_product = dot_product * 255.0;
    if( dot_product < 0 ) dot_product = 0.0;
    
    buffer[k++] = (unsigned char) dot_product;
  }

  delete[](unsigned char*) in.data;
  in.data = (void*) buffer;
  in.channels = 1;
  in.dataLength = in.width * in.height;

}


// Convert a single pixel from CIELAB to sRGB
static void LAB2sRGB( unsigned char *in, unsigned char *out ){

  /* First convert to XYZ
   */
  int l;
  float L, a, b;
  float X, Y, Z;
  double cby, tmp;
  double R, G, B;

  /* Extract our LAB - packed in TIFF as unsigned char for L
     and signed char for a/b. We also need to rescale
     correctly to 0-100 for L and -127 -> +127 for a/b.
  */
  l = in[0];
  L = (float) ( in[0] / 2.55 );
  l = ( (signed char*)in )[1];
  a = (float) l;
  l = ( (signed char*)in )[2];
  b = (float) l;


  if( L < 8.0 ) {
    Y = (L * D65_Y0) / 903.3;
    cby = 7.787 * (Y / D65_Y0) + 16.0 / 116.0;
  }
  else {
    cby = (L + 16.0) / 116.0;
    Y = D65_Y0 * cby * cby * cby;
  }

  tmp = a / 500.0 + cby;
  if( tmp < 0.2069 ) X = D65_X0 * (tmp - 0.13793) / 7.787;
  else X = D65_X0 * tmp * tmp * tmp;

  tmp = cby - b / 200.0;
  if( tmp < 0.2069 ) Z = D65_Z0 * (tmp - 0.13793) / 7.787;
  else Z = D65_Z0 * tmp * tmp * tmp;

  X /= 100.0;
  Y /= 100.0;
  Z /= 100.0;


  /* Then convert to sRGB
   */
  R = (X * _sRGB[0][0]) + (Y * _sRGB[0][1]) + (Z * _sRGB[0][2]);
  G = (X * _sRGB[1][0]) + (Y * _sRGB[1][1]) + (Z * _sRGB[1][2]);
  B = (X * _sRGB[2][0]) + (Y * _sRGB[2][1]) + (Z * _sRGB[2][2]);

  /* Clip any -ve values
   */
  if( R < 0.0 ) R = 0.0;
  if( G < 0.0 ) G = 0.0;
  if( B < 0.0 ) B = 0.0;


  /* We now need to convert these to non-linear display values
   */
  if( R <= 0.0031308 ) R *= 12.92;
  else R = 1.055 * pow( R, 1.0/2.4 ) - 0.055;

  if( G <= 0.0031308 ) G *= 12.92;
  else G = 1.055 * pow( G, 1.0/2.4 ) - 0.055;

  if( B <= 0.0031308 ) B *= 12.92;
  else B = 1.055 * pow( B, 1.0/2.4 ) - 0.055;

  /* Scale to 8bit
   */
  R *= 255.0;
  G *= 255.0;
  B *= 255.0;

  /* Clip to our 8 bit limit
   */
  if( R > 255.0 ) R = 255.0;
  if( G > 255.0 ) G = 255.0;
  if( B > 255.0 ) B = 255.0;


  /* Return our sRGB values
   */
  out[0] = (unsigned char) R;
  out[1] = (unsigned char) G;
  out[2] = (unsigned char) B;

}


// Convert whole tile from CIELAB to sRGB
void filter_LAB2sRGB( RawTile& in ){

  unsigned long np = in.width * in.height * in.channels;

  // Parallelize code using OpenMP
#pragma omp parallel for
  for( unsigned long n=0; n<np; n+=in.channels ){
    unsigned char* ptr = (unsigned char*) in.data;
    unsigned char q[3];
    LAB2sRGB( &ptr[n], &q[0] );
    ((unsigned char*)in.data)[n] = q[0];
    ((unsigned char*)in.data)[n+1] = q[1];
    ((unsigned char*)in.data)[n+2] = q[2];
  }
}


// Resize image using nearest neighbour interpolation
void filter_interpolate_nearestneighbour( RawTile& in, unsigned int resampled_width, unsigned int resampled_height ){

  unsigned char* buf = (unsigned char*) in.data; // Assume 8bit data
  int channels = in.channels;
  unsigned int width = in.width;
  unsigned int height = in.height;

  // Calculate our scale
  double xscale = (double)width / (double)resampled_width;
  double yscale = (double)height / (double)resampled_height;

  for( unsigned int j=0; j<resampled_height; j++ ){
    for( unsigned int i=0; i<resampled_width; i++ ){

      // Indexes in the current pyramid resolution and resampled spaces
      // Make sure to limit our input index to the image surface
      unsigned int ii = (unsigned int) floor(i*xscale);
      unsigned int jj = (unsigned int) floor(j*yscale);
      unsigned int pyramid_index = (unsigned int) channels * ( ii + jj*width );

      unsigned int resampled_index = (i + j*resampled_width)*channels;
      for( int k=0; k<in.channels; k++ ){
	buf[resampled_index+k] = buf[pyramid_index+k];
      }
    }
  }

  // Correctly set our Rawtile info
  in.width = resampled_width;
  in.height = resampled_height;
  in.dataLength = resampled_width * resampled_height * channels * in.bpc/8;

}


// Resize image using bilinear interpolation
//  - Floating point implementation which benchmarks about 2.5x slower than nearest neighbour
void filter_interpolate_bilinear( RawTile& in, unsigned int resampled_width, unsigned int resampled_height ){

  unsigned char* buf = (unsigned char*) in.data; // Assume 8bit data
  int channels = in.channels;
  unsigned int width = in.width;
  unsigned int height = in.height;

  // Calculate our scale
  float xscale = (float)width / (float)resampled_width;
  float yscale = (float)height / (float)resampled_height;

  for( unsigned int j=0; j<resampled_height; j++ ){

    // Index to the current pyramid resolution's bottom left right pixel
    unsigned int jj = (unsigned int) floor(j*yscale);

    // Calculate some weights - do this in the highest loop possible
    float jscale = j*yscale;
    float c = (float)(jj+1) - jscale;
    float d = jscale - (float)jj;

    for( unsigned int i=0; i<resampled_width; i++ ){

      // Index to the current pyramid resolution's bottom left right pixel
      unsigned int ii = (unsigned int) floor(i*xscale);

      // Calculate the indices of the 4 surrounding pixels
      unsigned int p11 = (unsigned int) ( channels * ( ii + jj*width ) );
      unsigned int p12 = (unsigned int) ( channels * ( ii + (jj+1)*width ) );
      unsigned int p21 = (unsigned int) ( channels * ( (ii+1) + jj*width ) );
      unsigned int p22 = (unsigned int) ( channels * ( (ii+1) + (jj+1)*width ) );
      unsigned int resampled_index = ((i + j*resampled_width) * channels);

      // Calculate the rest of our weights
      float iscale = i*xscale;
      float a = (float)(ii+1) - iscale;
      float b = iscale - (float)ii;

      for( int k=0; k<in.channels; k++ ){

	// If we are exactly coincident with a bounding box pixel, use that pixel value.
	// This should only ever occur on the top left p11 pixel.
	// Otherwise perform our full interpolation
	if( resampled_index == p11 ) buf[resampled_index+k] = buf[p11+k];
	else{
	  float tx = ((float)buf[p11+k])*a + ((float)buf[p21+k])*b;
	  float ty = ((float)buf[p12+k])*a + ((float)buf[p22+k])*b;
	  unsigned char r = (unsigned char)( c*tx + d*ty );
	  buf[resampled_index+k] = r;
	}
      }
    }
  }

  // Correctly set our Rawtile info
  in.width = resampled_width;
  in.height = resampled_height;
  in.dataLength = resampled_width * resampled_height * channels * in.bpc/8;

}


// Function to apply a contrast adjustment and clip to 8 bit
void filter_contrast( RawTile& in, float c ){

  unsigned int np = in.width * in.height * in.channels;

  unsigned char* buffer;

  if( in.bpc == 16 ){
    // Allocate new 8 bit buffer for tile
    buffer = new unsigned char[np];
    float contrast = c/256.0;
    for( unsigned int n=0; n<np; n++ ){
      float v = ((unsigned short*)in.data)[n] * contrast;
      v = (v<255.0) ? v : 255.0;
      buffer[n] = (unsigned char) v;
    }
    // Replace original buffer with new
    delete[] (unsigned short*) in.data;
    in.data = buffer;
    in.bpc = 8;
  }
  else{
    if( c == 1.0 ) return;
    buffer = (unsigned char*) in.data;
    for( unsigned int n=0; n<np; n++ ){
      float v = (float)(((unsigned char*)in.data)[n]) * c;
      v = (v<255.0) ? v : 255.0;
      buffer[n] = (unsigned char) v;
    }
  }

}
