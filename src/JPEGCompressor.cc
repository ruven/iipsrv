/*  JPEG class wrapper to ijg jpeg library

    Copyright (C) 2000-2022 Ruven Pillay.

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
#include <cstdio>

#include "JPEGCompressor.h"


using namespace std;


// Define round() function for older MSVC compilers
#if defined _MSC_VER && _MSC_VER<1900
inline double round(double r) { return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5); }
#endif


// Minimum buffer size for output data
#define MX 65536


/* Since an ICC profile can be larger than the maximum size of a JPEG marker
 * (64K), we need provisions to split it into multiple markers.  The format
 * defined by the ICC specifies one or more APP2 markers containing the
 * following data:
 *      Identifying string      ASCII "ICC_PROFILE\0"  (12 bytes)
 *      Marker sequence number  1 for first APP2, 2 for next, etc (1 byte)
 *      Number of markers       Total number of APP2's used (1 byte)
 *      Profile data            (remainder of APP2 data)
 * Decoders should use the marker sequence numbers to reassemble the profile,
 * rather than assuming that the APP2 markers appear in the correct sequence.
 */
#define ICC_MARKER  (JPEG_APP0 + 2)     /* JPEG marker code for ICC */
#define ICC_OVERHEAD_LEN  14            /* size of non-profile data in APP2 */
#define MAX_BYTES_IN_MARKER  65533      /* maximum data len of a JPEG marker */
#define MAX_DATA_BYTES_IN_MARKER  (MAX_BYTES_IN_MARKER - ICC_OVERHEAD_LEN)


// XMP definitions
#define XMP_PREFIX "http://ns.adobe.com/xap/1.0/%c%s"
#define XMP_PREFIX_SIZE 29



/* My version of the JPEG error_exit function. We want to pass control back
   to the program, so simply throw an exception
*/

METHODDEF(void) iip_error_exit( j_common_ptr cinfo )
{
  char buffer[ JMSG_LENGTH_MAX ];

  // Create the message
  (*cinfo->err->format_message) ( cinfo, buffer );

  // Let the memory manager delete any temp files before we die
  jpeg_destroy( cinfo );

  // Throw an exception rather than print out a message and exit
  throw string( buffer );
}



extern "C" {
  void setup_error_functions( jpeg_compress_struct *a ){
    a->err->error_exit = iip_error_exit; 
  }
}




/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

METHODDEF(void)
iip_init_destination (j_compress_ptr cinfo)
{
  iip_dest_ptr dest = (iip_dest_ptr) cinfo->dest;

  // Number of bytes written
  dest->written = 0;
  
  // Set compressor pointers for library
  dest->pub.next_output_byte = dest->source;
  dest->pub.free_in_buffer = dest->source_size;
}




METHODDEF(boolean)
iip_empty_output_buffer( j_compress_ptr cinfo )
{
  iip_dest_ptr dest = (iip_dest_ptr) cinfo->dest;

  // If we reach here, our output tile buffer must be too small, so reallocate
  unsigned int new_size = dest->source_size*2;
  unsigned char *source = new unsigned char[new_size];
  memcpy( source, dest->source, dest->source_size );

  // Swap buffers
  delete[] dest->source;
  dest->source = source;

  // Reset the pointer to the beginning of the buffer
  dest->written += dest->source_size;
  dest->pub.free_in_buffer = new_size - dest->written;
  dest->pub.next_output_byte = &(dest->source[dest->source_size]);
  dest->source_size = new_size;

  return TRUE;
}




/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

void iip_term_destination( j_compress_ptr cinfo )
{
  iip_dest_ptr dest = (iip_dest_ptr) cinfo->dest;

  // Update the number of bytes written
  dest->written = dest->source_size - dest->pub.free_in_buffer;
}




void JPEGCompressor::InitCompression( const RawTile& rawtile, unsigned int strip_height )
{
  // Do some initialisation
  dest = &dest_mgr;

  // Set up the correct width and height for this particular tile
  width = rawtile.width;
  height = rawtile.height;
  channels = rawtile.channels;


  // Make sure we only try to compress images with 1 or 3 channels
  if( ! ( (channels==1) || (channels==3) )  ){
    throw string( "JPEGCompressor: JPEG can only handle images of either 1 or 3 channels" );
  }

  // JPEG can only handle 8 bit data
  if( rawtile.bpc != 8 ) throw string( "JPEGCompressor: JPEG can only handle 8 bit images" );


  // We set up the normal JPEG error routines, then override error_exit.
  cinfo.err = jpeg_std_error( &jerr );


  // Override the error_exit function with our own.
  // Hmmm, we have to do this assignment in C due to the strong type checking of C++
  //  or something like that. So, we use an extern "C" function declared at the top
  //  of this file and pass our arguments through this. I'm sure there's a better
  //  way of doing this, but this seems to work :/

  //   cinfo.err.error_exit = iip_error_exit;
  setup_error_functions( &cinfo );

  jpeg_create_compress( &cinfo );


  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same file without re-executing jpeg_stdio_dest.
   * This makes it dangerous to use this manager and a different destination
   * manager serially with the same JPEG object, because their private object
   * sizes may be different.  Caveat programmer.
   */
  if( !cinfo.dest ){
    // first time for this JPEG object?
    cinfo.dest = ( struct jpeg_destination_mgr* )
      ( *cinfo.mem->alloc_small )
      ( (j_common_ptr) &cinfo, JPOOL_PERMANENT, sizeof( iip_destination_mgr ) );
  }


  dest = (iip_dest_ptr) cinfo.dest;
  dest->pub.init_destination = iip_init_destination;
  dest->pub.empty_output_buffer = iip_empty_output_buffer;
  dest->pub.term_destination = iip_term_destination;
  dest->strip_height = strip_height;

  // Calculate our metadata storage requirements
  unsigned int metadata_size =
    (icc.size()>0 ? (icc.size()+ICC_OVERHEAD_LEN) : 0) +
    (xmp.size()>0 ? (xmp.size()+XMP_PREFIX_SIZE) : 0);

  // Allocate enough memory for our header and metadata
  unsigned long output_size = metadata_size + MX;
  header = new unsigned char[output_size];
  dest->source = header;
  dest->source_size = output_size;

  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = channels;
  cinfo.in_color_space = ( channels == 3 ? JCS_RGB : JCS_GRAYSCALE );
  jpeg_set_defaults( &cinfo );

  // Set our physical output resolution (JPEG only supports integers)
  cinfo.X_density = round( dpi_x );
  cinfo.Y_density = round( dpi_y );
  cinfo.density_unit = dpi_units;

  // Set compression point quality (highest, but possibly slower depending
  //  on hardware) - must do this after we've set the defaults!
  cinfo.dct_method = JDCT_FASTEST;

  jpeg_set_quality( &cinfo, Q, TRUE );

  jpeg_start_compress( &cinfo, TRUE );

  // Add an identifying comment
  jpeg_write_marker( &cinfo, JPEG_COM, (const JOCTET*) "Generated by IIPImage", 21 );

  // Embed ICC profile if one is supplied
  writeICCProfile();

  // Add XMP metadata
  writeXMPMetadata();
  
  // Copy the encoded JPEG header data to a separate buffer
  size_t datacount = dest->source_size - dest->pub.free_in_buffer;
  header_size = datacount;

}



/*
  We use a separate tile_height from the predefined strip_height because
  the tile height for the final row can be different
 */
unsigned int JPEGCompressor::CompressStrip( unsigned char* input, unsigned char* output, unsigned int tile_height )
{
  JSAMPROW row[1];
  int row_stride = width * channels;

  // First delete our header if we have one
  if( header_size > 0 && header ){
    delete[] header;
    header = NULL;
    header_size = 0;
  }

  // Setup our destination manager
  dest->source = output;
  dest->source_size = tile_height*width*channels + MX;
  dest->pub.next_output_byte = output;
  dest->pub.free_in_buffer = dest->source_size;

  // Reset our scanline index
  cinfo.next_scanline = 0;

  // Now write out our scanlines
  while( cinfo.next_scanline < tile_height ) {
    row[0] = &input[ cinfo.next_scanline * row_stride ];
    jpeg_write_scanlines( &cinfo, row, 1 );
  }

  // Return the number of bytes written
  return (dest->source_size - dest->pub.free_in_buffer);

}




unsigned int JPEGCompressor::Finish( unsigned char* output )
{
  // Reset pointers
  dest->source = output;
  dest->pub.next_output_byte = dest->source;
  dest->pub.free_in_buffer = dest->source_size;

  // Need to set the scanline to the end for jpeg_finish_compress() to work
  cinfo.next_scanline = dest->strip_height;

  // Terminate the compression
  jpeg_finish_compress( &cinfo );

  // Calculate size of final data to be written before destroying our compression structures
  unsigned long dataLength = dest->source_size - dest->pub.free_in_buffer;

  // Destroy our compression structure
  jpeg_destroy_compress( &cinfo );

  // Return number of bytes written
  return ( dataLength );
}




unsigned int JPEGCompressor::Compress( RawTile& rawtile )
{
  // Do some initialisation
  data = (unsigned char*) rawtile.data;
  struct jpeg_error_mgr jerr;
  iip_destination_mgr dest_mgr;
  iip_dest_ptr dest = &dest_mgr;


  // Set up the correct width and height for this particular tile
  width = rawtile.width;
  height = rawtile.height;
  channels = rawtile.channels;


  // Make sure we only try to compress images with 1 or 3 channels
  if( ! ( (channels==1) || (channels==3) ) ){
    throw string( "JPEGCompressor: JPEG can only handle images of either 1 or 3 channels" );
  }

  // JPEG can only handle 8 bit data
  if( rawtile.bpc != 8 ) throw string( "JPEGCompressor: JPEG can only handle 8 bit images" );

  // We set up the normal JPEG error routines, then override error_exit.
  cinfo.err = jpeg_std_error( &jerr );


  // Override the error_exit function with our own.
  // Hmmm, we have to do this assignment in C due to the strong type checking of C++
  //  or something like that. So, we use an extern "C" function declared at the top
  //  of this file and pass our arguments through this. I'm sure there's a better
  //  way of doing this, but this seems to work :/

  //   cinfo.err.error_exit = iip_error_exit;
  setup_error_functions( &cinfo );

  jpeg_create_compress( &cinfo );


  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same file without re-executing jpeg_stdio_dest.
   * This makes it dangerous to use this manager and a different destination
   * manager serially with the same JPEG object, because their private object
   * sizes may be different.  Caveat programmer.
   */
  if( !cinfo.dest ){

    // first time for this JPEG object?
    cinfo.dest = ( struct jpeg_destination_mgr* )
      ( *cinfo.mem->alloc_small )
      ( (j_common_ptr) &cinfo, JPOOL_PERMANENT, sizeof( iip_destination_mgr ) );
  }


  dest = (iip_dest_ptr) cinfo.dest;
  dest->pub.init_destination = iip_init_destination;
  dest->pub.empty_output_buffer = iip_empty_output_buffer;
  dest->pub.term_destination = iip_term_destination;
  dest->strip_height = 0;

  // Calculate our metadata storage requirements
  unsigned int metadata_size =
    (icc.size()>0 ? (icc.size()+ICC_OVERHEAD_LEN) : 0) +
    (xmp.size()>0 ? (xmp.size()+XMP_PREFIX_SIZE) : 0);

  // Allocate enough memory for our compressed output data
  // - compressed images at overly high quality factors can be larger than raw data
  unsigned long output_size = (unsigned long)( (unsigned int)(width*height*channels*1.5) + metadata_size );
  dest->source = new unsigned char[output_size]; // Add some extra buffering
  dest->source_size = output_size;

  // Set image information
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = channels;
  cinfo.in_color_space = ( channels == 3 ? JCS_RGB : JCS_GRAYSCALE );
  jpeg_set_defaults( &cinfo );

  // Set our physical output resolution (JPEG only supports integers)
  cinfo.X_density = round( dpi_x );
  cinfo.Y_density = round( dpi_y );
  cinfo.density_unit = dpi_units;
  
  // Set compression quality (fastest, but possibly slower depending
  //  on hardware) - must do this after we've set the defaults!
  cinfo.dct_method = JDCT_FASTEST;

  jpeg_set_quality( &cinfo, Q, TRUE );

  jpeg_start_compress( &cinfo, TRUE );
  
  // Add an identifying comment
  jpeg_write_marker( &cinfo, JPEG_COM, (const JOCTET*) "Generated by IIPImage", 21 );

  // Embed ICC profile if one is supplied
  writeICCProfile();

  // Add XMP metadata
  writeXMPMetadata();

  // Send the tile data
  int row_stride = width * channels;

  // Compress the image line by line
  JSAMPROW row[1];
  while( cinfo.next_scanline < cinfo.image_height ){
    row[0] = &data[ cinfo.next_scanline * row_stride ];
    jpeg_write_scanlines( &cinfo, row, 1 );
  }

  // Tidy up, get the compressed data size and de-allocate memory
  jpeg_finish_compress( &cinfo );

  // Check that we have enough memory in our Rawtile for the JPEG data.
  // This can happen on small tiles with high quality factors. If so delete and reallocate memory.
  unsigned long dataLength;
  dataLength = dest->written;
  if( dataLength > rawtile.dataLength ){
    delete[] (unsigned char*) rawtile.data;
    rawtile.data = new unsigned char[dataLength];
  }
  
  // Copy memory back to the tile
  memcpy( rawtile.data, dest->source, dataLength );
  delete[] dest->source;
  jpeg_destroy_compress( &cinfo );


  // Set the tile compression parameters
  rawtile.dataLength = dataLength;
  rawtile.compressionType = JPEG;
  rawtile.quality = Q;


  // Return the size of the data we have compressed
  return dataLength;

}



// Write ICC profile into JPEG header if profile has been set
// Function *must* be called AFTER calling jpeg_start_compress() and BEFORE
// the first call to jpeg_write_scanlines().
// (This ordering ensures that the APP2 marker(s) will appear after the
// SOI and JFIF or Adobe markers, but before all else.)
//
// The ICC write function is based on an implementation by the Independent JPEG Group
// See the copyright notice in COPYING.ijg for details
void JPEGCompressor::writeICCProfile()
{
  unsigned int num_markers;     // total number of markers we'll write
  int cur_marker = 1;           // per spec, counting starts at 1
  unsigned int length;          // number of bytes to write in this marker

  // Skip if our profile has zero size or is too big
  //  if( icc.size() == 0 || icc.size() > MAX_DATA_BYTES_IN_MARKER ) return;
  if( icc.size() == 0 ) return;

  unsigned int icc_data_len = icc.size();
  const char* icc_data_ptr = icc.c_str();
  
  // Calculate the number of markers we'll need, rounding up of course
  num_markers = icc_data_len / MAX_DATA_BYTES_IN_MARKER;
  if( num_markers * MAX_DATA_BYTES_IN_MARKER != icc_data_len ){
    num_markers++;
  }

  while( icc_data_len > 0 ){

    // Length of profile to put in this marker
    length = icc_data_len;
    if( length > MAX_DATA_BYTES_IN_MARKER ){
      length = MAX_DATA_BYTES_IN_MARKER;
    }
    icc_data_len -= length;
    
    // Write the JPEG marker header (APP2 code and marker length)
    jpeg_write_m_header( &cinfo, ICC_MARKER,
			 (unsigned int) (length + ICC_OVERHEAD_LEN) );

    // Write the marker identifying string "ICC_PROFILE" (null-terminated).
    // We code it in this less-than-transparent way so that the code works
    // even if the local character set is not ASCII.
    jpeg_write_m_byte(&cinfo, 0x49);
    jpeg_write_m_byte(&cinfo, 0x43);
    jpeg_write_m_byte(&cinfo, 0x43);
    jpeg_write_m_byte(&cinfo, 0x5F);
    jpeg_write_m_byte(&cinfo, 0x50);
    jpeg_write_m_byte(&cinfo, 0x52);
    jpeg_write_m_byte(&cinfo, 0x4F);
    jpeg_write_m_byte(&cinfo, 0x46);
    jpeg_write_m_byte(&cinfo, 0x49);
    jpeg_write_m_byte(&cinfo, 0x4C);
    jpeg_write_m_byte(&cinfo, 0x45);
    jpeg_write_m_byte(&cinfo, 0x0);

    // Add the sequencing info
    jpeg_write_m_byte( &cinfo, cur_marker );
    jpeg_write_m_byte( &cinfo, (int) num_markers );

    // Write the profile data byte by byte
    while( length-- ){
      jpeg_write_m_byte(&cinfo, *icc_data_ptr);
      icc_data_ptr++;
    }
    cur_marker++;
  }
}



void JPEGCompressor::writeXMPMetadata()
{
  // Make sure our XMP data has a valid size (namespace prefix is 29 bytes)
  if( xmp.size()==0 || xmp.size()>(65536-XMP_PREFIX_SIZE) ) return;

  // The XMP data in a JPEG stream needs to be prefixed with a zero-terminated ID string
  // ref http://www.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/cs6/XMPSpecificationPart3.pdf (pp13-14)
  // so have to copy it into a larger buffer
  char xmpstr[65536];	// per spec, XMP max size is 65502; namespace prefix/id is 29 bytes

  // '0' should be 0, but snprintf is too smart...
  snprintf( xmpstr, 65536, XMP_PREFIX, '\0', xmp.c_str() );

  // Can't use regular addMetadata, because of the zero term after the namespace id; and the APP1 marker
  jpeg_write_marker( &cinfo, JPEG_APP0+1, (const JOCTET*) xmpstr, XMP_PREFIX_SIZE + xmp.size() );
}
