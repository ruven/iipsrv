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

// Colormap function
void filter_cmap( RawTile& in, enum cmap_type cmap, float min, float max ){

  int i;
  float value, outv[3], div, minimum, maximum;
  unsigned out_chan = 3;
  unsigned int ndata = in.dataLength * 8 / in.bpc;
  unsigned int k = 0;

  float max3=1./3.;
  float max4=1./4.;
  float c1=144./255.;

  void *buf;
  void *buffer;
  if( in.bpc == 8 )
    { buf = (unsigned char*) in.data;
      buffer = new unsigned char[in.width*in.height*out_chan];
      minimum = 0.;
      maximum = 255.; }
  else if( in.bpc == 16 ) 
    { buf = (unsigned short*) in.data;
      buffer = new unsigned short[in.width*in.height*out_chan];
      minimum = 0.;
      maximum = 65535.; }

  else if( in.bpc == 32 && in.sampleType == FIXEDPOINT )
    { buf = (unsigned int*) in.data;
      buffer = new unsigned int[in.width*in.height*out_chan];
      minimum = min;
      maximum = max; }

  else if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) 
    { buf = (float*) in.data;
      buffer = new float[in.width*in.height*out_chan];
      minimum = min;
      maximum = max; }

  div = ( maximum - minimum );

  for( int n=0; n<ndata; n++ ){
      if( in.bpc == 8 ) value =  (float) (((unsigned char*)buf)[n] - minimum) / div;
      else if( in.bpc == 16 ) value =  (float) (((unsigned short*)buf)[n] - minimum) / div;
      else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ) value =  (float) (((unsigned int*)buf)[n] - minimum) / div;
      else if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) value =  (float) (((float*)buf)[n] - minimum) / div;

    switch(cmap){
    case HOT:
      if(value>1.)
        { outv[0]=outv[1]=outv[2]=maximum; }
      else if(value<0)
        { outv[0]=outv[1]=outv[2]=0.; }
      else if(value<max3)
        { outv[0]=maximum * value/max3; outv[1]=outv[2]=0.; }
      else if(value<2*max3)
        { outv[0]=maximum; outv[1]= maximum*(value-max3)/max3; outv[2]=0.; }
      else if(value<1.)
        { outv[0]=outv[1]=maximum; outv[2]= maximum*(value-2*max3)/max3; }
      else { outv[0]=outv[1]=outv[2]=maximum; }
        break;
    case COLD:
      if(value>1.)
        { outv[0]=outv[1]=outv[2]=maximum; }
      else if(value<0)
        { outv[0]=outv[1]=outv[2]=0.; }
      else if(value<max3)
        { outv[0]=outv[1]=0.; outv[2]= maximum*value/max3; }
      else if(value<2*max3)
        { outv[0]=0.; outv[1]= maximum*(value-max3)/max3; outv[2]=maximum; }
      else if(value<1.)
        { outv[0]= maximum*(value-2*max3)/max3; outv[1]=outv[2]=maximum; }
      else {outv[0]=outv[1]=outv[2]=maximum;}
        break;
    case JET:
      if(value>1.)
        { outv[0]=outv[1]=outv[2]=maximum; }
      else if(value<0)
        { outv[0]=outv[1]=outv[2]=0.; }
      else if(value<max4)
        { outv[0]=outv[1]=0.; outv[2]=maximum* (c1+(1.-c1)*value/max4); }
      else if(value<2*max4)
        { outv[0]=0.; outv[1]= maximum* (value-max4)/max4; outv[2]=maximum; }
      else if(value<3*max4)
        { outv[0]= maximum* (value-2*max4)/max4; outv[1]=maximum; outv[2]=maximum-outv[0]; }
      else if(value<1.)
        { outv[0]=maximum; outv[1]= maximum*(1-(value-3*max4)/max4); outv[2]=0.; }
      else { outv[0]=maximum; outv[1]=outv[2]=0.; }
        break;
    case BLUE:
        outv[0]=outv[1]=0; outv[2]=maximum*value;
      break;
    case GREEN:
        outv[0]=0.; outv[1]=maximum*value; outv[2]=0.;
      break;
    case RED:
        outv[0]=maximum*value; outv[1]=outv[2]=0.;
      break;
    default:
      break;
    };

    for (int i = 0; i<3; i++) {
      if( in.bpc == 8 ) ((unsigned char*)buffer)[k++] = (unsigned char) outv[i];
      else if( in.bpc == 16 ) ((unsigned short*)buffer)[k++] = (unsigned short) outv[i];
      else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ) ((unsigned int*)buffer)[k++] = (unsigned int) outv[i];
      else if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) ((float*)buffer)[k++] = (float) outv[i];
    }
  }

  // Delete old data buffer
  if( in.bpc == 8 ) delete[] (unsigned char*) in.data;
  else if( in.bpc == 16 ) delete[] (unsigned short*) in.data;
  else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ) delete[] (unsigned int*) in.data;
  else if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) delete[] (float*) in.data;

  in.data = buffer;
  in.channels = out_chan;
  in.dataLength = in.width * in.height * out_chan;
}


// Resize image using nearest neighbour interpolation
void filter_interpolate_nearestneighbour( RawTile& in, unsigned int resampled_width, unsigned int resampled_height ){

  void *buf;
  if( in.bpc == 8 ) buf = (unsigned char*) in.data;
  else if( in.bpc == 16 ) buf = (unsigned short*) in.data;
  else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ) buf = (unsigned int*) in.data;
  else if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) buf = (float*) in.data;

  int channels = in.channels;
  unsigned int width = in.width;
  unsigned int height = in.height;

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
	if( in.bpc == 8 ){
	  ((unsigned char*)buf)[resampled_index+k] = ((unsigned char*)buf)[pyramid_index+k];
	}
	else if( in.bpc == 16 ){
	  ((unsigned short*)buf)[resampled_index+k] = ((unsigned short*)buf)[pyramid_index+k];
	}
	else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ){
	  ((unsigned int*)buf)[resampled_index+k] = ((unsigned int*)buf)[pyramid_index+k];
	}
	else if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ){
	  ((float*)buf)[resampled_index+k] = ((float*)buf)[pyramid_index+k];
	}
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

  void *buf;
  if( in.bpc == 8 ) buf = (unsigned char*) in.data;
  else if( in.bpc == 16 ) buf = (unsigned short*) in.data;
  else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ) buf = (unsigned int*) in.data;
  else if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) buf = (float*) in.data;

  int channels = in.channels;
  unsigned int width = in.width;
  unsigned int height = in.height;

  // Calculate our scale
  float xscale = (float)width / (float)resampled_width;
  float yscale = (float)height / (float)resampled_height;

  for( unsigned int j=0; j<resampled_height; j++ ){

    // Index to the current pyramid resolution's bottom left right pixel
    unsigned int jj = (unsigned int) floorf(j*yscale);

    // Calculate some weights - do this in the highest loop possible
    float jscale = j*yscale;
    float c = (float)(jj+1) - jscale;
    float d = jscale - (float)jj;

    for( unsigned int i=0; i<resampled_width; i++ ){

      // Index to the current pyramid resolution's bottom left right pixel
      unsigned int ii = (unsigned int) floorf(i*xscale);

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
	if( resampled_index == p11 ){
	  if( in.bpc == 8 ) ((unsigned char*)buf)[resampled_index+k] = ((unsigned char*)buf)[p11+k];
	  else if( in.bpc == 16 ) ((unsigned short*)buf)[resampled_index+k] = ((unsigned short*)buf)[p11+k];
	  else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ) ((unsigned int*)buf)[resampled_index+k] = ((unsigned int*)buf)[p11+k];
	  else if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) ((float*)buf)[resampled_index+k] = ((float*)buf)[p11+k];
	}
	else{
	  if( in.bpc == 8 ){
	    float tx = ((float)((unsigned char*)buf)[p11+k])*a + ((float)((unsigned char*)buf)[p21+k])*b;
	    float ty = ((float)((unsigned char*)buf)[p12+k])*a + ((float)((unsigned char*)buf)[p22+k])*b;
	    unsigned char r = (unsigned char)( c*tx + d*ty );
	    ((unsigned char*)buf)[resampled_index+k] = r;
	  }
	  else if( in.bpc == 16 ){
	    float tx = ((float)((unsigned short*)buf)[p11+k])*a + ((float)((unsigned short*)buf)[p21+k])*b;
	    float ty = ((float)((unsigned short*)buf)[p12+k])*a + ((float)((unsigned short*)buf)[p22+k])*b;
	    unsigned short r = (unsigned short)( c*tx + d*ty );
	    ((unsigned short*)buf)[resampled_index+k] = r;
	  }
	  else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ){
	    float tx = ((float)((unsigned int*)buf)[p11+k])*a + ((float)((unsigned int*)buf)[p21+k])*b;
	    float ty = ((float)((unsigned int*)buf)[p12+k])*a + ((float)((unsigned int*)buf)[p22+k])*b;
	    unsigned int r = (unsigned int)( c*tx + d*ty );
	    ((unsigned int*)buf)[resampled_index+k] = r;
	  }
	  else if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ){
	    float tx = ((float)((float*)buf)[p11+k])*a + ((float)((float*)buf)[p21+k])*b;
	    float ty = ((float)((float*)buf)[p12+k])*a + ((float)((float*)buf)[p22+k])*b;
	    float r = (float)( c*tx + d*ty );
	    ((float*)buf)[resampled_index+k] = r;
	  }
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
void filter_contrast( RawTile& in, float c, std::vector<float>& max, std::vector<float>& min ){

  unsigned int np = in.width * in.height * in.channels;

  unsigned char* buffer;

  // 8-bit case first
  if( in.bpc == 8 ){
    if( c == 1.0 ) return;
    buffer = (unsigned char*) in.data;
    for( unsigned int n=0; n<np; n++ ){
      float v = (float)(((unsigned char*)in.data)[n]) * c;
      v = (v<255.0) ? v : 255.0;
      buffer[n] = (unsigned char) v;
    }
  }
  // 16 and 32 bit images
  else{
    float v, contrast;

    // Allocate new 8 bit buffer for tile
    buffer = new unsigned char[np];

    if( in.bpc == 16 ) contrast = c/256.0;

    for( unsigned int n=0; n<np; n++ ){

      if( in.bpc == 32 ){
	int ch = (int)( n % in.channels );
	contrast = c * 256.0 / (max[ch]-min[ch]);
	v = ((float*)in.data)[n] * contrast;
      }
      else v = ((unsigned short*)in.data)[n] * contrast;

      v = (v<255.0) ? v : 255.0;
      v = (v>0.0) ? v : 0;
      buffer[n] = (unsigned char) v;
    }

    // Replace original buffer with new
    if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) delete[] (float*) in.data;
    else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ) delete[] (unsigned int*) in.data;
    else if( in.bpc == 16 ) delete[] (unsigned short*) in.data;

    in.data = buffer;
    in.bpc = 8;

  }

}


// Gamma correction
void filter_gamma( RawTile& in, float g, std::vector<float>& max, std::vector<float>& min ){

  float v;
  unsigned int np = in.width * in.height * in.channels;

  // Loop through our pixels
  for( unsigned int n=0; n<np; n++ ){

    int c = (int)( n % in.channels );

    if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) v = (float)((float*)in.data)[n];
    else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ) v = (float)((unsigned int*)in.data)[n];
    else if( in.bpc == 16 ) v = (float)((unsigned short*)in.data)[n];
    else v = (float)((unsigned char*)in.data)[n];

    // Normalize our data
    v = (v - min[c]) / (max[c] - min[c]);
    if (v<0.0) v = 0.0;
    else if (v>1.0) v = 1.0;

    // Perform gamma correction
    v = (max[c] - min[c]) * powf( v, g ) + min[c];

    // Limit to our allowed data range
    if( v < min[c] ) v = min[c];
    else if( v > max[c] ) v = max[c];

    if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) ((float*)in.data)[n] = (float) v;
    else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ) ((unsigned int*)in.data)[n] = (float) v;
    else if( in.bpc == 16 ) ((unsigned short*)in.data)[n] = (unsigned short) v;
    else v = ((unsigned char*)in.data)[n] = (unsigned char) v;
  }

}



// Rotation function
void filter_rotate( RawTile& in, float angle ){

  // Currently implemented only for rectangular rotations
  if( (int)angle % 90 == 0 && (int)angle % 360 != 0 ){

    // Intialize our counter and data buffer
    unsigned int n = 0;
    void* buffer = NULL;

    // Allocate memory for our temporary buffer
    if(in.bpc == 8) buffer = new unsigned char[in.width*in.height*in.channels];
    else if(in.bpc == 16) buffer = new unsigned short[in.width*in.height*in.channels];
    else if(in.bpc == 32 && in.sampleType == FIXEDPOINT ) buffer = new unsigned int[in.width*in.height*in.channels];
    else if(in.bpc == 32 && in.sampleType == FLOATINGPOINT ) buffer = new float[in.width*in.height*in.channels];

    // Rotate 90
    if( (int) angle % 360 == 90 ){
      for( unsigned int i=0; i < in.width; i++ ){
	for( unsigned int j=in.height; j>0; j-- ){
	  unsigned int index = (in.width*j + i)*in.channels;
	  for( int k=0; k < in.channels; k++ ){
	    if(in.bpc == 8) ((unsigned char*)buffer)[n++] = ((unsigned char*)in.data)[index+k];
	    else if(in.bpc == 16) ((unsigned short*)buffer)[n++] = ((unsigned short*)in.data)[index+k];
	    else if(in.bpc == 32 && in.sampleType == FIXEDPOINT) ((unsigned int*)buffer)[n++] = ((unsigned int*)in.data)[index+k];
	    else if(in.bpc == 32 && in.sampleType == FLOATINGPOINT ) ((float*)buffer)[n++] = ((float*)in.data)[index+k];
	  }
	}
      }
    }

    // Rotate 270
    else if( (int) angle % 360 == 270 ){
      for( int i=in.width; i>0; i-- ){
	for( int j=0; j < in.height; j++ ){
	  unsigned int index = (in.width*j + i)*in.channels;
	  for( int k=0; k < in.channels; k++ ){
	    if(in.bpc == 8) ((unsigned char*)buffer)[n++] = ((unsigned char*)in.data)[index+k];
	    else if(in.bpc == 16) ((unsigned short*)buffer)[n++] = ((unsigned short*)in.data)[index+k];
	    else if(in.bpc == 32 && in.sampleType == FIXEDPOINT ) ((unsigned int*)buffer)[n++] = ((unsigned int*)in.data)[index+k];
	    else if(in.bpc == 32 && in.sampleType == FLOATINGPOINT ) ((float*)buffer)[n++] = ((float*)in.data)[index+k];
	  }
	}
      }
    }

    // Rotate 180
    else if( (int) angle % 360 == 180 ){
      for( unsigned int i=(in.width*in.height)-1; i > 0; i-- ){
	unsigned index = i * in.channels;
	for( int k=0; k < in.channels; k++ ){
	  if(in.bpc == 8) ((unsigned char*)buffer)[n++]  = ((unsigned char*)in.data)[index+k];
	  else if(in.bpc == 16) ((unsigned short*)buffer)[n++] = ((unsigned short*)in.data)[index+k];
	  else if(in.bpc == 32 && in.sampleType == FIXEDPOINT) ((unsigned int*)buffer)[n++] = ((unsigned int*)in.data)[index+k];
	  else if(in.bpc == 32 && in.sampleType == FLOATINGPOINT ) ((float*)buffer)[n++] = ((float*)in.data)[index+k];
	}
      }
    }

    // Delete old data buffer
    if( in.bpc == 8 ) delete[] (unsigned char*) in.data;
    else if( in.bpc == 16 ) delete[] (unsigned short*) in.data;
    else if( in.bpc == 32 && in.sampleType == FIXEDPOINT ) delete[] (unsigned int*) in.data;
    else if( in.bpc == 32 && in.sampleType == FLOATINGPOINT ) delete[] (float*) in.data;

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
