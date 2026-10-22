// Image Transform Functions

/*  IIPImage image processing routines

    Copyright (C) 2004-2026 Ruven Pillay.

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
#include <algorithm>
#include <numeric>
#include <limits>
#include "Transforms.h"

#if HAVE_EIGEN3
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#endif



// Define something similar to C99 std::isfinite if this does not exist
// Need to also check for a direct define as it can be implemented as a macro
#ifndef HAVE_ISFINITE
#ifndef isfinite
#include <limits>
static bool isfinite( float arg )
{
  return arg == arg &&
    arg != std::numeric_limits<float>::infinity() &&
    arg != -std::numeric_limits<float>::infinity();
}
#endif
#endif



/* Size threshold for using parallel loops (256x256 pixels)
 */
#define PARALLEL_THRESHOLD 65536


/* D65 temp 6504.
 */
#define D65_X0 95.0470
#define D65_Y0 100.0
#define D65_Z0 108.8827

/* D65 XYZ <-> sRGB conversion matrices
 */
static const float XYZ_sRGB[3][3] = { { 3.2406255, -1.537208, -0.4986286},
				      {-0.9689307,  1.8757561, 0.0415175},
				      { 0.0557101, -0.2040211, 1.0569959} };

static const float sRGB_XYZ[3][3]  = { { 0.4124564,  0.3575761, 0.1804375},
				       { 0.2126729,  0.7151522, 0.0721750},
				       { 0.0193339,  0.1191920, 0.9503041} };


using namespace std;



// Normalization function
void Transform::normalize( RawTile& in, const vector<float>& max, const vector<float>& min ) {

  float *normdata;
  uint32_t np = (uint32_t) in.width * in.height * in.channels;
  unsigned int nc = in.channels;

  // Type pointers
  float* fptr;
  unsigned int* uiptr;
  unsigned short* usptr;
  unsigned char* ucptr;

  if( in.bpc == 32 && in.sampleType == SampleType::FLOATINGPOINT ) {
    normdata = (float*)in.data;
  }
  else {
    normdata = new float[np];
  }

  // Loop through each channel
  for( uint32_t c = 0 ; c<nc ; c++){

    float minc = min[c];
    float diffc = max[c] - minc;
    float invdiffc = fabs(diffc) > 1e-30? 1./diffc : 1e30;

    // Normalize our data
    if( in.bpc == 32 && in.sampleType == SampleType::FLOATINGPOINT ) {

      // Skip if channel data already normalized
      if( min[c] == 0.0 && max[c] == 1.0 ) continue;

      fptr = (float*)in.data;
      // Loop through our pixels for floating point pixels
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
      for( uint32_t n=c; n<np; n+=nc ){
        normdata[n] = isfinite(fptr[n])? (fptr[n] - minc) * invdiffc : 0.0f;
      }
    }
    else if( in.bpc == 32 && in.sampleType == SampleType::FIXEDPOINT ) {
      uiptr = (unsigned int*)in.data;
      // Loop through our pixels for unsigned int pixels
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
      for( uint32_t n=c; n<np; n+=nc ){
        normdata[n] = (uiptr[n] - minc) * invdiffc;
      }
    }
    else if( in.bpc == 16 ) {
      usptr = (unsigned short*)in.data;
      // Loop through our unsigned short pixels
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
      for( uint32_t n=c; n<np; n+=nc ){
        normdata[n] = (usptr[n] - minc) * invdiffc;
      }
    }
    else {
      ucptr = (unsigned char*)in.data;
      // Loop through our unsigned char pixels
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
      for( uint32_t n=c; n<np; n+=nc ){
        normdata[n] = (ucptr[n] - minc) * invdiffc;
      }
    }
  }

  // Delete our original buffers, unless we already had floats
  if( in.bpc == 32 && in.sampleType == SampleType::FIXEDPOINT ){
    delete[] (unsigned int*) in.data;
  }
  else if( in.bpc == 16 ){
    delete[] (unsigned short*) in.data;
  }
  else if( in.bpc == 8 ){
    delete[] (unsigned char*) in.data;
  }

  // Assign our new buffer and modify some info
  in.data = normdata;
  in.bpc = 32;
  in.sampleType = SampleType::FLOATINGPOINT;
  in.dataLength = np << 2;
  in.capacity = in.dataLength;

}



// Hillshading function - input data is a normalized floating-point 3-band normal map
void Transform::shade( RawTile& in, int h_angle, int v_angle ){

  // Incident light angle
  float a = (h_angle * 2 * M_PI) / 360.0;

  // We assume a hypotenous of 1.0
  float s_y = cos(a);
  float s_x = sqrt( 1.0 - s_y*s_y );
  if( h_angle > 180 ) s_x = -s_x;

  a = (v_angle * 2 * M_PI) / 360.0;
  float s_z = - sin(a);

  float s_norm = sqrt( s_x*s_x + s_y*s_y + s_z*s_z );
  s_x = s_x / s_norm;
  s_y = s_y / s_norm;
  s_z = s_z / s_norm;

  /* Normal components are o = 1 - 2*v, so output = 0.5 + 0.5*(s.o)
   *                         = 0.5 + 0.5*(s_x+s_y+s_z) - (s.v).
   * Precompute the constant once instead of per pixel
   */
  const float c = 0.5 + 0.5 * ( s_x + s_y + s_z );

  // Number of pixels
  uint32_t np = (uint32_t) in.width * in.height;

  float *infptr = (float*) in.data;

  // Create new (float) data buffer
  float *buffer = new float[np];

#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( np > PARALLEL_THRESHOLD )
#endif
  for( uint32_t k=0; k<np; k++ ){

    unsigned int n = k*3;

    float dot_product;
    // (0,0,0) normal map pixels signify no surface data -> black
    if( infptr[n] == 0.0 && infptr[n+1] == 0.0 && infptr[n+2] == 0.0 ){
      dot_product = 0.0;
    }
    else {
      dot_product = c - ( (s_x*infptr[n]) + (s_y*infptr[n+1]) + (s_z*infptr[n+2]) );
    }

    if( dot_product < 0.0 ) dot_product = 0.0;
    else if( dot_product > 1.0 ) dot_product = 1.0;

    buffer[k] = dot_product;
  }

  // Delete old data buffer
  delete[] (float*) in.data;

  in.data = buffer;
  in.channels = 1;
  in.dataLength = (uint32_t) in.width * in.height * (in.bpc/8);
  in.capacity = in.dataLength;
}



// Convert a single pixel from CIELAB to sRGB
void Transform::LAB2sRGB( const float *in, float *out ){

  float L, a, b;
  float X, Y, Z;
  double cby, tmp;
  float R, G, B;

  L = in[0];
  a = in[1];
  b = in[2];

  // Convert CIELAB to XYZ
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

  // Convert from XYZ to sRGB
  R = (X * XYZ_sRGB[0][0]) + (Y * XYZ_sRGB[0][1]) + (Z * XYZ_sRGB[0][2]);
  G = (X * XYZ_sRGB[1][0]) + (Y * XYZ_sRGB[1][1]) + (Z * XYZ_sRGB[1][2]);
  B = (X * XYZ_sRGB[2][0]) + (Y * XYZ_sRGB[2][1]) + (Z * XYZ_sRGB[2][2]);

  // Clip any -ve values
  R = (R<0.0 ? 0.0 : R);
  G = (G<0.0 ? 0.0 : G);
  B = (B<0.0 ? 0.0 : B);

  // We now need to convert these to non-linear display values
  if( R <= 0.0031308 ) R *= 12.92;
  else R = 1.055 * powf( R, 1.0/2.4 ) - 0.055;

  if( G <= 0.0031308 ) G *= 12.92;
  else G = 1.055 * powf( G, 1.0/2.4 ) - 0.055;

  if( B <= 0.0031308 ) B *= 12.92;
  else B = 1.055 * powf( B, 1.0/2.4 ) - 0.055;

  // Clip to 0.0 - 1.0
  R = (R>1.0 ? 1.0 : R);
  G = (G>1.0 ? 1.0 : G);
  B = (B>1.0 ? 1.0 : B);

  // Return our normalized sRGB values
  out[0] = R;
  out[1] = G;
  out[2] = B;
}



// Convert a single pixel from normalized sRGB to CIELAB
void Transform::sRGB2LAB( const float *in, float *out ){

  float R,G,B;
  float X,Y,Z;
  float L,a,b;

  R = in[0];
  G = in[1];
  B = in[2];

  // Linearize
  if( R <= 0.04045 ) R /= 12.92;
  else R = powf( ( (R+0.055) / 1.055 ), 2.4 );

  if( G <= 0.04045 ) G /= 12.92;
  else G = powf( ( (G+0.055) / 1.055 ), 2.4 );

  if( B <= 0.04045 ) B /= 12.92;
  else B = powf( ( (B+0.055) / 1.055 ), 2.4 );

  // Linear RGB needs to be scaled to 100
  R += 100.0;
  G *= 100.0;
  B *= 100.0;

  // Convert from linear RGB to XYZ
  X = (R * sRGB_XYZ[0][0]) + (G * sRGB_XYZ[0][1]) + (B * sRGB_XYZ[0][2]);
  Y = (R * sRGB_XYZ[1][0]) + (G * sRGB_XYZ[1][1]) + (B * sRGB_XYZ[1][2]);
  Z = (R * sRGB_XYZ[2][0]) + (G * sRGB_XYZ[2][1]) + (B * sRGB_XYZ[2][2]);

  // Scale XYZ by illuminant
  X /= D65_X0;
  Y /= D65_Y0;
  Z /= D65_Z0;

  // Convert from XYZ to CIELAB
  if( X > 0.008856452 ) X = cbrtf(X);
  else                  X = ( 7.787037037 * X ) + 0.137931034;

  if( Y > 0.008856452 ) Y = cbrtf(Y);
  else                  Y = ( 7.787037037 * Y ) + 0.137931034;

  if( Z > 0.008856452 ) Z = cbrtf(Z);
  else                  Z = ( 7.787037037 * Z ) + 0.137931034;


  L = ( 116.0 * Y ) - 16.0;
  a = 500.0 * ( X - Y );
  b = 200.0 * ( Y - Z );

  // Return our CIELAB values
  out[0] = L;
  out[1] = a;
  out[2] = b;
}



// Convert whole image from CIELAB to sRGB. Input data can be in 8 bit, 16 bit or floating point form
void Transform::LAB2sRGB( RawTile& in ){

  uint32_t np = (uint32_t) in.width * in.height * in.channels;

  // Output data should always be floating point
  float *output;

  // Set up our output buffer depending on input data type. If not already floating point, create new buffer
  if( in.sampleType == SampleType::FIXEDPOINT ) output = new float[np];
  else output = ((float*)in.data);

  // Parallelize code using OpenMP
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
  for( uint32_t n=0; n<np; n+=in.channels ){

    float rgb[3];
    float cielab[3] = {0.0,0.0,0.0};

    // 8 and 16 bit TIFF require unpacking
    if( in.sampleType == SampleType::FIXEDPOINT && in.bpc == 8 ){
      unsigned char *data = (unsigned char*) in.data;
      unsigned char *ptr = &data[n];
      cielab[0] = (float)(ptr[0]) / 2.55;
      float v = ((signed char*)ptr)[1];
      cielab[1] = v;
      v = ((signed char*)ptr)[2];
      cielab[2] = v;
    }
    else if( in.sampleType == SampleType::FIXEDPOINT && in.bpc == 16 ){
      unsigned short *data = (unsigned short*) in.data;
      unsigned short *ptr = &data[n];
      cielab[0] = (ptr[0]>>1) / 327.67;
      float l = ((short*)ptr)[1] >> 8;
      cielab[1] = l;
      l = ((short*)ptr)[2] >> 8;
      cielab[2] = l;
    }
    // Floating point TIFF can be read directly
    else if( in.sampleType == SampleType::FLOATINGPOINT ){
      float *data = (float*) in.data;
      cielab[0] = data[n];
      cielab[1] = data[n+1];
      cielab[2] = data[n+2];
    }

    // Perform color conversion on this pixel
    LAB2sRGB( cielab, rgb );

    output[n]   = rgb[0];
    output[n+1] = rgb[1];
    output[n+2] = rgb[2];
  }

  // If input was not floating point, update image metadata
  if( in.sampleType == SampleType::FIXEDPOINT ){
    if( in.bpc == 8 ) delete[] (unsigned char*) in.data;
    else if( in.bpc == 16 ) delete[] (unsigned short*) in.data;
    in.data = (void*) output;
    in.memoryManaged = 1;
    in.bpc = 32;
    in.sampleType = SampleType::FLOATINGPOINT;
    in.dataLength = np << 2; // Multiply by 4 for float
  }
}



// Convert whole image from sRGB to CIELAB - input should be normalized and in floating point format
void Transform::sRGB2LAB( RawTile& in ){

  uint32_t np = (uint32_t) in.width * in.height * in.channels;

  // Parallelize code using OpenMP
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
  for( uint32_t n=0; n<np; n+=in.channels ){

    float rgb[3];
    float cielab[3];

    rgb[0] = ((float*)in.data)[n];
    rgb[1] = ((float*)in.data)[n+1];
    rgb[2] = ((float*)in.data)[n+2];

    sRGB2LAB( rgb, cielab );

    ((float*)in.data)[n]   = cielab[0];
    ((float*)in.data)[n+1] = cielab[1];
    ((float*)in.data)[n+2] = cielab[2];
  }
}



// Colormap function - produce a 3 band color mapped image from normalized input data
// Based on the routine colormap.cpp in Imagin Raytracer by Olivier Ferrand http://www.imagin-raytracer.org
// Note that the colormap is applied to only the first band of each input image
void Transform::cmap( RawTile& in, enum cmap_type cmap ){

  unsigned in_chan = in.channels;
  unsigned out_chan = 3;
  uint32_t np = (uint32_t) in.width * in.height;               // pixels in input image
  uint32_t onp = (uint32_t) in.width * in.height * out_chan;   // pixels*channels in output image

  const float max3 = 1.0/3.0;
  const float max8 = 1.0/8.0;

  float *fptr = (float*)in.data;
  float *outptr = new float[onp];

  switch(cmap){

    case HOT:
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
      for( uint32_t p=0; p<np; p++ ){
        float value = fptr[p*in_chan];
        float *outv = outptr + p*3;
        if(value>1.)
          { outv[0]=outv[1]=outv[2]=1.; }
        else if(value<=0.)
          { outv[0]=outv[1]=outv[2]=0.; }
        else if(value<max3)
          { outv[0]=3.*value; outv[1]=outv[2]=0.; }
        else if(value<2*max3)
          { outv[0]=1.; outv[1]=3.*value-1.; outv[2]=0.; }
        else if(value<1.)
          { outv[0]=outv[1]=1.; outv[2]=3.*value-2.; }
        else { outv[0]=outv[1]=outv[2]=1.; }
      }
      break;

    case COLD:
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
      for( uint32_t p=0; p<np; p++ ){
        float value = fptr[p*in_chan];
        float *outv = outptr + p*3;
        if(value>1.)
          { outv[0]=outv[1]=outv[2]=1.; }
        else if(value<=0.)
          { outv[0]=outv[1]=outv[2]=0.; }
        else if(value<max3)
          { outv[0]=outv[1]=0.; outv[2]=3.*value; }
        else if(value<2.*max3)
          { outv[0]=0.; outv[1]=3.*value-1.; outv[2]=1.; }
        else if(value<1.)
          { outv[0]=3.*value-2.; outv[1]=outv[2]=1.; }
        else {outv[0]=outv[1]=outv[2]=1.;}
      }
      break;

    case JET:
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
      for( uint32_t p=0; p<np; p++ ){
        float value = fptr[p*in_chan];
        float *outv = outptr + p*3;
        if(value<0.)
          { outv[0]=outv[1]=outv[2]=0.; }
        else if(value<max8)
          { outv[0]=outv[1]=0.; outv[2]= 4.*value + 0.5; }
        else if(value<3.*max8)
          { outv[0]=0.; outv[1]= 4.*value - 0.5; outv[2]=1.; }
        else if(value<5.*max8)
          { outv[0]= 4*value - 1.5; outv[1]=1.; outv[2]= 2.5 - 4.*value; }
        else if(value<7.*max8)
          { outv[0]= 1.; outv[1]= 3.5 -4.*value; outv[2]= 0; }
        else if(value<1.)
          { outv[0]= 4.5-4.*value; outv[1]= outv[2]= 0.; }
        else { outv[0]=0.5; outv[1]=outv[2]=0.; }
      }
      break;

    case RED:
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
      for( uint32_t p=0; p<np; p++ ){
        float value = fptr[p*in_chan];
        float *outv = outptr + p*3;
	outv[0] = value;
	outv[1] = outv[2] = 0.;
      }
      break;

    case GREEN:
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
      for( uint32_t p=0; p<np; p++ ) {
        float value = fptr[p*in_chan];
        float *outv = outptr + p*3;
	outv[0] = outv[2] = 0.;
	outv[1] = value;
      }
      break;

    case BLUE:
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
      for( uint32_t p=0; p<np; p++ ) {
        float value = fptr[p*in_chan];
        float *outv = outptr + p*3;
	outv[0] = outv[1] = 0;
	outv[2] = value;
      }
      break;

    default:
      break;

  };

  // Delete old data buffer
  delete[] (float*) in.data;
  in.data = outptr;
  in.channels = out_chan;
  in.dataLength = onp * (in.bpc/8);
  in.capacity = in.dataLength;
}



// Inversion function
void Transform::inv( RawTile& in ){

  uint32_t np = (uint32_t) in.width * in.height * in.channels;
  float *infptr = (float*) in.data;

  // Loop through our pixels for floating values
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
  for( uint32_t n=0; n<np; n++ ){
    float v = infptr[n];
    infptr[n] = 1.0 - v;
  }
}



// Resize image using nearest neighbour interpolation
void Transform::interpolate_nearestneighbour( RawTile& in, unsigned int resampled_width, unsigned int resampled_height ){

  // Pointer to input buffer
  unsigned char *input = (unsigned char*) in.data;

  int channels = in.channels;
  unsigned int width = in.width;
  unsigned int height = in.height;

  // Pointer to output buffer
  unsigned char *output;

  // Create new buffer if size is larger than input size
  bool new_buffer = false;
  if( resampled_width*resampled_height > in.width*in.height ){
    new_buffer = true;
    output = new unsigned char[(uint32_t)resampled_width*resampled_height*in.channels];
  }
  else output = (unsigned char*) in.data;

  // Calculate our scale
  float xscale = (float)width / (float)resampled_width;
  float yscale = (float)height / (float)resampled_height;

  // Do not parallelize for small images (256x256 pixels) as this can be slower that single threaded.
  // Also, only parallelize when resampling to a larger image as our output buffer then never
  // aliases our input buffer and can be safely written to by multiple threads
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( new_buffer && resampled_width*resampled_height > PARALLEL_THRESHOLD )
#endif
  for( unsigned int j=0; j<resampled_height; j++ ){

    // Index to the current pyramid resolution's top left pixel
    uint32_t jj = (uint32_t) floorf(j*yscale);

    for( unsigned int i=0; i<resampled_width; i++ ){

      // Indexes in the current pyramid resolution and resampled spaces
      // Make sure to limit our input index to the image surface
      uint32_t ii = (uint32_t) floorf(i*xscale);
      uint32_t pyramid_index = (uint32_t) channels * ( ii + jj*width );

      uint32_t resampled_index = (uint32_t)(i + j*resampled_width)*channels;
      for( uint32_t k=0; k<(uint32_t)in.channels; k++ ){
	output[resampled_index+k] = input[pyramid_index+k];
      }
    }
  }

  // Delete original buffer
  if( new_buffer ) delete[] (unsigned char*) input;

  // Correctly set our Rawtile info
  in.width = resampled_width;
  in.height = resampled_height;
  in.dataLength = (uint32_t) resampled_width * resampled_height * channels * (in.bpc/8);
  in.capacity = in.dataLength;
  in.data = output;
}



// Resize image using bilinear interpolation
//  - Floating point implementation which benchmarks about 2.5x slower than nearest neighbour
void Transform::interpolate_bilinear( RawTile& in, unsigned int resampled_width, unsigned int resampled_height ){

  // Pointer to input buffer
  unsigned char *input = (unsigned char*) in.data;

  int channels = in.channels;
  unsigned int width = in.width;
  unsigned int height = in.height;

  // Define a max index position on the input buffer
  uint32_t max = (uint32_t) ( (width*height) - 1 ) * channels;

  // Create new buffer and pointer for our output - make sure we have enough digits via unsigned long long
  unsigned char *output = new unsigned char[(uint32_t)resampled_width*resampled_height*channels];

  // Calculate our scale
  float xscale = (float)(width) / (float)resampled_width;
  float yscale = (float)(height) / (float)resampled_height;


  // Do not parallelize for small images (256x256 pixels) as this can be slower that single threaded
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( resampled_width*resampled_height > PARALLEL_THRESHOLD )
#endif
  for( unsigned int j=0; j<resampled_height; j++ ){

    // Index to the current pyramid resolution's top left pixel
    int jj = (int) floor( j*yscale );

    // Calculate some weights - do this in the highest loop possible
    float jscale = j*yscale;
    float c = (float)(jj+1) - jscale;
    float d = jscale - (float)jj;

    for( unsigned int i=0; i<resampled_width; i++ ){

      // Index to the current pyramid resolution's top left pixel
      int ii = (int) floor( i*xscale );

      // Calculate the indices of the 4 surrounding pixels
      uint32_t p11, p12, p21, p22;
      uint32_t jj_w = (uint32_t) jj*width;
      p11 = (uint32_t) ( channels * ( ii + jj_w ) );
      p12 = (uint32_t) ( channels * ( ii + (jj_w+width) ) );
      p21 = (uint32_t) ( channels * ( (ii+1) + jj_w ) );
      p22 = (uint32_t) ( channels * ( (ii+1) + (jj_w+width) ) );

      // Make sure we don't stray outside our input buffer boundary
      // - replicate at the edge
      p12 = (p12<=max)? p12 : max;
      p21 = (p21<=max)? p21 : max;
      p22 = (p22<=max)? p22 : max;

      // Calculate the rest of our weights
      float iscale = i*xscale;
      float a = (float)(ii+1) - iscale;
      float b = iscale - (float)ii;

      // Output buffer index
      uint32_t resampled_index = (uint32_t)( (j*resampled_width + i) * channels );

      for( uint32_t k=0; k<(uint32_t)channels; k++ ){
	float tx = input[p11+k]*a + input[p21+k]*b;
	float ty = input[p12+k]*a + input[p22+k]*b;
	unsigned char r = (unsigned char)( c*tx + d*ty );
	output[resampled_index+k] = r;
      }
    }
  }

  // Delete original buffer
  delete[] (unsigned char*) input;

  // Correctly set our Rawtile info
  in.width = resampled_width;
  in.height = resampled_height;
  in.dataLength = (uint32_t) resampled_width * resampled_height * channels * (in.bpc/8);
  in.capacity = in.dataLength;
  in.data = output;
}



// Fast efficient scaling of higher fixed point bit depths to 8 bit
void Transform::scale_to_8bit( RawTile& in ){

  // Skip floating point data and data already in 8 bit form
  if( in.bpc == 8 || in.sampleType == SampleType::FLOATINGPOINT ) return;

  uint32_t np = (uint32_t) in.width * in.height * in.channels;
  unsigned char* buffer = new unsigned char[np];

  // 32 bit fixed point integer
  if( in.bpc == 32 && in.sampleType == SampleType::FIXEDPOINT ){
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
    for( uint32_t n=0; n<np; n++ ){
      buffer[n] = (unsigned char)(((unsigned int*)in.data)[n] >> 16);
    }
    delete[] (unsigned int*) in.data;
  }

  // 16 bit unsigned short
  else if( in.bpc == 16 ){
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
    for( uint32_t n=0; n<np; n++ ){
      buffer[n] = (unsigned char)(((unsigned short*)in.data)[n] >> 8);
    }
    delete[] (unsigned short*) in.data;
  }

  // Replace original buffer with new 8 bit data
  in.data = buffer;
  in.bpc = 8;
  in.sampleType = SampleType::FIXEDPOINT;
  in.dataLength = np;
  in.capacity = np;
}



// Function to apply a contrast adjustment and convert to 8 bit
void Transform::contrast( RawTile& in, float contrast ){

  uint32_t np = (uint32_t) in.width * in.height * in.channels;
  unsigned char* buffer = new unsigned char[np];
  float* infptr = (float*)in.data;
  const float max8 = 255.0;     // Max pixel value for 8 bit data
  float cmax = max8 * contrast; // Set variable to avoid loop multiplication

#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
  for( uint32_t n=0; n<np; n++ ){
    float v = infptr[n] * cmax;
    buffer[n] = (unsigned char)( (v<max8) ? (v<0.0? 0.0f : v) : max8 );
  }

  // Replace original buffer with new
  delete[] (float*) in.data;
  in.data = buffer;
  in.bpc = 8;
  in.sampleType = SampleType::FIXEDPOINT;
  in.dataLength = np;
  in.capacity = np;
}



// Gamma correction (exponential transform): out = in * exp(g)
void Transform::gamma( RawTile& in, float g ){

  if( g == 1.0 ) return;

  uint32_t np = (uint32_t) in.width * in.height * in.channels;
  float* infptr = (float*)in.data;

  // Loop through our pixels for floating values
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
  for( uint32_t n=0; n<np; n++ ){
    float v = infptr[n];
    infptr[n] = powf( v<0.0 ? 0.0f : v, g );
  }
}



// Apply log transform: out = c log( 1 + in )
void Transform::log( RawTile& in ){

  // Need to handle input scale appropriately - log between 0-1 more linear than 0-255
  // - assume only 8 bit output for now
  float max = 255.0;

  // Scale factor
  float scale = 1.0 / logf( max + 1.0 );

  uint32_t np = (uint32_t) in.width * in.height * in.channels;

#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
  for( uint32_t i=0; i<np; i++ ){
    float v = ((float*)in.data)[i] * max;
    ((float*)in.data)[i] = scale * logf( 1.0 + v );
  }
}



// Rotation function
void Transform::rotate( RawTile& in, float angle=0.0 ){

  // Currently implemented only for rectangular rotations
  if( (int)angle % 90 == 0 && (int)angle % 360 != 0 ){

    // Allocate memory for our temporary buffer - rotate function only ever operates on 8bit data
    void *buffer = new unsigned char[in.width*in.height*in.channels];

    // Rotate 90
    if( (int) angle % 360 == 90 ){
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
      for( unsigned int i=0; i < in.width; i++ ){
	unsigned int n = i*in.height*in.channels;
	for( int j=in.height-1; j>=0; j-- ){
	  unsigned int index = (in.width*j + i)*in.channels;
	  for( int k=0; k < in.channels; k++ ){
	    ((unsigned char*)buffer)[n++] = ((unsigned char*)in.data)[index+k];
	  }
	}
      }
    }

    // Rotate 270
    else if( (int) angle % 360 == 270 ){
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
      for( int i=in.width-1; i>=0; i-- ){
	unsigned int n = (in.width-1-i)*in.height*in.channels;
	for( unsigned int j=0; j<in.height; j++ ){
	  unsigned int index = (in.width*j + i)*in.channels;
	  for( int k=0; k < in.channels; k++ ){
	    ((unsigned char*)buffer)[n++] = ((unsigned char*)in.data)[index+k];
	  }
	}
      }
    }

    // Rotate 180
    else if( (int) angle % 360 == 180 ){
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
      for( int i=(in.width*in.height)-1; i >= 0; i-- ){
	unsigned int index  = i * in.channels;
	unsigned int oindex = (in.width*in.height - 1 - i) * in.channels;
	for( int k=0; k < in.channels; k++ ){
	  ((unsigned char*)buffer)[oindex+k] = ((unsigned char*)in.data)[index+k];
	}
      }
    }

    // Delete old data buffer
    delete[] (unsigned char*) in.data;

    // Assign new data to Rawtile
    in.data = buffer;

    // For 90 and 270 rotation swap width and height
    if( (int)angle % 180 == 90 ){
      unsigned int tmp = in.height;
      in.height = in.width;
      in.width = tmp;
    }
  }
}



// Convert colour to grayscale using the conversion formula:
//   Luminance = 0.2126*R + 0.7152*G + 0.0722*B
// Note that we don't linearize before converting
void Transform::greyscale( RawTile& rawtile ){

  if( rawtile.bpc != 8 || rawtile.channels != 3 ) return;

  uint32_t np = (uint32_t) rawtile.width * rawtile.height;
  unsigned char* buffer = new unsigned char[rawtile.width * rawtile.height];

  // Calculate using fixed-point arithmetic
  //  - benchmarks to around 25% faster than floating point
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( rawtile.width*rawtile.height > PARALLEL_THRESHOLD )
#endif
  for( uint32_t i=0; i<np; i++ ){
    unsigned int n = i*rawtile.channels;
    unsigned char R = ((unsigned char*)rawtile.data)[n++];
    unsigned char G = ((unsigned char*)rawtile.data)[n++];
    unsigned char B = ((unsigned char*)rawtile.data)[n++];
    buffer[i] = (unsigned char)( ( 1254097*R + 2462056*G + 478151*B ) >> 22 );
  }

  // Delete our old data buffer and instead point to our grayscale data
  delete[] (unsigned char*) rawtile.data;
  rawtile.data = (void*) buffer;

  // Update our number of channels and data length
  rawtile.channels = 1;
  rawtile.dataLength = np;
  rawtile.capacity = np;
}



// Apply twist or channel recombination to colour or multi-channel image.
// Matrix is a vector of vectors. Each top level vector represents a row and
// contains the output transform for one output channel. Each lower level vector
// represents the columns and maps to the raw data and should contain as many
// values as there are channels in the raw input data
void Transform::twist( RawTile& rawtile, const vector< vector<float> >& matrix ){

  uint32_t np = (uint32_t) rawtile.width * rawtile.height;

  // Determine the number of rows and, therefore, output channels (this can be different to the number of input channels)
  int output_channels = matrix.size();

  // Output buffer
  float *output = NULL;

  // If we are creating an image with different number of output channels, create a new data buffer
  if( output_channels == rawtile.channels ) output = (float*) rawtile.data;
  else output = new float[np * output_channels];

  // Prepare flattened matrix and row sizes for efficient access
  std::vector<unsigned int> row_sizes( output_channels );
  std::vector<float> flat_matrix( output_channels * rawtile.channels, 0.0f );

  for( int k = 0; k < output_channels; ++k ){
    unsigned int cols = std::min( matrix[k].size(), (size_t) rawtile.channels );
    row_sizes[k] = cols;
    std::copy( matrix[k].begin(), matrix[k].begin() + cols, flat_matrix.begin() + k * rawtile.channels );
  }

#pragma omp parallel if( rawtile.width*rawtile.height > PARALLEL_THRESHOLD )
  {
    // Create temporary buffer for our calculated values (need one for each thread)
    float* pixel = new float[output_channels];

    #pragma omp for
    for( uint32_t i=0; i<np; i++ ){

      // Pointer to input data
      const float* in = ((float*)rawtile.data) + i*rawtile.channels;

      // Compute all output channels into the temporary pixel buffer
      for( int k=0; k<output_channels; k++ ){
	float sum = 0.0f;
	const float* row = &flat_matrix[k * rawtile.channels];
	unsigned int sz = row_sizes[k];

        #pragma omp simd reduction(+:sum)
	for( unsigned int j = 0; j < sz; j++ ){
	  sum += in[j] * row[j];
	}
	pixel[k] = sum;
      }

      // Write the computed pixel values to the output buffer
      float* out = output + i*output_channels;
      for( int k=0; k<output_channels; k++ ){
	out[k] = pixel[k];
      }
    }

    delete[] pixel;
  }

  // If we have a different number of output channels, swap our buffer and update our rawtile parameters
  if( output_channels != rawtile.channels ){
    delete[] (float*) rawtile.data;
    rawtile.data = output;
    rawtile.channels = output_channels;
    rawtile.dataLength = (uint32_t) np * rawtile.channels * (rawtile.bpc/8);
    rawtile.capacity = rawtile.dataLength;
  }

}



// Flatten a multi-channel image to a given number of bands by simply stripping
// away extra bands
void Transform::flatten( RawTile& in, int bands ){

  // We cannot increase the number of channels
  if( bands >= in.channels ) return;

  uint32_t np = (uint32_t) in.width * in.height;
  uint32_t ni = 0;
  uint32_t no = 0;
  unsigned int gap = in.channels - bands;

  // Simply loop through assigning to the same buffer
  for( uint32_t i=0; i<np; i++ ){
    for( int k=0; k<bands; k++ ){
      ((unsigned char*)in.data)[ni++] = ((unsigned char*)in.data)[no++];
    }
    no += gap;
  }

  in.channels = bands;
  in.dataLength = ni * (in.bpc/8);
  // Note that we haven't changed our data buffer, so no change in capacity
}



// Flip image in horizontal or vertical direction (0=horizontal,1=vertical)
void Transform::flip( RawTile& rawtile, int orientation ){

  unsigned char* buffer = new unsigned char[rawtile.width * rawtile.height * rawtile.channels];

  // Vertical
  if( orientation == 2 ){
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( rawtile.width*rawtile.height > PARALLEL_THRESHOLD )
#endif
    for( int j=rawtile.height-1; j>=0; j-- ){
      uint32_t n = (uint32_t) (rawtile.height-1-j)*rawtile.width*rawtile.channels;
      for( unsigned int i=0; i<rawtile.width; i++ ){
        uint32_t index = (uint32_t) (rawtile.width*j + i)*rawtile.channels;
        for( int k=0; k<rawtile.channels; k++ ){
          buffer[n++] = ((unsigned char*)rawtile.data)[index++];
        }
      }
    }
  }
  // Horizontal
  else{
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( rawtile.width*rawtile.height > PARALLEL_THRESHOLD )
#endif
    for( unsigned int j=0; j<rawtile.height; j++ ){
      uint32_t n = (uint32_t) j*rawtile.width*rawtile.channels;
      for( int i=rawtile.width-1; i>=0; i-- ){
        uint32_t index = (uint32_t) (rawtile.width*j + i)*rawtile.channels;
        for( int k=0; k<rawtile.channels; k++ ){
	  buffer[n++] = ((unsigned char*)rawtile.data)[index++];
        }
      }
    }
  }

  // Delete our old data buffer and instead point to our grayscale data
  delete[] (unsigned char*) rawtile.data;
  rawtile.data = (void*) buffer;
}



// Calculate histogram of an image
//  - Only calculate for 8 bits and a single histogram for all channels
vector<unsigned int> Transform::histogram( RawTile& in, const vector<float>& max, const vector<float>& min ){

  // An 8 bit (256 level) histogram should be sufficient
  if( in.bpc > 8 ){
    this->normalize( in, max, min );
    this->contrast( in, 1.0 );
  }

  // Initialize our vector to zero - note that we use a single histogram for all channels
  vector<unsigned int> histogram( (1<<in.bpc), 0 );

  // Fill our histogram - for color or multiband images, use channel average
  uint32_t np = (uint32_t) in.width * in.height;
  for( uint32_t n=0; n<np; n++ ){
    float value = 0.0f;

    // For color or multiband images, use channel average
    for( int k=0; k<in.channels; k++ ){
      value += (float)(((unsigned char*)in.data)[n*in.channels + k]);
    }
    value = round( value/(float)in.channels );

    // Update histogram
    histogram[(unsigned int)value]++;
  }

  return histogram;
}



// Calculate the threshold for binary image segmentation using Otsu's method
unsigned char Transform::threshold( vector<unsigned int>& histogram ){

  const unsigned int bits = histogram.size();

  // Calculate sum
  float sum = 0.0, sumb = 0.0;
  uint32_t np = 0;
  for( uint32_t n=0; n<bits; n++ ){
    np += histogram[n];
    sum += (float)n * histogram[n];
  }

  // Calculate threshold
  float wb = 0.0, wf = 0.0, mb = 0.0, mf = 0.0, max = 0.0;
  unsigned char otsu = 0;
  for( unsigned int n=0; n<bits; n++ ){
    wb += histogram[n];
    if( wb == 0.0 ) continue;

    wf = np - wb;
    if( wf == 0.0 ) break;

    sumb += (float) n * histogram[n];
    mb = sumb / wb;
    mf = (sum-sumb) / wf;
    float diff = wb * wf * (mb-mf) * (mb-mf);

    if( diff > max ){
      otsu = (unsigned char) n;
      max = diff;
    }
  }
  return otsu;
}



// Apply threshold to create binary image
void Transform::binary( RawTile &in, unsigned char threshold ){

  // Only apply to 8 bit images
  if( in.bpc != 8 ) return;

  // First make sure our image is greyscale
  this->greyscale( in );

  uint32_t np = (uint32_t) in.width * in.height;

#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
  for( uint32_t i=0; i<np; i++ ){
    ((unsigned char*)in.data)[i] = ( ((unsigned char*)in.data)[i] < threshold ? (unsigned char)0 : (unsigned char)255 );
  }
}



void Transform::equalize( RawTile& in, vector<unsigned int>& histogram ){

  uint32_t np = (uint32_t) in.width * in.height;

  // Number of levels in our histogram
  const unsigned int bits = histogram.size();

  // Allocate and initialize our array to zero using std::fill
  float *cdf = new float[bits];
  fill( cdf, cdf+bits, 0.0f );

  // Find first non-zero bin
  unsigned int n0 = 0;
  while( histogram[n0] == 0 ) ++n0;

  // Calculate cumulative histogram
  cdf[0] = histogram[0];
  for( unsigned int i=1; i<bits; i++ ){
    cdf[i] = cdf[i-1] + histogram[i];
  }

  // Scale our CDF
  float scale = (float)(bits-1) / cdf[bits-1];
  float cdfmin = cdf[n0] / (float)(in.width*in.height);
  for( unsigned int i=0; i<bits; i++ ){
    cdf[i] = round( scale * (cdf[i]-cdfmin) );
  }

  // Map image through cumulative histogram
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
  for( uint32_t i=0; i<np; i++ ){
    for( int j=0; j<in.channels; j++ ){
      uint32_t index = (uint32_t) i*in.channels + j;
      unsigned int value = (unsigned int) (((unsigned char*)in.data)[index]);
      ((unsigned char*)in.data)[index] = (unsigned char) cdf[value];
    }
  }

  // Free our dynamically allocated array
  delete[] cdf;
}



// Apply convolution kernel to image
void Transform::convolution( RawTile& in, const vector<float>& kernel ){

  float* data = (float*)in.data;
  float* buffer = new float[in.width * in.height * in.channels];

  unsigned int ksize = kernel.size();
  unsigned int side = (unsigned int) sqrtf( ksize );
  int half_side = side / 2;

  // Sum kernel elements using STL accumulate function
  float total = accumulate( kernel.begin(), kernel.end(), 0.0 );

#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for collapse(2) if( in.width*in.height > PARALLEL_THRESHOLD )
#endif
  for( unsigned long y=0; y<in.height; y++ ){
    for( unsigned long x=0; x<in.width; x++ ){
      for( int c=0; c < in.channels; c++) {
	uint32_t n = ((y*in.width) + x) * in.channels + c;
        double v = 0;

	for( unsigned int fy=0; fy<side; fy++ ){
	  // Note that we extend at the edges
	  // Calculate pixel y location on image
	  long iny = y + fy - half_side;
	  if( iny < 0 ) iny = 0;
	  else if( iny >= in.height ) iny = in.height - 1;

          for( unsigned int fx=0; fx<side; fx++ ){
	    // Calculate pixel x location on image
	    long inx = x + fx - half_side;
	    if( inx < 0 ) inx = 0;
	    else if( inx >= in.width ) inx = in.width - 1;

	    long i = (inx + (iny * in.width)) * in.channels + c;
            v += data[i] * kernel[(fy*side) + fx];
          }
        }
	// Normalize our values
	if( total > 0.0 ) v = v / total;
	buffer[n] = (float) v;
      }
    }
  }

  delete[] (float*) in.data;
  in.data = (void*) buffer;
}



// Calculate per-channel mean
vector<float> Transform::mean( const RawTile& in ){

  // Create vector and initialize values to zero
  vector<float> mean( in.channels, 0 );

  uint32_t np = (uint32_t) in.width * in.height * in.channels;
  uint32_t pixels = in.width * in.height;

  // Sum the pixel values for each channel
  /*#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif*/
  for( uint32_t n = 0; n < np; n += in.channels ){
    for( int k = 0; k < in.channels; k++ ){
      mean[k] += (float)((float*)in.data)[n+k];
    }
  }

  // Divide to obtain mean for each channel
  for( int k = 0; k < in.channels; k++ ){
    mean[k] /= (float) pixels;
  }

  return mean;
}



// Calculate covariance between channels
vector<float> Transform::covariance( const RawTile& in, const vector<float>& means ){

  // Number of pixels – guard against division-by-zero
  uint32_t np = (uint32_t) in.width * in.height;
  if( np <= 1 ) return vector<float>( in.channels * in.channels, 0.0f );

  // Global accumulator: double precision to minimize rounding error
  vector<double> globalCov( in.channels * in.channels, 0.0 );

  // The tile data has already been normalised to float
  const float *data = (float *)in.data;

  // Parallel region – each thread gets its own private covariance matrix
#pragma omp parallel if( in.width * in.height > PARALLEL_THRESHOLD )
  {
    vector<double> localCov( in.channels * in.channels, 0.0 );

    // Distribute pixels across threads
    #pragma omp for nowait
    for( uint32_t n = 0; n < np; n++ ){

      const float *pixel = data + n * in.channels;

      // Centered values – accumulate only the upper triangle
      for( int i = 0; i < in.channels; i++ ){
	float vi = pixel[i] - means[i];

        #pragma omp simd
	for( int j = i; j < in.channels; j++ ){
	  float vj = pixel[j] - means[j];
	  localCov[i * in.channels + j] += (double)vi * vj;
	}
      }
    }

    // Reduce thread-local matrices into the global one
#pragma omp critical
    for( size_t k = 0; k < globalCov.size(); k++ ){
      globalCov[k] += localCov[k];
    }
  }

  // Normalise by (N-1) and copy to a float vector, preserving symmetry
  vector<float> covariance( in.channels * in.channels, 0.0f );
  double denom = (double)(np - 1);
  for( int i = 0; i < in.channels; i++ ){
    for( int j = i; j < in.channels; j++ ){
      float val = (float)(globalCov[i * in.channels + j] / denom);
      covariance[i * in.channels + j] = val;
      covariance[j * in.channels + i] = val;   // matrix is symmetric
    }
  }

  return covariance;
}



/// Calculate Eigenvalues and Eigenvectors. These are returned in a single STL vector where
/// vector[0] are the eigenvalues and vector[1] are the eigenvectors. The eigenvectors are stored row-wise
/// and not column-wise
vector<vector<float>> Transform::eigenvectors( const vector<float>& covariance ){

  int channels = (int) sqrtf( covariance.size() );
  vector<float> values( channels, 0.0 );
  vector<float> vectors( channels*channels, 0.0 );

#ifdef HAVE_EIGEN3

  // Copy our covariance matrix to an Eigen matrix structure
  Eigen::MatrixXf cov( channels, channels );
  unsigned int n = 0;
  for( int j=0; j<channels; j++ ){
    for( int i=0; i<channels; i++ ){
      cov(i,j) = covariance[n++];
    }
  }

  // Solve to find eigenvalues and vectors - use fast solver for symmetric matrices
  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXf> eigenValueSolver( cov );

  // Get Eigenvalues and load into vector
  Eigen::MatrixXf v = eigenValueSolver.eigenvalues();
  values = vector<float>( v.data(), v.data() + v.size() );

  // Eigen's solver stores eigenvectors as columns, and MatrixXf is column-major.
  // Therefore, each eigenvector k is already stored contiguously at offset [k * channels + i].
  Eigen::MatrixXf eigenvectors = eigenValueSolver.eigenvectors();

  // Make sure largest value in each eigenvector is positive
  for( Eigen::Index k = 0; k < eigenvectors.cols(); ++k ){
    // Find the coefficient with the largest absolute value
    Eigen::Index maxIndex;
    eigenvectors.col(k).cwiseAbs().maxCoeff(&maxIndex);
    // Ensure that coefficient is positive
    if (eigenvectors(maxIndex, k) < 0.0) eigenvectors.col(k) *= -1.0;
  }

  vectors = vector<float>( eigenvectors.data(), eigenvectors.data() + eigenvectors.size() );

#endif

  // Pack both eigen values and eigen vectors into single STL vector
  // with the first array item the values and the second the eigenvectors
  vector<vector<float>> eigen = {values,vectors};
  return eigen;
}



// Decorrelation stretch on N-band image
void Transform::decorrelation( RawTile& in, const vector<vector<float>>& eigenvectors,
                               const vector<float>& mean, const vector<float>& covariance ){

  // Basic sanity check – data must exist and there must be at least one channel
  if( !in.data || in.channels <= 0 ) return;
  if( in.sampleType != SampleType::FLOATINGPOINT || in.bpc != 32 ) return;

  const int channels = in.channels;

  // Verify that the supplied statistics match the channel count
  if( eigenvectors.size() < 2 ||
      eigenvectors[0].size() < (size_t)channels ||
      eigenvectors[1].size() < (size_t)channels * channels ||
      mean.size()        < (size_t)channels ||
      covariance.size()  < (size_t)channels * channels )
    return;

  const vector<float>& values  = eigenvectors[0];   // eigenvalues (size channels)
  const vector<float>& vectors = eigenvectors[1];   // eigenvectors (row‑wise, channels×channels)

  // Standard deviation per channel (sqrt of diagonal of covariance)
  vector<float> stddev( channels );
  for( int k = 0; k < channels; ++k ){
    float var = covariance[k * channels + k];
    stddev[k] = (var > 0.0f) ? sqrtf(var) : 0.0f;
  }

  // Scale factors - 1 / sqrt(eigenvalue) (whitening)
  vector<float> scale( channels );
  for( int k = 0; k < channels; ++k ){
    scale[k] = (values[k] > 1e-6f) ? (1.0f / sqrtf(values[k])) : 0.0f;
  }

  // Build the N×N transformation matrix T = D·V·S·Vᵀ
  // Stored row‑major as a flat vector: T[row*N + col]
  vector<float> T( channels * channels, 0.0f );
  for( int j = 0; j < channels; ++j ){
    for( int i = 0; i < channels; ++i ){
      float sum = 0.0f;
      for( int k = 0; k < channels; ++k ){
        // Vectors are row‑wise: row k, column *
        sum += scale[k] * vectors[k * channels + j] * vectors[k * channels + i];
      }
      T[j * channels + i] = stddev[j] * sum;
    }
  }

  // Pre‑compute the constant bias vector: b = mean – T·mean
  vector<float> bias( channels );
  for( int j = 0; j < channels; ++j ){
    float dot = 0.0f;
    for( int i = 0; i < channels; ++i ){
      dot += T[j * channels + i] * mean[i];
    }
    bias[j] = mean[j] - dot;
  }

  uint32_t np = (uint32_t) in.width * in.height * channels;
  float *data = (float*) in.data;

  // Fast path for 3-channel RGB: eliminates inner loops and enables SIMD vectorization
  if( channels == 3 ){
    const float T0 = T[0], T1 = T[1], T2 = T[2];
    const float T3 = T[3], T4 = T[4], T5 = T[5];
    const float T6 = T[6], T7 = T[7], T8 = T[8];
    const float b0 = bias[0], b1 = bias[1], b2 = bias[2];

#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width * in.height > PARALLEL_THRESHOLD )
#endif
    for( uint32_t n = 0; n < np; n += 3 ){
      float r = data[n], g = data[n + 1], b = data[n + 2];
      float o0 = b0 + T0 * r + T1 * g + T2 * b;
      float o1 = b1 + T3 * r + T4 * g + T5 * b;
      float o2 = b2 + T6 * r + T7 * g + T8 * b;
      data[n]     = (o0 < 0.0f) ? 0.0f : (o0 > 1.0f ? 1.0f : o0);
      data[n + 1] = (o1 < 0.0f) ? 0.0f : (o1 > 1.0f ? 1.0f : o1);
      data[n + 2] = (o2 < 0.0f) ? 0.0f : (o2 > 1.0f ? 1.0f : o2);
    }
    return;
  }

  // Generic path for arbitrary channels (allocates buffer once per thread, no VLAs)
#pragma omp parallel if( in.width * in.height > PARALLEL_THRESHOLD )
  {
    vector<float> out( channels );

    #pragma omp for
    for( uint32_t n = 0; n < np; n += channels ){
      const float *pixel = data + n;
      for( int j = 0; j < channels; ++j ){
        float acc = bias[j];
        const float *Trow = &T[j * channels];
        for( int i = 0; i < channels; ++i ){
          acc += Trow[i] * pixel[i];
        }
        out[j] = (acc < 0.0f) ? 0.0f : (acc > 1.0f ? 1.0f : acc);
      }

      for( int i = 0; i < channels; ++i ){
        data[n + i] = out[i];
      }
    }
  }
}



// PCA Transform with global variance scaling
void Transform::pca( RawTile& in, const vector<vector<float>>& eigenvectors, const vector<float>& mean ){

  // Make sure we have valid image data and eigen values / vectors
  if( !in.data || in.channels <= 0 ) return;
  if( in.sampleType != SampleType::FLOATINGPOINT || in.bpc != 32 ) return;
  if( eigenvectors.size() < 2 || eigenvectors[0].empty() || eigenvectors[1].empty() ) return;

  const vector<float>& values  = eigenvectors[0];
  const vector<float>& vectors = eigenvectors[1];

  const int channels = in.channels;
  if( (int)values.size() < channels || (int)vectors.size() < channels * channels || (int)mean.size() < channels ){
    return;
  }

  // Sort component indices in descending order of eigenvalue (largest variance / PC1 first)
  vector<int> order( channels );
  for( int i = 0; i < channels; i++ ) order[i] = i;
  sort( order.begin(), order.end(), [&values]( int a, int b ){
    return values[a] > values[b];
  });

  // Precompute affine projection: out = P * x + bias
  // Using 2.75-sigma scaling (k=2.75) to map [-2.75*sigma, +2.75*sigma] to [0.0, 1.0]
  // (2.75 equates to approximately 0.6% percentile clipping - 0.3% at each low/high extremity)
  const float k_sigma = 2.75f;
  vector<float> P( channels * channels, 0.0f );
  vector<float> bias( channels, 0.0f );

  for( int j = 0; j < channels; j++ ){
    int vec_idx = order[j];
    float lambda = values[vec_idx];
    float scale = (lambda > 1e-6f) ? (1.0f / (2.0f * k_sigma * sqrtf(lambda))) : 0.0f;

    float dot_mean = 0.0f;
    for( int i = 0; i < channels; i++ ){
      float v = vectors[vec_idx * channels + i];
      P[j * channels + i] = v * scale;
      dot_mean += v * mean[i];
    }
    bias[j] = 0.5f - dot_mean * scale;
  }

  uint32_t np = (uint32_t) in.width * in.height * channels;
  float *data = (float*) in.data;

  // Fast path for 3-channel RGB: eliminates inner loops and enables SIMD vectorization
  if( channels == 3 ){
    const float P0 = P[0], P1 = P[1], P2 = P[2];
    const float P3 = P[3], P4 = P[4], P5 = P[5];
    const float P6 = P[6], P7 = P[7], P8 = P[8];
    const float b0 = bias[0], b1 = bias[1], b2 = bias[2];

#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( in.width * in.height > PARALLEL_THRESHOLD )
#endif
    for( uint32_t n = 0; n < np; n += 3 ){
      float r = data[n], g = data[n + 1], b = data[n + 2];
      float o0 = b0 + P0 * r + P1 * g + P2 * b;
      float o1 = b1 + P3 * r + P4 * g + P5 * b;
      float o2 = b2 + P6 * r + P7 * g + P8 * b;
      data[n]     = (o0 < 0.0f) ? 0.0f : (o0 > 1.0f ? 1.0f : o0);
      data[n + 1] = (o1 < 0.0f) ? 0.0f : (o1 > 1.0f ? 1.0f : o1);
      data[n + 2] = (o2 < 0.0f) ? 0.0f : (o2 > 1.0f ? 1.0f : o2);
    }
    return;
  }

  // Generic path for arbitrary channels (allocates buffer once per thread)
#pragma omp parallel if( in.width * in.height > PARALLEL_THRESHOLD )
  {
    vector<float> out( channels );

    #pragma omp for
    for( uint32_t n = 0; n < np; n += channels ){
      const float *pixel = data + n;
      for( int j = 0; j < channels; ++j ){
        float acc = bias[j];
        const float *Prow = &P[j * channels];
        for( int i = 0; i < channels; ++i ){
          acc += Prow[i] * pixel[i];
        }
        out[j] = (acc < 0.0f) ? 0.0f : (acc > 1.0f ? 1.0f : acc);
      }

      for( int i = 0; i < channels; ++i ){
        data[n + i] = out[i];
      }
    }
  }
}
