/*  IIPImage Server: JPEG reader

    Copyright (C) 2024-2025 Ruven Pillay.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "JPEGImage.h"
#include "Logger.h"
#include <cstdio>
#include <sstream>
#include <cmath>
#ifdef JPEG_DEBUG
#include "Timer.h"
#endif


using namespace std;


// Reference our logging object
extern Logger logfile;


METHODDEF(void) iip_error_exit( j_common_ptr cinfo )
{
  // Output the message
  (*cinfo->err->output_message) (cinfo);

  // Let the memory manager delete any temp files before we die
  jpeg_destroy( cinfo );

  // Throw an exception rather than print out a message and exit
  throw file_error( "JPEG :: aborting" );
}


METHODDEF(void) iip_output_message( j_common_ptr cinfo )
{
  char buffer[ JMSG_LENGTH_MAX ];

  // Create the message
  (*cinfo->err->format_message) ( cinfo, buffer );

  // Print to logfile
  logfile << "JPEG :: " << buffer << endl;
}



void JPEGImage::openImage()
{
  string filename = getFileName( currentX, currentY );

  // Update our timestamp
  updateTimestamp( filename );

#ifdef JPEG_DEBUG
  Timer timer;
  timer.start();
#endif

  if( _input == NULL ){
    if( (_input = fopen(filename.c_str(), "rb") ) == NULL ){
      throw file_error( "Unable to open file: " + filename );
    }
  }

  // Create our decompress object
  jpeg_create_decompress( &cinfo );

  // Set error handling
  cinfo.err = jpeg_std_error( &jerr );
  jerr.error_exit = iip_error_exit;
  jerr.output_message = iip_output_message;

  // Assign our input file descriptor
  jpeg_stdio_src( &cinfo, _input );

  // Read header and save some markers
  jpeg_save_markers( &cinfo, JPEG_APP0+2, 0xFFFF ); // ICC marker
  jpeg_save_markers( &cinfo, JPEG_APP0+1, 0xFFFF ); // XMP marker
  jpeg_read_header( &cinfo, TRUE );

  // Load our metadata if not already loaded
  if( bpc == 0 ) loadImageInfo( currentX, currentY );

#ifdef JPEG_DEBUG
  logfile << "JPEG :: openImage() :: " << timer.getTime() << " microseconds" << endl;
#endif

}



void JPEGImage::closeImage(){

#ifdef JPEG_DEBUG
  Timer timer;
  timer.start();
#endif

  jpeg_destroy_decompress( &cinfo );
  if( _input ){
    fclose( _input );
    _input = NULL;
  }

#ifdef JPEG_DEBUG
  logfile << "JPEG :: closeImage() :: " << timer.getTime() << " microseconds" << endl;
#endif
}



void JPEGImage::loadImageInfo( int seq, int ang ){

#ifdef JPEG_DEBUG
  Timer timer;
  timer.start();
#endif

  // Save first resolution level
  image_widths.clear();
  image_heights.clear();
  unsigned int w = cinfo.image_width;
  unsigned int h = cinfo.image_height;
  image_widths.push_back(w);
  image_heights.push_back(h);
#ifdef JPEG_DEBUG
  logfile << "JPEG :: Full resolution    : " << w << "x" << h << endl;
#endif

  // The JPEG format has no concept of sub-resolutions, so need to create these virtually ourselves
  // Shrink until the resolution fits into a single tile
  w = image_widths[0];
  h = image_heights[0];

  while( (w>tile_widths[0]) || (h>tile_heights[0]) ){
    w = floor( w/2.0 );
    h = floor( h/2.0 );
    image_widths.push_back(w);
    image_heights.push_back(h);
#ifdef JPEG_DEBUG
    logfile << "JPEG :: Virtual resolution : " << w << "x" << h << endl;
#endif
  }

  numResolutions = image_widths.size();
  channels = cinfo.num_components;
  bpc = cinfo.data_precision;;

  if( cinfo.X_density ) dpi_x = cinfo.X_density;
  if( cinfo.Y_density ) dpi_y = cinfo.Y_density;
  if( cinfo.density_unit ) dpi_units = cinfo.density_unit;

  // Need to assign basic colorspace information
  if( channels == 1 ) colorspace = ColorSpace::GREYSCALE;
  else if( channels == 3 ) colorspace = ColorSpace::sRGB;

#ifdef JPEG_DEBUG
  logfile << "JPEG :: " << bpc << " bit data" << endl
	  << "JPEG :: " << channels << " channels" << endl;
  if( dpi_x || dpi_y ) logfile << "JPEG :: " << dpi_x << "x" << dpi_y
			       << " pixels/" << ((dpi_units==1)?"inch":"cm") << endl;
#endif

  // Extract ICC profile
  JOCTET *icc = NULL;
  unsigned int icc_length;
  if( jpeg_read_icc_profile ( &cinfo, &icc, &icc_length ) == TRUE ){
    metadata.insert( {"icc", string((const char*)icc,icc_length)} );
    // Buffer is allocated by libjpeg but not freed - need to free it ourselves
    free( icc );
#ifdef JPEG_DEBUG
    logfile << "JPEG :: ICC Profile found with size " << icc_length << endl;
#endif
  }

  // Extract XMP and EXIF metadata
  jpeg_saved_marker_ptr p;
  for( p = cinfo.marker_list; p; p = p->next ){
    // Look for initial XMP prefix which should start with "http://ns.adobe.com/xap"
    if( p->data_length > 4 && (memcmp( "http", p->data, 4 )==0) ){
      unsigned int len = p->data_length-2 < 80 ? p->data_length-2 : 80;
      unsigned int i;
      // Look for null byte in initial part of sequence, which separates XMP prefix from data
      for( i=0; i<len; i++ ){
	if( !p->data[i] ) break;
      }
      if( !p->data[i] ){
	len = p->data_length - i - 1;
	unsigned char* ptr = p->data + i + 1;
	// Insert into metadata map
	metadata.insert( {"xmp",string((const char*)ptr,len)} );
#ifdef JPEG_DEBUG
	logfile << "JPEG :: XMP marker found with size " << len << endl;
#endif
      }
    }
    else if( p->data_length > 4 && (memcmp( "Exif", p->data, 4 )==0) ){
      // Strip off 6 byte 'Exif\0\0' prefix
      unsigned char* ptr = p->data + 6;
      int len = p->data_length-6;
      metadata.insert( {"exif",string((const char*)ptr,len)} );
#ifdef JPEG_DEBUG
	logfile << "JPEG :: EXIF marker found with size " << len << endl;
#endif
    }
  }

  // Set the max and min values for our data type
  for( unsigned int i=0; i<channels; i++ ){
    min.push_back( 0.0 );
    max.push_back( 255.0 );
  }

  // Indicate that our metadata has been read
  isSet = true;

#ifdef JPEG_DEBUG
  logfile << "JPEG :: loadImageInfo() :: " << timer.getTime() << " microseconds" << endl;
#endif
}



// Get an individual tile
RawTile JPEGImage::getTile( int seq, int ang, unsigned int res, int layers, unsigned int tile, ImageEncoding e ){

#ifdef JPEG_DEBUG
  Timer timer;
  timer.start();
#endif

  if( res > numResolutions ){
    ostringstream tile_no;
    tile_no << "JPEG :: Asked for non-existent resolution: " << res;
    throw file_error( tile_no.str() );
  }

  int vipsres = getNativeResolution( res );

  unsigned int tw = tile_widths[0];
  unsigned int th = tile_heights[0];

  // Get the width and height for last row and column tiles
  unsigned int rem_x = image_widths[vipsres] % tile_widths[0];
  unsigned int rem_y = image_heights[vipsres] % tile_heights[0];

  // Calculate the number of tiles in each direction
  unsigned int ntlx = (image_widths[vipsres] / tile_widths[0]) + (rem_x == 0 ? 0 : 1);
  unsigned int ntly = (image_heights[vipsres] / tile_heights[0]) + (rem_y == 0 ? 0 : 1);

  // Check whether requested tile exists
  if( tile >= ntlx*ntly ){
    ostringstream tile_no;
    tile_no << "JPEG :: Asked for non-existent tile: " << tile;
    throw file_error( tile_no.str() );
  }

  // Alter the tile size if it's in the last column
  if( ( tile % ntlx == ntlx - 1 ) && ( rem_x != 0 ) ) {
    tw = rem_x;
  }

  // Alter the tile size if it's in the bottom row
  if( ( tile / ntlx == ntly - 1 ) && rem_y != 0 ) {
    th = rem_y;
  }

  // Calculate the pixel offsets for this tile
  int xoffset = (tile % ntlx) * tile_widths[0];
  int yoffset = (unsigned int) floor((double)(tile/ntlx)) * tile_heights[0];

#ifdef JPEG_DEBUG
  logfile << "JPEG :: Tile size: " << tw << "x" << th << " @" << channels << endl;
#endif

  // Create our Rawtile object and initialize with data
  RawTile rawtile( tile, res, seq, ang, tw, th, channels, 8 );
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.allocate();

  // Process the tile
  process( res, layers, xoffset, yoffset, tw, th, rawtile.data );

#ifdef JPEG_DEBUG
  logfile << "JPEG :: getTile() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;
}



// Get an entire region and not just a tile
RawTile JPEGImage::getRegion( int ha, int va, unsigned int res, int layers, int x, int y, unsigned int w, unsigned int h ){

#ifdef JPEG_DEBUG
  Timer timer;
  timer.start();
#endif

  RawTile rawtile( 0, res, ha, va, w, h, channels, 8 );
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.allocate();

  process( res, layers, x, y, w, h, rawtile.data );

#ifdef JPEG_DEBUG
  logfile << "JPEG :: getRegion() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;
}



// Main processing function for both tiles and regions
void JPEGImage::process( unsigned int res, int layers, int xoffset, int yoffset, unsigned int tw, unsigned int th, void *d )
{
  // Our effective position and size may not end up matching the requested values, so assign temporary variables
  unsigned int x0 = xoffset;
  unsigned int y0 = yoffset;
  unsigned int w0 = tw;
  unsigned int h0 = th;

  // The libjpeg API can perform efficient scaling on the raw DCT co-efficients.
  // The supported scaling ratios are M/8 with all M from 1 to 16. In other words
  // we can shrink to a max of 1/8.
  // Note that 1/8 is the 4th resolution level with index=3 (0=1/1, 1=1/2, 2=1/4, 3=1/8)
  // So calculate which libjpeg scaling to use and what extra scaling, if any, we also need to
  // apply ourselves.
  unsigned int factor = 1;                  // Downsampling factor - set it to default value
  int vipsres = getNativeResolution( res );

  if( vipsres > 3 ){
    int scale = vipsres-3;
    factor = 1 << scale;
    x0 = x0 << scale;
    y0 = y0 << scale;
    w0 = w0 << scale;
    h0 = h0 << scale;

    // Clamp to 1/8 scaling, which is the 4th resolution level with index=3 (0=1/1, 1=1/2, 2=1/4, 3=1/8)
    vipsres = 3;

#ifdef JPEG_DEBUG
    logfile << "JPEG :: Using sub-resolution at scale 1/8 with position " << x0 << "x" << y0
	    << " and size " << w0 << "x" << h0 << endl;
#endif
  }


  // Configure libjpeg for fastest possible decoding
  cinfo.dct_method = JDCT_FASTEST;
  cinfo.do_fancy_upsampling = FALSE;
  cinfo.two_pass_quantize = FALSE;
  cinfo.dither_mode = JDITHER_ORDERED;

  // Set libjpeg scaling (limited to 1/8)
  cinfo.scale_num = 1;
  cinfo.scale_denom = 1 << vipsres;

  // Start decompression
  jpeg_start_decompress( &cinfo );

  // Clip to our edges
  if( x0+w0 > image_widths[vipsres] ) w0 = image_widths[vipsres] - x0;

  // Use libjpeg API to efficiently offset and crop a particular area and avoid decoding entire image
  // Note that jpeg_crop_scanline() can modify provided coordinates if request is not aligned to iMCU block
  JDIMENSION xx, ww;
  xx = x0;
  ww = w0;
  jpeg_crop_scanline( &cinfo, &xx, &ww );
  jpeg_skip_scanlines( &cinfo, (JDIMENSION) y0 );

#ifdef JPEG_DEBUG
  if( xx != x0 ) logfile << "JPEG :: region request not aligned to JPEG block: " << x0 << " vs " << xx << endl;
#endif

  // Clip to our image height
  if( y0+h0 > cinfo.output_height ) h0 = cinfo.output_height - y0;


  // Create pointers to our data
  unsigned char* buffer = NULL;
  unsigned char* dst_ptr = NULL;


  // Byte width of the line of pixels we want
  unsigned int tile_stride = w0 * cinfo.output_components;


  // If we have to perform manual re-scaling, create a temporary buffer large enough for the data
  if( factor > 1 ){
    buffer = (unsigned char*) malloc( tile_stride * h0 );
    if( !buffer ){
      jpeg_abort_decompress( &cinfo );
      throw string( "JPEGImage :: Unable to allocate memory for tile" );
    }
    dst_ptr = buffer;
  }
  else dst_ptr = (unsigned char*) d;


  // We may need to crop some of the image, as the region returned by libjpeg's crop function
  // has to fall on an iMCU boundary so calculate offset from left
  size_t left = (x0 - xx) * cinfo.output_components;


  // If we have a perfectly aligned block and we don't need to shrink manually
  // just place our decoded pixels directly into our final Rawtile buffer
  if( factor == 1 && left == 0 ){
    for( uint32_t j=0; j<h0; j++ ){
      jpeg_read_scanlines( &cinfo, &dst_ptr, 1 );
      dst_ptr += tile_stride;
    }
    dst_ptr = NULL;
  }

  // Otherwise, create a temporary buffer for a single line of decoded pixels so that
  // we can crop the left - requires an extra memcpy() call, however.
  // Note that buffer needs to be large enough to hold a whole line of pixels, but
  // this may be larger (ww) than our requested region (w0) due to iMCU block alignment
  else{
    unsigned char* dptr = dst_ptr;
    unsigned int row_stride = ww * cinfo.output_components;
    unsigned char *line = new unsigned char[row_stride];

    for( uint32_t j=0; j<h0; j++ ){
      jpeg_read_scanlines( &cinfo, &line, 1 );
      memcpy( dptr, (line+left), tile_stride );
      dptr += tile_stride;
    }
    delete[] line;
  }


  // Need to skip to end of image to avoid libjpeg warning
  cinfo.output_scanline = cinfo.output_height;

  // Finish our decompress
  jpeg_finish_decompress( &cinfo );


  // libjpeg can natively handle up to a 1/8 shrink (vipsres=3), so rescale down further manually
  if( factor > 1 ){

#ifdef JPEG_DEBUG
    logfile << "JPEG :: Extra shrink by factor " << factor << endl;
#endif

    // Fast shrink our data by simply skipping pixels
    size_t n = 0;
    for( unsigned int j=0; j<h0; j+=factor ){
      for( unsigned int i=0; i<w0; i+=factor ){
	size_t index = w0*j*channels + i*channels;
	for( unsigned int k=0; k<channels; k++ ){
	  ((unsigned char*)d)[n++] = dst_ptr[index++];
	}
      }
    }

    // Free temporary buffer we created for when factor > 1
    if( buffer ) free( buffer );
  }

}
