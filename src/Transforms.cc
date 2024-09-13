// Image Transform Functions

/*  IIPImage image processing routines

    Copyright (C) 2004-2024 Ruven Pillay.

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
#include "Transforms.h"


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



/* D65 temp 6504.
 */
#define D65_X0 95.0470
#define D65_Y0 100.0
#define D65_Z0 108.8827

/* Size threshold for using parallel loops (256x256 pixels)
 */
#define PARALLEL_THRESHOLD 65536


static const float _sRGB[3][3] = { { 3.2406255, -1.537208, -0.4986286},
				   {-0.9689307,  1.8757561, 0.0415175},
				   { 0.0557101, -0.2040211, 1.0569959} };

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
  in.dataLength = (uint32_t) np * (in.bpc/8);
  in.capacity = in.dataLength;

}



// Hillshading function
void Transform::shade( RawTile& in, int h_angle, int v_angle ){

  float o_x, o_y, o_z;

  // Incident light angle
  float a = (h_angle * 2 * M_PI) / 360.0;

  // We assume a hypotenous of 1.0
  float s_y = cos(a);
  float s_x = sqrt( 1.0 - s_y*s_y );
  if( h_angle > 180 ){
    s_x = -s_x;
  }

  a = (v_angle * 2 * M_PI) / 360.0;
  float s_z = - sin(a);

  float s_norm = sqrt( s_x*s_x + s_y*s_y + s_z*s_z );
  s_x = s_x / s_norm;
  s_y = s_y / s_norm;
  s_z = s_z / s_norm;

  float *buffer, *infptr;

  uint32_t np = (uint32_t) in.width * in.height * in.channels;

  infptr= (float*)in.data;

  // Create new (float) data buffer
  buffer = new float[np];


#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
  for( uint32_t k=0; k<np; k++ ){

    unsigned int n = k*3;
    if( infptr[n] == 0.0 && infptr[n+1] == 0.0 && infptr[n+2] == 0.0 ){
      o_x = o_y = o_z = 0.0;
    }
    else {
      o_x = (float) - ((float)infptr[n]-0.5) * 2.0;
      o_y = (float) - ((float)infptr[n+1]-0.5) * 2.0;
      o_z = (float) - ((float)infptr[n+2]-0.5) * 2.0;
    }

    float dot_product;
    dot_product = (s_x*o_x) + (s_y*o_y) + (s_z*o_z);

    dot_product = 0.5 * dot_product;
    if( dot_product < 0.0 ) dot_product = 0.0;
    if( dot_product > 1.0 ) dot_product = 1.0;

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
void Transform::LAB2sRGB( unsigned char *in, unsigned char *out ){

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
  R = (R<0.0 ? 0.0 : R);
  G = (G<0.0 ? 0.0 : G);
  B = (B<0.0 ? 0.0 : B);

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
  R = (R>255.0 ? 255.0 : R);
  G = (G>255.0 ? 255.0 : G);
  B = (B>255.0 ? 255.0 : B);

  /* Return our sRGB values
   */
  out[0] = (unsigned char) R;
  out[1] = (unsigned char) G;
  out[2] = (unsigned char) B;

}



// Convert whole tile from CIELAB to sRGB
void Transform::LAB2sRGB( RawTile& in ){

  uint32_t np = (uint32_t) in.width * in.height * in.channels;

  // Parallelize code using OpenMP
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
  for( uint32_t n=0; n<np; n+=in.channels ){
    unsigned char* ptr = (unsigned char*) in.data;
    unsigned char q[3];
    LAB2sRGB( &ptr[n], &q[0] );
    ((unsigned char*)in.data)[n] = q[0];
    ((unsigned char*)in.data)[n+1] = q[1];
    ((unsigned char*)in.data)[n+2] = q[2];
  }
}



// Colormap function
// Based on the routine colormap.cpp in Imagin Raytracer by Olivier Ferrand
// http://www.imagin-raytracer.org
void Transform::cmap( RawTile& in, enum cmap_type cmap ){

  float value;
  unsigned in_chan = in.channels;
  unsigned out_chan = 3;
  uint32_t np = (uint32_t) in.width * in.height * in.channels; // pixels in input image
  uint32_t onp = (uint32_t) in.width * in.height * out_chan;   // pixels in output image

  const float max3 = 1.0/3.0;
  const float max8 = 1.0/8.0;

  float *fptr = (float*)in.data;
  float *outptr = new float[onp];
  float *outv = outptr;

  switch(cmap){

    case HOT:
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#endif
      for( uint32_t n=0; n<np; n+=in_chan, outv+=3 ){
        value = fptr[n];
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
#endif
      for( uint32_t n=0; n<np; n+=in_chan, outv+=3 ){
        value = fptr[n];
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
#endif
      for( uint32_t n=0; n<np; n+=in_chan, outv+=3 ){
        value = fptr[n];
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
#endif
      for( uint32_t n=0; n<np; n+=in_chan, outv+=3 ){
	value = fptr[n];
	outv[0] = value;
	outv[1] = outv[2] = 0.;
      }
      break;

    case GREEN:
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#endif
      for( uint32_t n=0; n<np; n+=in_chan, outv+=3 ) {
	value = fptr[n];
	outv[0] = outv[2] = 0.;
	outv[1] = value;
      }
      break;

    case BLUE:
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#endif
      for( uint32_t n=0; n<np; n+=in_chan, outv+=3 ) {
	value = fptr[n];
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

  for( unsigned int j=0; j<resampled_height; j++ ){
    for( unsigned int i=0; i<resampled_width; i++ ){

      // Indexes in the current pyramid resolution and resampled spaces
      // Make sure to limit our input index to the image surface
      uint32_t ii = (uint32_t) floorf(i*xscale);
      uint32_t jj = (uint32_t) floorf(j*yscale);
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

    // Initialize our counter
    unsigned int n = 0;

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
      for( int i=(in.width*in.height)-1; i >= 0; i-- ){
	unsigned index = i * in.channels;
	for( int k=0; k < in.channels; k++ ){
	  ((unsigned char*)buffer)[n++] = ((unsigned char*)in.data)[index+k];
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

  // Need to make sure the matrix is adapted to the number of channels in the raw data
  unsigned int* row_sizes = new unsigned int[output_channels]; // Number of columns for each row

  // Get the number of columns in each row - can be less than number of channels - limit if there are more
  for( int k=0; k<output_channels; k++ ){
    row_sizes[k] = (matrix[k].size()>(unsigned int)rawtile.channels) ? rawtile.channels : matrix[k].size();
  }

#pragma omp parallel if( rawtile.width*rawtile.height > PARALLEL_THRESHOLD )
  {
    // Create temporary buffer for our calculated values (need one for each thread)
    float* pixel = new float[output_channels];

    // Loop through each pixel
#pragma omp for
    for( uint32_t i=0; i<np; i++ ){

      uint32_t in = (uint32_t) i*rawtile.channels;  // Pixel index in input buffer
      uint32_t on = (uint32_t) i*output_channels;   // Pixel index in output buffer

      // Calculate pixel value for each output channel
      for( int k=0; k<output_channels; k++ ){

	// First zero our pixel buffer
	pixel[k] = 0.0f;

	// Loop through each column (input channel coefficient ) in the matrix
	for( unsigned int j=0; j<row_sizes[k]; j++ ){
	  float m = matrix[k][j];
	  if( m ){
	    float p = ((float*)rawtile.data)[in+j];
	    pixel[k] += (m == 1.0) ? p : p * m;
	  }
	}
      }

      // Only write out to our buffer at the end as we reuse channel values several times during the twist loops
      for( int k=0; k<output_channels; k++ ) output[on++] = pixel[k];
    }

    delete[] pixel;
  }

  delete[] row_sizes;

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
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
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
#pragma omp parallel for
#endif
  for( unsigned long y=0; y<in.height; y++ ){
    for( unsigned long x=0; x<in.width; x++ ){
      for( int c=0; c < in.channels; c++) {
	uint32_t n = ((y*in.width) + x) * in.channels + c;
        double v = 0;
	for( unsigned int fy=0; fy<side; fy++ ){
	  // Note that we do wrapping at the edges
	  long iny = (y + fy - half_side + in.height) % in.height;
          for( unsigned int fx=0; fx<side; fx++ ){
            long inx = (x + fx - half_side + in.width) % in.width;
            long i = (inx + (iny * in.width)) * in.channels + c;
            v += data[i] * kernel[(fy*side) + fx];
          }
        }
	// Normalize our values
	if( total > 0.0 ) v = v / total;
	buffer[n++] = v;
      }
    }
  }

  delete[] (float*) in.data;
  in.data = (void*) buffer;
}
