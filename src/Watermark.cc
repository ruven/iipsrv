/*
    IIPImage Server - Member functions for Watermark.h

    Development supported by Moravian Library in Brno (Moravska zemska
    knihovna v Brne, http://www.mzk.cz/) R&D grant MK00009494301 & Old
    Maps Online (http://www.oldmapsonline.org/) from the Ministry of
    Culture of the Czech Republic.


    Copyright (C) 2010-2025 Ruven Pillay.

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


#include "Watermark.h"
#include <cstring>
#include <cstdlib>
#include <tiff.h>
#include <tiffio.h>



// Load up and initialize our watermark image
void Watermark::init()
{
  if( _image.length() > 0 ){

    TIFF *tiff_watermark;
    if( ( tiff_watermark = TIFFOpen( _image.c_str(), "r" ) ) ){

      TIFFGetField( tiff_watermark, TIFFTAG_IMAGEWIDTH, &_width );
      TIFFGetField( tiff_watermark, TIFFTAG_IMAGELENGTH, &_height );
      TIFFGetField( tiff_watermark, TIFFTAG_BITSPERSAMPLE, &_bpc );

      uint32_t *buffer = new uint32_t[_width*_height];

      if( TIFFReadRGBAImageOriented( tiff_watermark, _width, _height, buffer, ORIENTATION_TOPLEFT ) == 0 ){
	delete[] buffer;
	TIFFClose( tiff_watermark );
	return;
      }

      // Set our number of channels to 3 as TIFFReadRGBAImage always outputs an 8bit colour image
      _channels = 3;

      // Set up the memory storage
      _watermark = new unsigned char[_width*_height*_channels];
      memset( _watermark, 0, (size_t) _width*_height*_channels );

      // Load the data into our buffers
      for( uint32_t i=0; i<_width*_height; i++ ){
	uint32_t rgba = buffer[i];
	unsigned char r,g,b;
	float a;
	// Extract the RGBA values
	r = (unsigned char) TIFFGetR(rgba);
	g = (unsigned char) TIFFGetG(rgba);
	b = (unsigned char) TIFFGetB(rgba);
	a = (float) TIFFGetA(rgba) / 255;
	_watermark[i*3] = r * _opacity * a;
	_watermark[i*3 + 1] = g * _opacity * a;
	_watermark[i*3 + 2] = b * _opacity * a;
      }

      delete[] buffer;
      TIFFClose( tiff_watermark );
      _isSet = true;
    }

  }

}



// Apply the watermark to a buffer of data
void Watermark::apply( void* data, unsigned int width, unsigned int height, unsigned int channels, unsigned int bpc, unsigned int block )
{
  // Sanity check
  if( !_isSet || (_probability==0) || (_opacity==0) ) return;

  /* Calculate the size of the blocks into which we paste the watermark and the number of blocks horizontally and vertically.
     For tile requests we define the block as being the whole image and, thus, apply a single watermark.
     For larger regions, we divide the image into blocks and allow multiple watermarks to be placed, one within each block
  */
  unsigned int ntlx = 1;
  unsigned int ntly = 1;
  unsigned int tile_width = width;
  unsigned int tile_height = height;
  unsigned rem_x = 0;
  unsigned rem_y = 0;

  // If block has been defined and our image is larger than the block size, apply multiple watermarks, each randomly placed within a block
  if( (block > 0) && (width > block || height > block) ){
    tile_width = block;
    tile_height = block;
    rem_x = width % tile_width;
    ntlx = (width / tile_width) + (rem_x == 0 ? 0 : 1);
    rem_y = height % tile_height;
    ntly = (height / tile_height) + (rem_y == 0 ? 0 : 1);
  }


  // Loop through each block
  for( unsigned int ty=0; ty<ntly; ty++ ){
    for( unsigned int tx=0; tx<ntlx; tx++ ){

      // Get random number as a float between 0 and 1
      float random = rand() / (float)RAND_MAX;
 
      // Only apply if our random number is less than our given probability
      if( random < _probability ){

	// Block width and height
	unsigned int tw = tile_width;
	unsigned int th = tile_height;

	// Update block size if this is last row
	if( (tx == ntlx - 1) && ( rem_x != 0 ) ) tw = rem_x;
	if( (ty == ntly - 1) && ( rem_y != 0 ) ) th = rem_y;

	// Vary watermark position randomly within the block depending on available space
	unsigned int xoffset = 0;
	if( tw > _width ){
	  random = rand() / (float)RAND_MAX;
	  xoffset = random * (tw - _width);
	}

	unsigned int yoffset = 0;
	if( th > _height ){
	  random = rand() / (float)RAND_MAX;
	  yoffset = random * (th - _height);
	}

	// Limit the area of the watermark to the size of the block
	unsigned int xlimit = _width;
	unsigned int ylimit = _height;
	if( _width > tw ) xlimit = tw;
	if( _height > th ) ylimit = th;

	for( unsigned int j=0; j<ylimit; j++ ){
	  for( unsigned int i=0; i<xlimit; i++ ){
	    for( unsigned int k=0; k<channels; k++ ){

	      // Calculate the index on the image buffer that is to be watermarked
	      uint32_t id = ((ty*tile_width)+j+yoffset)*width*channels + ((tx*tile_height)+i+xoffset)*channels + k;

	      // For 16bit images we need to multiply up as our watermark data is always 8bit
	      // We do our maths in unsigned int to allow us to clip correctly
	      if( bpc == 16 ){
		unsigned short* d = (unsigned short*) data;
		unsigned int t = (unsigned int)( d[id] + _watermark[j*_width*_channels + i*_channels + k]*256 );
		if( t > 65535 ) t = 65535;
		d[id] = (unsigned short) t;
	      }
	      // TIFFReadRGBAImage always scales to 8bit, so never any need for downscaling, but clip to 255
	      // We do our maths in unsigned short to allow us to clip correctly after
	      else{
		unsigned char* d = (unsigned char*) data;
		unsigned short t = (unsigned short)( d[id] + _watermark[j*_width*_channels + i*_channels + k] );
		if( t > 255 ) t = 255;
		d[id] = (unsigned char) t;
	      }
	    }
	  }
	}
      }
    }
  }
}
