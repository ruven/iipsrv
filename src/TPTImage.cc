// Member functions for TPTImage.h

/*  IIP Server: Tiled Pyramidal TIFF handler

    Copyright (C) 2000-2012 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.

*/


#include "TPTImage.h"
#include <sstream>


using namespace std;


void TPTImage::openImage() throw (string)
{
//   tiff = NULL;
//   tile_buf = NULL;

  // Insist that the tiff and tile_buf be non-NULL
  if( tiff || tile_buf ){
    throw string( "TPT::openImage: tiff or tile_buf is not NULL" );
  }

  string filename = getFileName( currentX, currentY );

  // Check if our image has been modified
  updateTimestamp(filename);


  // Try to open and allocate a buffer
  if( ( tiff = TIFFOpen( filename.c_str(), "r" ) ) == NULL ){
    throw string( "tiff open failed for: " + filename );
  }
  loadImageInfo( currentX, currentY );

  // Insist on a tiled image
  if( (tile_width == 0) && (tile_height == 0) )
    throw string( "TIFF image is not tiled" );

  // Also get some metadata
  char *tmp = NULL;
  int count;
  if( TIFFGetField( tiff, TIFFTAG_ARTIST, &tmp ) ) metadata["author"] = tmp;
  if( TIFFGetField( tiff, TIFFTAG_COPYRIGHT, &tmp ) ) metadata["copyright"] = tmp;
  if( TIFFGetField( tiff, TIFFTAG_DATETIME, &tmp ) ) metadata["create-dtm"] = tmp;
  if( TIFFGetField( tiff, TIFFTAG_IMAGEDESCRIPTION, &tmp ) ) metadata["subject"] = tmp;
  if( TIFFGetField( tiff, TIFFTAG_SOFTWARE, &tmp ) ) metadata["app-name"] = tmp;
  if( TIFFGetField( tiff, TIFFTAG_XMLPACKET, &count, &tmp ) ) metadata["xmp"] = string(tmp,count);
  isSet = true;

}


void TPTImage::loadImageInfo( int seq, int ang ) throw(string)
{
  tdir_t current_dir;
  int count;
  uint16 colour, samplesperpixel, bitspersample, sampleformat;
  double sminvalue[4], smaxvalue[4];
  unsigned int w, h;
  string filename;

  // If we are currently working on a different sequence number, then
  //  close and reload the image.
  if( (currentX != seq) || (currentY != ang) ){
    closeImage();
    filename = getFileName( seq, ang );
    if( ( tiff = TIFFOpen( filename.c_str(), "r" ) ) == NULL ){
      throw string( "tiff open failed for:" + filename );
    }
    currentX = seq;
    currentY = ang;
  }

  // Get the tile and image sizes
  TIFFGetField( tiff, TIFFTAG_TILEWIDTH, &tile_width );
  TIFFGetField( tiff, TIFFTAG_TILELENGTH, &tile_height );
  TIFFGetField( tiff, TIFFTAG_IMAGEWIDTH, &w );
  TIFFGetField( tiff, TIFFTAG_IMAGELENGTH, &h );
  TIFFGetField( tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel );
  TIFFGetField( tiff, TIFFTAG_BITSPERSAMPLE, &bitspersample );
  TIFFGetField( tiff, TIFFTAG_PHOTOMETRIC, &colour );
  TIFFGetField( tiff, TIFFTAG_SAMPLEFORMAT, &sampleformat );

  // We have to do this conversion explicitly to avoid problems on Mac OS X
  channels = (unsigned int) samplesperpixel;
  bpp = (unsigned int) bitspersample;
  sampleType = (sampleformat==3) ? _FLOAT : _FIXED;

  // Check for the no. of resolutions in the pyramidal image
  current_dir = TIFFCurrentDirectory( tiff );
  TIFFSetDirectory( tiff, 0 );

  // Store the list of image dimensions available
  image_widths.push_back( w );
  image_heights.push_back( h );

  for( count = 0; TIFFReadDirectory( tiff ); count++ ){
    TIFFGetField( tiff, TIFFTAG_IMAGEWIDTH, &w );
    TIFFGetField( tiff, TIFFTAG_IMAGELENGTH, &h );
    image_widths.push_back( w );
    image_heights.push_back( h );
  }
  // Reset the TIFF directory
  TIFFSetDirectory( tiff, current_dir );

  numResolutions = count+1;

  // Handle various colour spaces
  if( colour == PHOTOMETRIC_CIELAB ) colourspace = CIELAB;
  else if( colour == PHOTOMETRIC_MINISBLACK ) colourspace = GREYSCALE;
  else if( colour == PHOTOMETRIC_PALETTE ){
    // Watch out for colourmapped images. There are stored as 1 sample per pixel,
    // but are decoded to 3 channels by libtiff, so declare them as sRGB
    colourspace = sRGB;
    channels = 3;
  }
  else if( colour == PHOTOMETRIC_YCBCR ){
    // JPEG encoded tiles can be subsampled YCbCr encoded. Ask to decode of these to RGB
    TIFFSetField( tiff, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB );
    colourspace = sRGB;
  }
  else colourspace = sRGB;

  // Get the max and min values for our data type - required for floats
  TIFFGetFieldDefaulted( tiff, TIFFTAG_SMINSAMPLEVALUE, sminvalue );
  TIFFGetFieldDefaulted( tiff, TIFFTAG_SMAXSAMPLEVALUE, smaxvalue );
  for( int i=0; i<channels; i++ ){
    min.push_back( (float)sminvalue[i] );
    if( bpp < 32 ){
      if( bpp == 16 ) smaxvalue[i] = 65535.0;
      else smaxvalue[i] = 255.0;
    }
    max.push_back( smaxvalue[i] );
  }

}


void TPTImage::closeImage()
{
  if( tiff != NULL ){
    TIFFClose( tiff );
    tiff = NULL;
  }
  if( tile_buf != NULL ){
    _TIFFfree( tile_buf );
    tile_buf = NULL;
  }
}


RawTile TPTImage::getTile( int seq, int ang, unsigned int res, int layers, unsigned int tile ) throw (string)
{
  uint32 im_width, im_height, tw, th, ntlx, ntly;
  uint32 rem_x, rem_y;
  uint16 colour;
  string filename;


  // Check the resolution exists
  if( res > numResolutions ){
    ostringstream error;
    error << "TPTImage :: Asked for non-existant resolution: " << res;
    throw error.str();
  }


  // If we are currently working on a different sequence number, then
  //  close and reload the image.
  if( (currentX != seq) || (currentY != ang) ){
    closeImage();
  }


  // Open the TIFF if it's not already open
  if( !tiff ){
    filename = getFileName( seq, ang );
    if( ( tiff = TIFFOpen( filename.c_str(), "r" ) ) == NULL ){
      throw string( "tiff open failed for:" + filename );
    }
  }


  // Reload our image information in case the tile size etc is different
  if( (currentX != seq) || (currentY != ang) ){
    loadImageInfo( currentX, currentY );
  }


  // The first resolution is the highest, so we need to invert 
  //  the resolution - can avoid this if we store our images with
  //  the smallest image first. 
  int vipsres = ( numResolutions - 1 ) - res;
  

  // Change to the right directory for the resolution
  if( !TIFFSetDirectory( tiff, vipsres ) ) {
    throw string( "TIFFSetDirectory failed" );
  }


  // Check that a valid tile number was given  
  if( tile >= TIFFNumberOfTiles( tiff ) ) {
    ostringstream tile_no;
    tile_no << "Asked for non-existant tile: " << tile;
    throw tile_no.str();
  } 


  // Get the size of this tile, the current image,
  //  the number of samples and the colourspace.
  // TIFFTAG_TILEWIDTH give us the values for the resolution,
  //  not for the tile itself
  TIFFGetField( tiff, TIFFTAG_TILEWIDTH, &tw );
  TIFFGetField( tiff, TIFFTAG_TILELENGTH, &th );
  TIFFGetField( tiff, TIFFTAG_IMAGEWIDTH, &im_width );
  TIFFGetField( tiff, TIFFTAG_IMAGELENGTH, &im_height );
  TIFFGetField( tiff, TIFFTAG_PHOTOMETRIC, &colour );
//   TIFFGetField( tiff, TIFFTAG_SAMPLESPERPIXEL, &channels );
//   TIFFGetField( tiff, TIFFTAG_BITSPERSAMPLE, &bpp );


  // Get the width and height for last row and column tiles
  rem_x = im_width % tw;
  rem_y = im_height % th;


  // Calculate the number of tiles in each direction
  ntlx = (im_width / tw) + (rem_x == 0 ? 0 : 1);
  ntly = (im_height / th) + (rem_y == 0 ? 0 : 1);


  // Alter the tile size if it's in the last column
  if( ( tile % ntlx == ntlx - 1 ) && ( rem_x != 0 ) ) {
    tw = rem_x;
  }


  // Alter the tile size if it's in the bottom row
  if( ( tile / ntlx == ntly - 1 ) && rem_y != 0 ) {
    th = rem_y;
  }


  // Handle various colour spaces
  if( colour == PHOTOMETRIC_CIELAB ) colourspace = CIELAB;
  else if( colour == PHOTOMETRIC_MINISBLACK ) colourspace = GREYSCALE;
  else if( colour == PHOTOMETRIC_PALETTE ){
    // Watch out for colourmapped images. There are stored as 1 sample per pixel,
    // but are decoded to 3 channels by libtiff, so declare them as sRGB
    colourspace = GREYSCALE;
    channels = 1;
  }
  else if( colour == PHOTOMETRIC_YCBCR ){
    // JPEG encoded tiles can be subsampled YCbCr encoded. Ask to decode these to RGB
    TIFFSetField( tiff, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB );
    colourspace = sRGB;
  }
  else colourspace = sRGB;


  // Allocate memory for our tile.
  if( !tile_buf ){
    if( ( tile_buf = _TIFFmalloc( TIFFTileSize(tiff) ) ) == NULL ){
      throw string( "tiff malloc tile failed" );
    }
  }

  // Decode and read the tile
  int length = TIFFReadEncodedTile( tiff, (ttile_t) tile,
				    tile_buf, (tsize_t) - 1 );
  if( length == -1 ) {
    throw string( "TIFFReadEncodedTile failed for " + getFileName( seq, ang ) );
  }


  RawTile rawtile( tile, res, seq, ang, tw, th, channels, bpp );
  rawtile.data = tile_buf;
  rawtile.dataLength = length;
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.memoryManaged = 0;
  rawtile.padded = true;
  rawtile.sampleType = sampleType;

  return( rawtile );

}

