// Image Transform Functions

/*  IIP fcgi server module - image processing routines

    Copyright (C) 2004-2016 Ruven Pillay.

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
#include "Filters.h"
#include "Timer.h"


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


static const float _sRGB[3][3] = { {  3.240479, -1.537150, -0.498535 },
				   { -0.969256, 1.875992, 0.041556 },
				   { 0.055648, -0.204043, 1.057311 } };

using namespace std;


// Normalization function
void filter_normalize( RawTile& in, vector<float>& max, vector<float>& min ) {

  float *normdata;
  unsigned int np = in.dataLength * 8 / in.bpc;
  unsigned int nc = in.channels;

  // Type pointers
  float* fptr;
  unsigned int* uiptr;
  unsigned short* usptr;
  unsigned char* ucptr;

  if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) {
    normdata = (float*)in.data;
  }
  else {
    normdata = new float[np];
  }

  for( unsigned int c = 0 ; c<nc ; c++){

    float minc = min[c];
    float diffc = max[c] - minc;
    float invdiffc = fabs(diffc) > 1e-30? 1./diffc : 1e30;

    // Normalize our data
    if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) {
      fptr = (float*)in.data;
      // Loop through our pixels for floating point pixels
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
      for( unsigned int n=c; n<np; n+=nc ){
        normdata[n] = isfinite(fptr[n])? (fptr[n] - minc) * invdiffc : 0.0;
      }
    }
    else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ) {
      uiptr = (unsigned int*)in.data;
      // Loop through our pixels for unsigned int pixels
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
      for( unsigned int n=c; n<np; n+=nc ){
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
      for( unsigned int n=c; n<np; n+=nc ){
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
      for( unsigned int n=c; n<np; n+=nc ){
        normdata[n] = (ucptr[n] - minc) * invdiffc;
      }
    }
  }

  // Delete our original buffers, unless we already had floats
  if( in.bpc == 32 && in.sampleType == FIXEDPOINT ){
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
  in.dataLength = np * in.bpc / 8;

}



// Hillshading function
void filter_shade( RawTile& in, int h_angle, int v_angle ){

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

  unsigned int ndata = in.dataLength * 8 / in.bpc;

  infptr= (float*)in.data;

  // Create new (float) data buffer
  buffer = new float[ndata];


#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
  for( unsigned int k=0; k<ndata; k++ ){

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
  in.dataLength = in.width * in.height * in.bpc / 8;
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
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
  for( unsigned long n=0; n<np; n+=in.channels ){
    unsigned char* ptr = (unsigned char*) in.data;
    unsigned char q[3];
    LAB2sRGB( &ptr[n], &q[0] );
    ((unsigned char*)in.data)[n] = q[0];
    ((unsigned char*)in.data)[n+1] = q[1];
    ((unsigned char*)in.data)[n+2] = q[2];
  }
}



// Colormap function
void filter_cmap( RawTile& in, enum cmap_type cmap ){

  float value;
  unsigned in_chan = in.channels;
  unsigned out_chan = 3;
  unsigned int ndata = in.dataLength * 8 / in.bpc;

  const float max3 = 1.0/3.0;
  const float max8 = 1.0/8.0;

  float *fptr = (float*)in.data;
  float *outptr = new float[ndata*out_chan];
  float *outv = outptr;

  switch(cmap){
    case HOT:
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#endif
      for( int unsigned n=0; n<ndata; n+=in_chan, outv+=3 ){
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
      for( unsigned int n=0; n<ndata; n+=in_chan, outv+=3 ){
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
      for( unsigned int n=0; n<ndata; n+=in_chan, outv+=3 ){
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
    default:
      break;
    };


  // Delete old data buffer
  delete[] (float*) in.data;
  in.data = outptr;
  in.channels = out_chan;
  in.dataLength = ndata * out_chan * in.bpc / 8;
}



// Inversion function
void filter_inv( RawTile& in ){

  unsigned int np = in.dataLength * 8 / in.bpc;
  float *infptr = (float*) in.data;

  // Loop through our pixels for floating values
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
  for( unsigned int n=0; n<np; n++ ){
    float v = infptr[n];
    infptr[n] = 1.0 - v;
  }
}



// Resize image using nearest neighbour interpolation
void filter_interpolate_nearestneighbour( RawTile& in, unsigned int resampled_width, unsigned int resampled_height ){

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
    output = new unsigned char[resampled_width*resampled_height*in.channels];
  }
  else output = (unsigned char*) in.data;

  // Calculate our scale
  float xscale = (float)width / (float)resampled_width;
  float yscale = (float)height / (float)resampled_height;

  for( unsigned int j=0; j<resampled_height; j++ ){
    for( unsigned int i=0; i<resampled_width; i++ ){

      // Indexes in the current pyramid resolution and resampled spaces
      // Make sure to limit our input index to the image surface
      unsigned int ii = (unsigned int) floorf(i*xscale);
      unsigned int jj = (unsigned int) floorf(j*yscale);
      unsigned int pyramid_index = (unsigned int) channels * ( ii + jj*width );

      unsigned int resampled_index = (i + j*resampled_width)*channels;
      for( int k=0; k<in.channels; k++ ){
	output[resampled_index+k] = input[pyramid_index+k];
      }
    }
  }

  // Delete original buffer
  if( new_buffer ) delete[] (unsigned char*) input;

  // Correctly set our Rawtile info
  in.width = resampled_width;
  in.height = resampled_height;
  in.dataLength = resampled_width * resampled_height * channels * in.bpc/8;
  in.data = output;
}



// Resize image using bilinear interpolation
//  - Floating point implementation which benchmarks about 2.5x slower than nearest neighbour
void filter_interpolate_bilinear( RawTile& in, unsigned int resampled_width, unsigned int resampled_height ){

  // Pointer to input buffer
  unsigned char *input = (unsigned char*) in.data;

  int channels = in.channels;
  unsigned int width = in.width;
  unsigned int height = in.height;

  // Create new buffer and pointer for our output
  unsigned char *output = new unsigned char[resampled_width*resampled_height*in.channels];

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
      unsigned int p11, p12, p21, p22;
      p11 = (unsigned int) ( channels * ( ii + jj*width ) );
      p12 = (unsigned int) ( channels * ( ii + (jj+1)*width ) );
      p21 = (unsigned int) ( channels * ( (ii+1) + jj*width ) );
      p22 = (unsigned int) ( channels * ( (ii+1) + (jj+1)*width ) );

      // Calculate the rest of our weights
      float iscale = i*xscale;
      float a = (float)(ii+1) - iscale;
      float b = iscale - (float)ii;

      // Output buffer index
      unsigned int resampled_index = j*resampled_width*in.channels + i*in.channels;

      for( int k=0; k<in.channels; k++ ){
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
  in.dataLength = resampled_width * resampled_height * channels * in.bpc/8;
  in.data = output;
}



// Function to apply a contrast adjustment and clip to 8 bit
void filter_contrast( RawTile& in, float c ){

  unsigned long np = in.width * in.height * in.channels;
  unsigned char* buffer = new unsigned char[np];
  float* infptr = (float*)in.data;

#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
  for( unsigned long n=0; n<np; n++ ){
    float v = infptr[n] * 255.0 * c;
    buffer[n] = (unsigned char)( (v<255.0) ? (v<0.0? 0.0 : v) : 255.0 );
  }

  // Replace original buffer with new
  delete[] (float*) in.data;
  in.data = buffer;
  in.bpc = 8;
  in.dataLength = np * in.bpc/8;
}



// Gamma correction
void filter_gamma( RawTile& in, float g ){

  if( g == 1.0 ) return;

  unsigned int np = in.dataLength * 8 / in.bpc;
  float* infptr = (float*)in.data;

  // Loop through our pixels for floating values
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for
#endif
  for( unsigned int n=0; n<np; n++ ){
    float v = infptr[n];
    infptr[n] = powf( v<0.0 ? 0.0 : v, g );
  }
}



// Rotation function
void filter_rotate( RawTile& in, float angle=0.0 ){

  // Currently implemented only for rectangular rotations
  if( (int)angle % 90 == 0 && (int)angle % 360 != 0 ){

    // Intialize our counter
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
void filter_greyscale( RawTile& rawtile ){

  if( rawtile.bpc != 8 || rawtile.channels != 3 ) return;

  unsigned int np = rawtile.width * rawtile.height;
  unsigned char* buffer = new unsigned char[rawtile.width * rawtile.height];

  // Calculate using fixed-point arithmetic
  //  - benchmarks to around 25% faster than floating point
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( rawtile.width*rawtile.height > PARALLEL_THRESHOLD )
#endif
  for( unsigned int i=0; i<np; i++ ){
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
}



// Apply twist or channel recombination to colour or multi-channel image
void filter_twist( RawTile& rawtile, const vector< vector<float> >& matrix ){

  unsigned long np = rawtile.width * rawtile.height;

  // Create temporary buffer for our calculated values
  float* pixel = new float[rawtile.channels];

  // Calculate the number of columns - limit to our number of channels if necessary
  unsigned int ncols = (matrix.size()>(unsigned int)rawtile.channels) ? rawtile.channels : matrix.size();
  unsigned int* nrows = new unsigned int[ncols];

  // Pre-calculate the size of each row
  for( unsigned int i=0; i<ncols; i++ ){
    nrows[i] = (matrix[i].size()>(unsigned int)rawtile.channels) ? rawtile.channels : matrix[i].size();
  }


  for( unsigned long i=0; i<np; i++ ){

    unsigned long n = i*rawtile.channels;

    // Calculate value for each channel
    for( unsigned int k=0; k<ncols; k++ ){

      // Zero our pixel buffer
      pixel[k] = 0.0;

      for( unsigned int j=0; j<nrows[k]; j++ ){
	float m = matrix[k][j];
	if( m ){
	  pixel[k] += (m == 1.0) ? ((float*)rawtile.data)[n+j] : ((float*)rawtile.data)[n+j] * m;
	}
      }
    }

    // Only write our values at the end as we reuse channel values several times during the twist loops
    for( int k=0; k<rawtile.channels; k++ ) ((float*)rawtile.data)[n++] = pixel[k];

  }
  delete[] nrows;
  delete[] pixel;
}



// Flatten a multi-channel image to a given number of bands by simply stripping
// away extra bands
void filter_flatten( RawTile& in, int bands ){

  // We cannot increase the number of channels
  if( bands >= in.channels ) return;

  unsigned long np = in.width * in.height;
  unsigned long ni = 0;
  unsigned long no = 0;
  unsigned int gap = in.channels - bands;

  // Simply loop through assigning to the same buffer
  for( unsigned long i=0; i<np; i++ ){
    for( int k=0; k<bands; k++ ){
      ((unsigned char*)in.data)[ni++] = ((unsigned char*)in.data)[no++];
    }
    no += gap;
  }

  in.channels = bands;
  in.dataLength = ni * in.bpc/8;
}




// Flip image in horizontal or vertical direction (0=horizontal,1=vertical)
void filter_flip( RawTile& rawtile, int orientation ){

  unsigned char* buffer = new unsigned char[rawtile.width * rawtile.height * rawtile.channels];

  // Vertical
  if( orientation == 2 ){
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma ivdep
#elif defined(_OPENMP)
#pragma omp parallel for if( rawtile.width*rawtile.height > PARALLEL_THRESHOLD )
#endif
    for( int j=rawtile.height-1; j>=0; j-- ){
      unsigned long n = j*rawtile.width*rawtile.channels;
      for( unsigned int i=0; i<rawtile.width; i++ ){
        unsigned long index = (rawtile.width*j + i)*rawtile.channels;
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
      unsigned long n = j*rawtile.width*rawtile.channels;
      for( int i=rawtile.width-1; i>=0; i-- ){
        unsigned long index = (rawtile.width*j + i)*rawtile.channels;
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

/***************************************************************************************************************************
 Adapted from resizing filters of the FreeImage project: http://freeimage.sourceforge.net/
 Integer approximation and other optimizations by @beaudet

 Resize image using integer implementation of lanczos filter 
 The algorithms for 24 bit color resizing were copied from the FreeImage project and modified to align with IIP's
 variable names and image storage format - optimizations using integer math scaled with a multiplier were used to 
 approximate floating point precision rather than relying on floating point math exclusively. The optimized 
 approximation still produces excellent image quality at good performance levels and runs many times faster than 
 the original Lanczos implementation from the FreeImage project
 Dave Beaudet (github: @beaudet)
***************************************************************************************************************************/
void filter_interpolate_lanczos( RawTile& in, int resampled_width, int resampled_height ){

    // Pointer to input buffer
    byte *input = (byte*) in.data;

    byte bytesPerColor = in.bpc / 8;
    byte bytesPerPixel = bytesPerColor * in.channels;

    // pitch is the number of bytes in a scan line
    const int src_pitch = in.width * bytesPerPixel;
    const int dst_pitch = resampled_width * bytesPerPixel;

    // Create a new buffer for the temporary output that is identical to the height of the input since
    // we're first going to shrink along the X axis, then secondarily along the Y axis
    byte *xscaled_output = new byte[in.height * dst_pitch];

    // build our filter and weights table
    CGenericFilter *filter = new(std::nothrow) CLanczos3Filter();

    // initialize weight caching table
    CWeightsTable xweightsTable(filter, resampled_width,  in.width);

    // cache all weights across the horizontal sampling line
    Weight xweights[resampled_width];
    for ( int x =0; x < resampled_width; x++ ) {
        int xleft  = CLAMP<int>(xweightsTable.getLeftBoundary(x), 0, in.width-1);
        int xright = CLAMP<int>(xweightsTable.getRightBoundary(x), 0, in.width-1);
        int numsamples = xright-xleft+1;
        xweights[x].newWeights(numsamples);
        for ( int i=0; i < numsamples; i++ ) {
            int w = xweightsTable.getIntWeight(x, i);
            xweights[x].addWeight(xleft + i, w);
        }
    }

    CWeightsTable yweightsTable(filter, resampled_height, in.height);
    // cache all weights across the vertical sampling line for faster access
    Weight yweights[resampled_height];
    for ( int y =0; y < resampled_height; y++ ) {
        int yleft  = CLAMP<int>(yweightsTable.getLeftBoundary(y), 0, in.height-1);
        int yright = CLAMP<int>(yweightsTable.getRightBoundary(y), 0, in.height-1);
        int numsamples = yright-yleft+1;
        yweights[y].newWeights(numsamples);                 
        for ( int i = 0; i < numsamples; i++ ) {
            int w = yweightsTable.getIntWeight(y, i);
            yweights[y].addWeight(yleft + i, w);
        }
    }

    /********************************************************************************************************************
     resampling first operates on the X-axis and then on the Y-axis since we are almost always downsampling with IIP
     rather than upscaling -- I'm amazed that the algorithm works as well as it does by only sampling images on a line
     rather than all the pixels surrounding it since it would seem that throws away image data that could be used in the
     resampling.  It's certainly faster using line sampling rather than area sampling and it still produces very good results.
     FreeImage claims that the code runs faster by scaling in the X direction first (presumably due to proximity of bytes to
     each other in RAM)

     The difference that the parallel compiler instructions make cannot be understated - the speed up is roughly linear 
     with the number of cores. Since we have approximately 4x number of samples here than in bilinear resizing, 
     we don't even bother checking whether the size of the image is below a parallel threshold - it pretty much always 
     will make sense to parallelize Lanczos
    ********************************************************************************************************************/
    #if defined(__ICC) || defined(__INTEL_COMPILER)
    #pragma ivdep
    #elif defined(_OPENMP)
    #pragma omp parallel for
    #endif
    for (int y = 0; y < in.height; y++) {

        // since we are scaling each row we get the start of the current scan line
        const byte * const src_bits = &input[y*src_pitch];
        byte *dst_pixel = &xscaled_output[y*dst_pitch];

        for (int x = 0; x < resampled_width; x++) {
            // loop through the source sample's pixels using the source scanline only
            const int sample_iLeft = xweights[x].minIdx;
            const int sample_width = xweights[x].maxIdx - sample_iLeft + 1;       // width of sample in pixels

            const byte *src_pixel = src_bits + ( sample_iLeft * bytesPerPixel );  // byte index of the start of sample

            // accumulate r,g,b values as we move along the sample horizontally
            int rint = 0, gint = 0, bint = 0;

            for (int i = 0; i < sample_width; i++) {
                const int intWeight = xweights[x].weights[i];
                if (intWeight != 0) {
                    rint += (intWeight * src_pixel[0]);
                    if ( in.channels > 1 ) {
                        gint += (intWeight * src_pixel[1]);
                        bint += (intWeight * src_pixel[2]);
                    }
                }
                src_pixel += bytesPerPixel;
            }

            // clamp and place result in destination pixel so it stays within 0->255 range of a byte without wrapping
            // adding 0.5 forces a rounding operation 
            dst_pixel[0] = (byte) CLAMP<int>( (int) ( rint / INTSCALER ), 0, 0xFF );
            dst_pixel[0] = (byte) CLAMP<int>( (int) ( rint / INTSCALER ), 0, 0xFF );
            if ( in.channels > 1 ) {
                dst_pixel[1] = (byte) CLAMP<int>( (int) ( gint / INTSCALER ), 0, 0xFF );
                dst_pixel[2] = (byte) CLAMP<int>( (int) ( bint / INTSCALER ), 0, 0xFF );
            }
            dst_pixel += bytesPerPixel;
        }
    }

    // safe to delete the original input buffer now
    delete[] (byte*) input;

    // switch the input to the now-x-scaled output from above and resample again along Y axis to get the final image
    input = xscaled_output;

    // and the pitch of our src is now the same as dst_pitch since we already scaled along X axis but in order to keep our
    // const declaration valid we just use dst_pitch below even though it might read a little bit funny
    // essentially the new src_pitch = dst_pitch at this point

    // create the final output buffer
    byte *output = new byte[resampled_height * dst_pitch];

    // scale the 24-bit transparent image into a 24 bpp destination image
    const byte *const src_base = &input[0];

    // inject performance enhancing steroids here
    #if defined(__ICC) || defined(__INTEL_COMPILER)
    #pragma ivdep
    #elif defined(_OPENMP)
    #pragma omp parallel for
    #endif
    for (int x = 0; x < resampled_width; x++) {

        // work on column x in destination
        const int bytesFromLeft = x * bytesPerPixel;   // the number of bytes into the current scanline for both dst and src
        byte *dst_pixel = output + bytesFromLeft;      // the pixel address in destination that we're going to set

        // loop through each column, scaling it as we go
        for (int y = 0; y < resampled_height; y++) {
            const int sample_iLower = yweights[y].minIdx;
            const int sample_height = yweights[y].maxIdx - sample_iLower + 1;              // width of sample 

            // pixel working with is base byte + this scan line start byte + bytes in from left side
            const byte *src_pixel = src_base + sample_iLower * dst_pitch + bytesFromLeft;  

            // accumulate r,g,b values as we move along the sample vertically
            int rint = 0, gint = 0, bint = 0;

            for (int i = 0; i < sample_height; i++) {
                const int intWeight = yweights[y].weights[i];
                if (intWeight != 0) {
                    rint += (intWeight * src_pixel[0]);
                    if ( in.channels > 1 ) {
                        gint += (intWeight * src_pixel[1]);
                        bint += (intWeight * src_pixel[2]);
                    }
                }
                src_pixel += dst_pitch;
            }

            // clamp and place result in destination pixel so it stays within 0->255 range of a byte without wrapping
            // adding 0.5 forces a rounding operation
            dst_pixel[0] = (byte) CLAMP<int>( (int) ( rint /  INTSCALER ), 0, 0xFF );
            if ( in.channels > 1 ) {
                dst_pixel[1] = (byte) CLAMP<int>( (int) ( gint /  INTSCALER ), 0, 0xFF );
                dst_pixel[2] = (byte) CLAMP<int>( (int) ( bint /  INTSCALER ), 0, 0xFF );
            }
            dst_pixel += dst_pitch;
        }
    }

    // delete the intermediately resized bitmap and the filter that we allocated
    delete[] (byte*) input;
    delete filter;

    // set the resulting dimensions and data of the resampled image
    in.width    = resampled_width;
    in.height   = resampled_height;
    in.data     = output;
    in.dataLength = resampled_height * dst_pitch;
}

