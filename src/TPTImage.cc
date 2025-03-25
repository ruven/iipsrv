// Member functions for TPTImage.h

/*  IIP Server: Tiled Pyramidal TIFF handler

    Copyright (C) 2000-2025 Ruven Pillay.

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


#include "TPTImage.h"
#include "Logger.h"
#include <sstream>

using namespace std;


/* Set our TIFF open mode to read-only (r), no memory mapping (m) and on-demand strip/tile offset/bytecount array loading (O):
   Memory mapping makes the kind of sparse random access we require for iipsrv slower
   On-demand loading (available in libtiff 4.1.0) enables significantly faster loading of very large TIFF files
*/
static const char* mode = "rmO";


// Reference our logging object
extern Logger logfile;


// Handle libtiff errors as exceptions and log warnings to our Logger
static void errorHandler( const char* module, const char* fmt, va_list args ){
  char buffer[1024];
  vsnprintf( buffer, sizeof(buffer), fmt, args );
  throw file_error( "TPTImage :: TIFF error: " + string(buffer) );
}

static void warningHandler( const char* module, const char* fmt, va_list args ){
  if( IIPImage::logging ){
    char buffer[1024];
    vsnprintf( buffer, sizeof(buffer), fmt, args );
    logfile << "TPTImage :: TIFF warning: " << buffer << endl;
  }
}


void TPTImage::setupLogging(){
  TIFFSetErrorHandler( errorHandler );
  TIFFSetWarningHandler( warningHandler );
}


void TPTImage::openImage()
{
  // Insist that the tiff pointer be NULL
  if( tiff ) throw file_error( "TPTImage :: tiff pointer is not NULL" );

  string filename = getFileName( currentX, currentY );

  // Update our timestamp
  updateTimestamp( filename );

  // Try to open and allocate a buffer
  if( ( tiff = TIFFOpen( filename.c_str(), mode ) ) == NULL ){
    throw file_error( "TPTImage :: TIFFOpen() failed for: " + filename );
  }

  // Load our metadata if not already loaded
  if( bpc == 0 ) loadImageInfo( currentX, currentY );

  // Insist on a tiled image
  if( (tile_widths[0] == 0) && (tile_heights[0] == 0) ){
    throw file_error( "TPTImage :: Image is not tiled" );
  }

  isSet = true;

}


void TPTImage::loadImageInfo( int seq, int ang )
{
  tdir_t current_dir;
  int count = 0;
  uint16_t colour, samplesperpixel, bitspersample, sampleformat;
  double *sminvalue = NULL, *smaxvalue = NULL;
  double scale;
  unsigned int tw, th, w, h;
  string filename;
  const char *tmp = NULL;

  currentX = seq;
  currentY = ang;

  // Get various essential image parameters
  TIFFGetField( tiff, TIFFTAG_IMAGEWIDTH, &w );
  TIFFGetField( tiff, TIFFTAG_IMAGELENGTH, &h );
  TIFFGetField( tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel );
  TIFFGetField( tiff, TIFFTAG_BITSPERSAMPLE, &bitspersample );
  TIFFGetField( tiff, TIFFTAG_PHOTOMETRIC, &colour );
  TIFFGetField( tiff, TIFFTAG_SAMPLEFORMAT, &sampleformat );
  TIFFGetField( tiff, TIFFTAG_XRESOLUTION, &dpi_x );
  TIFFGetField( tiff, TIFFTAG_YRESOLUTION, &dpi_y );
  TIFFGetField( tiff, TIFFTAG_RESOLUTIONUNIT, &dpi_units );

  // If image is untiled, set tile sizes to zero
  if( TIFFGetField( tiff, TIFFTAG_TILEWIDTH, &tw ) == 0 ) tw = 0;
  if( TIFFGetField( tiff, TIFFTAG_TILEWIDTH, &th ) == 0 ) th = 0;

  // Units for libtiff are 1=unknown, 2=DPI and 3=pixels/cm, whereas we want 0=unknown, 1=DPI and 2=pixels/cm
  dpi_units--;

  // We have to do this conversion explicitly to avoid problems on Mac OS X
  channels = (unsigned int) samplesperpixel;
  bpc = (unsigned int) bitspersample;
  sampleType = (sampleformat==3) ? SampleType::FLOATINGPOINT : SampleType::FIXEDPOINT;

  // Check for the no. of resolutions in the pyramidal image
  current_dir = TIFFCurrentDirectory( tiff );

  // In order to get our list of image sizes, make sure we start in the first TIFF directory
  if( current_dir != 0 ){
    if( !TIFFSetDirectory( tiff, 0 ) ) throw file_error( "TPTImage :: TIFFSetDirectory() failed" );
  }


  // Empty any existing list of available resolution sizes
  image_widths.clear();
  image_heights.clear();
  tile_widths.clear();
  tile_heights.clear();

  // Store the list of image dimensions available, starting with the full resolution
  image_widths.push_back( w );
  image_heights.push_back( h );
  tile_widths.push_back( tw );
  tile_heights.push_back( th );

  // Add this to our list of valid resolutions
  resolution_ids.push_back( 0 );

  // Sub-resolutions can either be stored within the SubIFDs of a top-level IFD or in separate top-level IFDs.
  // Check first for sub-resolution levels stored within SubIFDs (as used by OME-TIFF).
  // In these files, the full resolution image is stored in the first IFD and subsequent
  // resolutions are stored in SubIFDs
  loadSubIFDs();
  subifd_ifd = 0;

  if( subifds.size() > 0 ){
    // Start from 1 as the top-level IFD already holds the full resolution image
    for( unsigned int n = 1; n<subifds.size(); n++ ){
      if( TIFFSetSubDirectory( tiff, subifds[n] ) ){
	uint32_t stype;
	// Only use valid reduced image subfile types
	if( (TIFFGetField( tiff, TIFFTAG_SUBFILETYPE, &stype ) == 1) && (stype == 0x01) ){

	  // Store exact image size for each resolution level
	  TIFFGetField( tiff, TIFFTAG_IMAGEWIDTH, &w );
	  TIFFGetField( tiff, TIFFTAG_IMAGELENGTH, &h );
	  image_widths.push_back( w );
	  image_heights.push_back( h );

	  // Tile sizes can vary between resolutions
	  TIFFGetField( tiff, TIFFTAG_TILEWIDTH, &tw );
	  TIFFGetField( tiff, TIFFTAG_TILELENGTH, &th );
	  tile_widths.push_back( tw );
	  tile_heights.push_back( th );

	  count++;
	}
      }
    }

    // If there are valid SubIFDs, tag this image appropriately and check whether we have a stack of images
    if( count > 0 ){
      pyramid = SUBIFD;
      loadStackInfo();
    }

    // Reset to first TIFF directory
    if( !TIFFSetDirectory( tiff, 0 ) ) throw file_error( "TPTImage :: TIFFSetDirectory() failed" );
  }

  // If there are no SubIFD resolutions, look for them in the main sequence of IFD TIFF directories
  if( pyramid == NORMAL ){
    for( count = 0; TIFFReadDirectory( tiff ); count++ ){

      // Only use tiled IFD directories
      if( ( TIFFGetField( tiff, TIFFTAG_TILEWIDTH, &tw ) == 1 ) &&
	  ( TIFFGetField( tiff, TIFFTAG_TILELENGTH, &th ) == 1 ) ){

	// Tile sizes can vary between resolutions
	tile_widths.push_back( tw );
	tile_heights.push_back( th );

	// Store exact image size for each resolution level
	TIFFGetField( tiff, TIFFTAG_IMAGEWIDTH, &w );
	TIFFGetField( tiff, TIFFTAG_IMAGELENGTH, &h );
	image_widths.push_back( w );
	image_heights.push_back( h );

	// Add this index to our list of valid resolutions
	resolution_ids.push_back( count+1 );
      }

    }

    // Check whether this is in fact a stack from an image too small to have SubIFD resolutions
    if( (image_widths.size() > 0) && (image_widths[0] == image_widths[1]) && (image_heights[0] == image_heights[1]) ){
      loadStackInfo();
      if( stack.size() > 0 ){
	// Remove duplicate sizes
	image_widths.resize(1);
	image_heights.resize(1);
	tile_widths.resize(1);
	tile_heights.resize(1);
	count = 0;
      }
    }
  }


  // Total number of available resolutions
  numResolutions = image_widths.size();


  // Reset the TIFF directory to where it was
  if( !TIFFSetDirectory( tiff, current_dir ) ) throw file_error( "TPTImage :: TIFFSetDirectory() failed" );


  // Handle various colour spaces
  if( colour == PHOTOMETRIC_CIELAB ) colorspace = ColorSpace::CIELAB;
  else if( colour == PHOTOMETRIC_MINISBLACK ){
    colorspace = (bpc==1)? ColorSpace::BINARY : ColorSpace::GREYSCALE;
  }
  else if( colour == PHOTOMETRIC_PALETTE ){
    // Watch out for colourmapped images. These are stored as 1 sample per pixel,
    // but are decoded to 3 channels by libtiff, so declare them as sRGB
    colorspace = ColorSpace::sRGB;
    channels = 3;
  }
  else if( colour == PHOTOMETRIC_YCBCR ){
    // JPEG encoded tiles can be subsampled YCbCr encoded. Ask to decode these to RGB
    TIFFSetField( tiff, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB );
    colorspace = ColorSpace::sRGB;
  }
  else colorspace = ColorSpace::sRGB;


  // Get the max and min values for our data (important for float data)
  // First initialize default values to zero in case
  double *default_min = new double[channels];
  double *default_max = new double[channels];
  for( unsigned int k=0; k<channels; k++ ){
    default_min[k] = 0.0;
    default_max[k] = 0.0;
  }

  // These max and min values can either be single values per image, or as from libtiff > 4.0.2
  // per channel (see: http://www.asmail.be/msg0055458208.html)
#ifdef TIFFTAG_PERSAMPLE

  TIFFSetField( tiff, TIFFTAG_PERSAMPLE, PERSAMPLE_MULTI ); // Need to activate per sample mode
  TIFFGetField( tiff, TIFFTAG_SMINSAMPLEVALUE, &sminvalue );
  TIFFGetField( tiff, TIFFTAG_SMAXSAMPLEVALUE, &smaxvalue );

  if( !sminvalue ) sminvalue = default_min;
  if( !smaxvalue ) smaxvalue = default_max;

#else
  // Set defaults
  sminvalue = default_min;
  smaxvalue = default_max;

  // Add the single min/max header value to each channel if tag exists
  double minmax;
  if( TIFFGetField( tiff, TIFFTAG_SMINSAMPLEVALUE, &minmax ) == 1 ){
    for( unsigned int k=0; k<channels; k++ ) sminvalue[k] = minmax;
  }
  if( TIFFGetField( tiff, TIFFTAG_SMAXSAMPLEVALUE, &minmax ) == 1 ){
    for( unsigned int k=0; k<channels; k++ ) smaxvalue[k] = minmax;
  }
#endif

  // Make sure our min and max arrays are empty
  min.clear();
  max.clear();

  for( unsigned int i=0; i<channels; i++ ){
    // Set our max to the full bit range if max not set in header
    if( smaxvalue[i] == 0 ){
      if( bpc <= 8 ) smaxvalue[i] = 255.0;
      else if( bpc == 12 ) smaxvalue[i] = 4095.0;
      else if( bpc == 16 ) smaxvalue[i] = 65535.0;
      else if( bpc == 32 && sampleType == SampleType::FIXEDPOINT ) smaxvalue[i] = 4294967295.0;
      else if( bpc == 32 && sampleType == SampleType::FLOATINGPOINT ) smaxvalue[i] = 1.0;  // Set dummy value for float
    }
    min.push_back( (float)sminvalue[i] );
    max.push_back( (float)smaxvalue[i] );
  }
  // Don't forget to delete our allocated arrays
  delete[] default_min;
  delete[] default_max;

  // Also get some basic metadata
  if( TIFFGetField( tiff, TIFFTAG_ARTIST, &tmp ) ) metadata.insert( {"creator",tmp} );
  if( TIFFGetField( tiff, TIFFTAG_COPYRIGHT, &tmp ) ) metadata.insert( {"rights",tmp} );
  if( TIFFGetField( tiff, TIFFTAG_DATETIME, &tmp ) ) metadata.insert( {"date",tmp} );
  if( TIFFGetField( tiff, TIFFTAG_IMAGEDESCRIPTION, &tmp ) ) metadata.insert( {"description",tmp} );
  if( TIFFGetField( tiff, TIFFTAG_DOCUMENTNAME, &tmp ) ) metadata.insert( {"title",tmp} );
  if( TIFFGetField( tiff, TIFFTAG_PAGENAME, &tmp ) ) metadata.insert( {"pagename",tmp} );
  if( TIFFGetField( tiff, TIFFTAG_SOFTWARE, &tmp ) ) metadata.insert( {"software",tmp} );
  if( TIFFGetField( tiff, TIFFTAG_MAKE, &tmp ) ) metadata.insert( {"make",tmp} );
  if( TIFFGetField( tiff, TIFFTAG_MODEL, &tmp ) ) metadata.insert( {"model",tmp} );
  if( TIFFGetField( tiff, TIFFTAG_XMLPACKET, &count, &tmp ) ) metadata.insert( {"xmp",string(tmp,count)} );
  if( TIFFGetField( tiff, TIFFTAG_ICCPROFILE, &count, &tmp ) ) metadata.insert( {"icc",string(tmp,count)} );
  if( TIFFGetField( tiff, TIFFTAG_STONITS, &scale ) ){
    char buffer[32];
    snprintf( buffer, sizeof(buffer), "%g", scale );
    metadata.insert( {"scale",buffer} );
  }
}



void TPTImage::closeImage()
{
  if( tiff != NULL ){
    TIFFClose( tiff );
    tiff = NULL;
  }
}



RawTile TPTImage::getTile( int x, int y, unsigned int res, int layers, unsigned int tile, ImageEncoding requested_encoding )
{
  uint32_t im_width, im_height, tw, th, ntlx, ntly;
  uint32_t rem_x, rem_y;
  uint16_t colour, planar, compression;
  string filename;


  // Check the resolution exists
  if( res > numResolutions ){
    ostringstream error;
    error << "TPTImage :: Asked for non-existent resolution: " << res;
    throw file_error( error.str() );
  }


  // If we are currently working on a different sequence number, then
  //  close and reload the image.
  if( stack.empty() && ( (currentX != x) || (currentY != y) ) ){
    closeImage();
  }


  // Open the TIFF if it's not already open
  if( !tiff ){
    filename = getFileName( x, y );
    if( ( tiff = TIFFOpen( filename.c_str(), mode ) ) == NULL ){
      throw file_error( "TPTImage :: TIFFOpen() failed for:" + filename );
    }
  }


  // Reload our image information in case the tile size etc is different - no need to do this for image stacks
  if( stack.empty() && ( (currentX != x) || (currentY != y) ) ){
    loadImageInfo( x, y );
  }


  // The IIP protocol defines the first resolution as the smallest, so we need to invert
  //  the requested resolution as our TIFF images are stored with the largest resolution first
  int vipsres = ( numResolutions - 1 ) - res;


  // Check in which directory we currently are
  tdir_t cd = TIFFCurrentDirectory( tiff );

  // Handle SubIFD-based resolution levels
  if( pyramid == SUBIFD ){

    // If we have an image stack within our TIFF, change to the appropriate directory
    if( (int)cd != x ){
      if( !TIFFSetDirectory( tiff, x ) ){
	ostringstream error;
	error << "TPTImage :: TIFFSetDirectory() failed for stack " << x;
	throw file_error( error.str() );
      }
      cd = x;
    }

    // Reload our SubIFD list if necessary
    if( subifds.empty() || x != (int)subifd_ifd ){
      loadSubIFDs();
      subifd_ifd = cd;
    }

    // Change to the appropriate SubIFD directory if necessary
    if( (vipsres < (int)subifds.size()) && (subifds[vipsres] > 0) ){
      if( !TIFFSetSubDirectory( tiff, subifds[vipsres] ) ){
	ostringstream error;
	error << "TPTImage :: TIFFSetSubDirectory() failed for SubIFD offset " << subifds[vipsres];
	throw file_error( error.str() );
      }
    }
  }
  // If TIFF pyramid is a "classic" image pyramid with sub-resolutions within successive IFDs, just move to the appropriate directory
  else {
    if( vipsres != (int)cd ){
      if( !TIFFSetDirectory( tiff, resolution_ids[vipsres] ) ){
	ostringstream error;
	error << "TPTImage :: TIFFSetDirectory() failed for resolution " << vipsres;
	throw file_error( error.str() );
      }
    }
  }


  // Check that a valid tile number was given
  if( tile >= TIFFNumberOfTiles( tiff ) ) {
    ostringstream tile_no;
    tile_no << "TPTImage :: Asked for non-existent tile: " << tile;
    throw file_error( tile_no.str() );
  }


  // Get the size of this tile, the size of the current resolution,
  //   the number of samples and the colorspace.
  //  TIFFGetField( tiff, TIFFTAG_TILEWIDTH, &tw );
  //  TIFFGetField( tiff, TIFFTAG_TILELENGTH, &th );
  TIFFGetField( tiff, TIFFTAG_IMAGEWIDTH, &im_width );
  TIFFGetField( tiff, TIFFTAG_IMAGELENGTH, &im_height );
  TIFFGetField( tiff, TIFFTAG_PHOTOMETRIC, &colour );
  TIFFGetField( tiff, TIFFTAG_SAMPLESPERPIXEL, &channels );
  TIFFGetField( tiff, TIFFTAG_BITSPERSAMPLE, &bpc );
  TIFFGetField( tiff, TIFFTAG_PLANARCONFIG, &planar );
  TIFFGetField( tiff, TIFFTAG_COMPRESSION, &compression );

  // Get tile size for this resolution - make sure it is tiled
  tw = tile_widths[vipsres];
  th = tile_heights[vipsres];

  if( (tw == 0) || (th == 0) ){
    throw file_error( "TPTImage :: Requested resolution is not tiled" );
  }

  // Total number of pixels in tile
  size_t np = tw * th;

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
  if( colour == PHOTOMETRIC_CIELAB ) colorspace = ColorSpace::CIELAB;
  else if( colour == PHOTOMETRIC_MINISBLACK ){
    colorspace = (bpc==1)? ColorSpace::BINARY : ColorSpace::GREYSCALE;
  }
  else if( colour == PHOTOMETRIC_PALETTE ){
    // Watch out for colourmapped images. There are stored as 1 sample per pixel,
    // but are decoded to 3 channels by libtiff, so declare them as sRGB
    colorspace = ColorSpace::GREYSCALE;
    channels = 1;
  }
  else if( colour == PHOTOMETRIC_YCBCR ){
    // JPEG encoded tiles can be subsampled YCbCr encoded. Ask to decode these to RGB
    TIFFSetField( tiff, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB );
    colorspace = ColorSpace::sRGB;
  }
  else colorspace = ColorSpace::sRGB;


  // Initialize our RawTile object
  RawTile rawtile( tile, res, x, y, tile_widths[vipsres], tile_heights[vipsres], channels, bpc );
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.sampleType = sampleType;


  // If codec pass-through is disabled or the tile needs reprocessing (cropping or change of bit depth),
  //  make sure we decode the tile to raw pixel format
  if( (IIPImage::codec_passthrough == false) ||
      (tw != tile_widths[vipsres] || th != tile_heights[vipsres]) ||
      (bpc==1 && channels==1) ){
    requested_encoding = ImageEncoding::RAW;
  }

  // Also disable pass-through for TIFF-JPEGs where YCbCr sub-sampling has been defined with an RGB color space - these
  // require the full JFIF format rather than the abreviated JPEG within the TIFF
  if( requested_encoding == ImageEncoding::JPEG && compression == COMPRESSION_JPEG ){
    uint16_t subsampling[2];
    if( (TIFFGetField( tiff, TIFFTAG_YCBCRSUBSAMPLING, &subsampling[0], &subsampling[1] ) != 0) && (colour == PHOTOMETRIC_RGB) ){
      if( IIPImage::logging ) logfile << "TPTImage :: Sub-sampled RGB JPEG-encoded TIFF: tile decoded to RAW" << endl;
      requested_encoding = ImageEncoding::RAW;
    }
  }


  // Calculate maximum amount of memory required for the tile when decoded as raw pixels values
  tsize_t bytes = TIFFTileSize( tiff );


  // If we are in pass-through mode, we need to know the exact number of bytes the tile
  // takes up as this may sometimes be larger than the raw data size
  if( ( requested_encoding == ImageEncoding::JPEG && compression == COMPRESSION_JPEG )
#ifdef HAVE_WEB
      || ( requested_encoding == ImageEncoding::WEBP && compression == COMPRESSION_WEBP )
#endif
      ){
    // Get actual number of bytes for this specific tile from the TILEBYTECOUNTS tag
    // which returns an array of all tile sizes in current IFD
    uint64_t *bytecounts;
    if( !TIFFGetField( tiff, TIFFTAG_TILEBYTECOUNTS, &bytecounts ) ){
      ostringstream error;
      error << "TPTImage :: Unable to get byte count for tile " << tile;
      throw file_error( error.str() );
    }
    bytes = bytecounts[tile];
    if( IIPImage::logging ) logfile << "TPTImage :: Byte count for compressed tile: " << bytes << endl;
  }


  // Get raw pre-encoded tile if our request matches the tile encoding - only currently makes sense for JPEG and WEBP
  if( requested_encoding == ImageEncoding::JPEG && compression == COMPRESSION_JPEG ){

    /* TIFF JPEG uses the JPEGTABLES field to store quantization and Huffman tables with image data stored
       separately in each tile.
       The JPEGTABLES tag data begins with a JPEG SOI marker (0XFF,0xD8), followed by the tables themselves
       and ending with an EOI marker (0xFF,0xD9).
       The tile data consists of image data preceeded by an SOI marker.
       To reconstruct a full JPEG image, the table and tile data need to be concatenated, but with the final
       EOI marker from the table tag and the initial SOI marker from the tile data removed
    */

    unsigned char* jpeg_tables;
    uint16_t count = 0;

    if( ( TIFFGetField( tiff, TIFFTAG_JPEGTABLES, &count, &jpeg_tables ) != 0 ) && ( count > 4 ) ){

      // Allocate enough space for compressed tile data + JPEG tables
      rawtile.allocate( bytes + count - 4 );

      // Store last 2 bytes of the JPEG table data itself for use later - skip over the final 2 byte EOI marker
      unsigned char table_end[2];
      table_end[0] = ((unsigned char*)jpeg_tables)[count-4];
      table_end[1] = ((unsigned char*)jpeg_tables)[count-3];

      // Copy tables to our RawTile buffer - ignore the final 2 byte EOI marker
      memcpy( rawtile.data, jpeg_tables, count-2 );

      // Note starting position before we add image data - rewind by extra 2 bytes as we want to temporarily
      // overwrite the end of the table data with the SOI which preceeds the image data in the tile stream
      int pos = count - 4;

      int length = TIFFReadRawTile( tiff, (ttile_t) tile, (tdata_t) &(((unsigned char*)rawtile.data)[pos]), bytes );
      if( length == -1 ){
	throw file_error( "TPTImage :: TIFFReadRawTile() failed for JPEG-encoded tile for " + getFileName( x, y ) );
      }

      // Overwrite superfluous SOI marker from tile with previously saved end of JPEG tables
      ((unsigned char*)rawtile.data)[pos]=table_end[0];
      ((unsigned char*)rawtile.data)[pos+1]=table_end[1];

      rawtile.dataLength = pos + length;
      rawtile.compressionType = ImageEncoding::JPEG;
    }
    else{
      // Throw error if no JPEG tables present
      throw file_error( "TPTImage :: Empty TIFFTAG_JPEGTABLES tag for JPEG-encoded tile for " + getFileName( x, y ) );
    }
  }

#ifdef COMPRESSION_WEBP
  else if( requested_encoding == ImageEncoding::WEBP && compression == COMPRESSION_WEBP ){

    // Allocate buffer
    rawtile.allocate( bytes );

    // Copy compressed tile directly into buffer
    int length = TIFFReadRawTile( tiff, (ttile_t) tile, (tdata_t) rawtile.data, bytes );
    if( length == -1 ){
      throw file_error( "TPTImage :: TIFFReadRawTile() failed for WebP-encoded tile for " + getFileName( x, y ) );
    }
    rawtile.dataLength = length;
    rawtile.compressionType = ImageEncoding::WEBP;
  }
#endif

  // Decode the tile into raw pixel data - dump data directly into RawTile buffer
  else{

    // Allocate buffer
    rawtile.allocate( bytes );

    // Copy decoded tile into buffer as raw pixel values
    int length = TIFFReadEncodedTile( tiff, (ttile_t) tile, (tdata_t) rawtile.data, bytes );
    if( length == -1 ){
      throw file_error( "TPTImage :: TIFFReadEncodedTile() failed for " + getFileName( x, y ) );
    }
    rawtile.dataLength = length;
    rawtile.compressionType = ImageEncoding::RAW;
  }


  // For non-interleaved channels (separate image planes), each color channel is stored as a separate image, which is stored consecutively.
  // A color image will, therefore, have 3x the number of tiles. For now just handle the first plane and classify the image as greyscale.
  if( channels > 1 && planar == PLANARCONFIG_SEPARATE ){
    if( IIPImage::logging ) logfile << "TPTImage :: Image contains separate image planes: extracting first plane only" << endl;
    rawtile.channels = 1;
  }

  // Pad 1 bit 1 channel bilevel images to 8 bits for output
  if( bpc==1 && channels==1 ){

    // Pixel index
    unsigned int n = 0;

    // Calculate number of bytes used - round integer up efficiently
    unsigned int nbytes = (np + 7) / 8;
    unsigned char *buffer = new unsigned char[np];

    // Take into account photometric interpretation:
    //   0: white is zero, 1: black is zero
    unsigned char min = (unsigned char) 0;
    unsigned char max = (unsigned char) 255;
    if( colour == 0 ){
      min = (unsigned char) 255; max = (unsigned char) 0;
    }

    // Unpack each raw byte into 8 8-bit pixels
    for( unsigned int i=0; i<nbytes; i++ ){
      unsigned char t = ((unsigned char*)rawtile.data)[i];
      // Count backwards as TIFF is usually MSB2LSB
      for( int k=7; k>=0; k-- ){
	// Set values depending on whether bit is set
	buffer[n++] = (t & (1 << k)) ? max : min;
      }
    }

    // Free old data buffer and assign pointer to new data
    rawtile.deallocate( rawtile.data );
    rawtile.data = buffer;
    rawtile.capacity = np;
    rawtile.dataLength = n;
    rawtile.bpc = 8;
  }


  // Crop our tile if necessary
  if( tw != tile_widths[vipsres] || th != tile_heights[vipsres] ) rawtile.crop( tw, th );


  return rawtile;

}



// Load any list of SubIFDs linked to this IFD
void TPTImage::loadSubIFDs()
{
  uint16_t n_subifd;
  toff_t *subifd;
  subifds.clear();
  if( TIFFGetField( tiff, TIFFTAG_SUBIFD, &n_subifd, &subifd ) == 1 ){
    if( n_subifd > 0 ){
      subifds.push_back(0);
      for( int n = 0; n<n_subifd; n++ ) subifds.push_back( subifd[n] );
    }
  }
}



// Load name and scale metadata for image stacks
void TPTImage::loadStackInfo()
{
  double scale;
  const char *tmp = NULL;

  // Reset to first TIFF directory
  if( !TIFFSetDirectory( tiff, 0 ) ) throw file_error( "TPTImage :: TIFFSetDirectory() failed" );

  // Start from 1 as horizontalAnglesList is initialized with 0 by default
  int n = 1;

  // Loop through our IFDs and get the name and scaling factor for each
  do {
    uint32_t stype;

    // Stack layers should really be in multi-page type sub file types
    if( (TIFFGetField( tiff, TIFFTAG_SUBFILETYPE, &stype ) == 1) && (stype == 0x02) ){
      Stack s;
      horizontalAnglesList.push_back(n++);
      if( TIFFGetField( tiff, TIFFTAG_DOCUMENTNAME, &tmp ) ) s.name = string(tmp);
      if( TIFFGetField( tiff, TIFFTAG_STONITS, &scale ) ) s.scale = (float) scale;
      stack.push_back( std::move(s) );
    }
  } while( TIFFReadDirectory(tiff) );

  // Need to remove last item from stack list
  if( horizontalAnglesList.size() > 1 ) horizontalAnglesList.pop_back();
}
