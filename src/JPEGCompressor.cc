/*  JPEG class wrapper to ijg jpeg library

    Copyright (C) 2000-2026 Ruven Pillay

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


#include <new>
#include <cmath>
#include <limits>
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
#define XMP_PREFIX "http://ns.adobe.com/xap/1.0/"
#define XMP_PREFIX_SIZE 29

// EXIF definitions
#define EXIF_PREFIX "Exif\0\0"
#define EXIF_PREFIX_SIZE 6


/* IIPImage version of the JPEG error_exit function. We want to pass control back
   to the program, so simply throw an exception
*/
METHODDEF(void) iip_error_exit( j_common_ptr cinfo )
{
  char buffer[ JMSG_LENGTH_MAX ];

  // Create the message
  (*cinfo->err->format_message) ( cinfo, buffer );

  // Throw an exception rather than print out a message and exit.
  // Any memory is released by the JPEGCompressor destructor (RAII).
  throw string( buffer );
}


extern "C" {
  /* Hmmm, we have to do this assignment in C due to the strong type checking of C++
   *  or something like that. So, we use an extern "C" function declared at the top
   *  of this file and pass our arguments through this. I'm sure there's a better
   *  way of doing this, but this seems to work :/
   */
  void setup_error_functions( jpeg_compress_struct *a ){
    a->err->error_exit = iip_error_exit;
  }
}



/* Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */
METHODDEF(void)
iip_init_destination (j_compress_ptr cinfo)
{
  iip_destination_mgr* dest = (iip_destination_mgr*) cinfo->dest;

  // Number of bytes written
  dest->written = 0;
  
  // Set compressor pointers for library
  dest->pub.next_output_byte = dest->source;
  dest->pub.free_in_buffer = dest->source_size;
}



METHODDEF(boolean)
iip_empty_output_buffer( j_compress_ptr cinfo )
{
  iip_destination_mgr* dest = (iip_destination_mgr*) cinfo->dest;

  // If the destination buffer is not owned by us, we must not grow it:
  // freeing a caller-owned buffer would corrupt memory. Signal an error
  // instead, which libjpeg reports via error_exit.
  if( !dest->owns_source ) return FALSE;

  // If we reach here, our output tile buffer must be too small, so reallocate
  size_t new_size = dest->source_size*2;
  if( new_size <= dest->source_size ) return FALSE;   // size_t overflow guard
  unsigned char *source = new(std::nothrow) unsigned char[new_size];
  if( source == NULL ) return FALSE;
  memcpy( source, dest->source, dest->source_size );

  // Swap buffers
  delete[] dest->source;
  dest->source = source;

  // Reset the pointer to the beginning of the buffer
  dest->pub.free_in_buffer = new_size - dest->source_size;
  dest->pub.next_output_byte = &(dest->source[dest->source_size]);
  dest->source_size = new_size;

  return TRUE;
}



/* Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */
void iip_term_destination( j_compress_ptr cinfo )
{
  iip_destination_mgr* dest = (iip_destination_mgr*) cinfo->dest;

  // Update the number of bytes written
  dest->written = dest->source_size - dest->pub.free_in_buffer;
}



void JPEGCompressor::InitCompression( const RawTile& rawtile, unsigned int strip_height )
{
  // Make sure we only try to compress images with 1 or 3 channels
  if( ! ( (rawtile.channels==1) || (rawtile.channels==3) ) ){
    throw string( "JPEGCompressor: JPEG can only handle images of either 1 or 3 channels" );
  }

  // JPEG can only handle 8 bit data
  if( rawtile.bpc != 8 ) throw string( "JPEGCompressor: JPEG can only handle 8 bit images" );


  // Initialize our destination manager
  dest->pub.init_destination = iip_init_destination;
  dest->pub.empty_output_buffer = iip_empty_output_buffer;
  dest->pub.term_destination = iip_term_destination;
  dest->strip_height = strip_height;

  // We set up the normal JPEG error routines, then override error_exit. Must be done before calling jpeg_create_compress()
  cinfo.err = jpeg_std_error( &jerr );

  // Override the error_exit function with our own.
  setup_error_functions( &cinfo );

  // Create compression object
  jpeg_create_compress( &cinfo );

  // Assign our destination manager to the cinfo object
  cinfo.dest = (jpeg_destination_mgr*) dest;


  // Set up the correct width and height for this particular tile
  width = rawtile.width;
  height = rawtile.height;
  channels = rawtile.channels;

  // Calculate our metadata storage requirements
  unsigned long metadata_size = metadataSize();

  // Allocate enough memory for our header and metadata
  unsigned long output_size = metadata_size + MX;
  if( header ){ delete[] header; header = NULL; }
  header_size = 0;
  dest->source = new unsigned char[output_size];
  dest->source_size = output_size;
  dest->owns_source = true;

  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = channels;
  cinfo.in_color_space = ( channels == 3 ? JCS_RGB : JCS_GRAYSCALE );
  jpeg_set_defaults( &cinfo );

  // Add DPI
  writeResolution();

  // Set DCT method to fastest - must do this after we've set the defaults!
  cinfo.dct_method = JDCT_FASTEST;

  jpeg_set_quality( &cinfo, Q, TRUE );

  jpeg_start_compress( &cinfo, TRUE );

  // Add EXIF metadata
  writeExifMetadata();

  // Add XMP metadata
  writeXMPMetadata();

  // Embed ICC profile if one is supplied
  writeICCProfile();

  // Add an identifying comment
  const char *comment = "iipsrv/" VERSION;
  jpeg_write_marker( &cinfo, JPEG_COM, (const JOCTET*) comment, strlen(comment) );

  // Copy the encoded JPEG header data into our own independent buffer
  size_t datacount = dest->source_size - dest->pub.free_in_buffer;
  header = new unsigned char[datacount];
  memcpy( header, dest->source, datacount );
  header_size = datacount;

  // The scratch buffer is no longer needed
  delete[] dest->source;
  dest->source = NULL;
  dest->owns_source = false;
}



/* We use a separate tile_height from the predefined strip_height because
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
  dest->owns_source = false;
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
  dest->owns_source = false;
  dest->pub.next_output_byte = dest->source;
  dest->pub.free_in_buffer = dest->source_size;

  // Need to set the scanline to the end for jpeg_finish_compress() to work
  cinfo.next_scanline = dest->strip_height;

  // Terminate the compression
  jpeg_finish_compress( &cinfo );

  // Calculate size of final data to be written before destroying our compression structures
  uint32_t dataLength = dest->source_size - dest->pub.free_in_buffer;

  // Destroy our compression structure
  jpeg_destroy_compress( &cinfo );

  // Return number of bytes written
  return ( dataLength );
}




unsigned int JPEGCompressor::Compress( RawTile& rawtile )
{
  // Make sure we only try to compress images with 1 or 3 channels
  if( ! ( (rawtile.channels==1) || (rawtile.channels==3) ) ){
    throw string( "JPEGCompressor: JPEG can only handle images of either 1 or 3 channels" );
  }

  // JPEG can only handle 8 bit data
  if( rawtile.bpc != 8 ) throw string( "JPEGCompressor: JPEG can only handle 8 bit images" );

  // Make sure we actually have some data to compress
  if( rawtile.data == NULL ) throw string( "JPEGCompressor: no image data to compress" );


  // Initialize our destination manager
  dest->pub.init_destination = iip_init_destination;
  dest->pub.empty_output_buffer = iip_empty_output_buffer;
  dest->pub.term_destination = iip_term_destination;
  dest->strip_height = 0;

  // We set up the normal JPEG error routines, then override error_exit. Must be done before calling jpeg_create_compress()
  cinfo.err = jpeg_std_error( &jerr );

  // Override the error_exit function with our own.
  setup_error_functions( &cinfo );

  // Create compression object
  jpeg_create_compress( &cinfo );

  // Assign our destination manager to the cinfo object
  cinfo.dest = (jpeg_destination_mgr*) dest;


  // Set up the correct width and height for this particular tile
  width = rawtile.width;
  height = rawtile.height;
  channels = rawtile.channels;


  // Calculate our metadata storage requirements
  unsigned long metadata_size = metadataSize();

  // Allocate enough memory for our compressed output data
  // - compressed images at overly high quality factors can be larger than raw data
  unsigned long output_size = (unsigned long)( (uint64_t)width * height * channels *1.5 ) + metadata_size;
  dest->source = new unsigned char[output_size]; // Add some extra buffering
  dest->source_size = output_size;
  dest->owns_source = true;

  // Set image information
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = channels;
  cinfo.in_color_space = ( channels == 3 ? JCS_RGB : JCS_GRAYSCALE );
  jpeg_set_defaults( &cinfo );

  // Add DPI
  writeResolution();
  
  // Set compression quality (fastest, but possibly slower depending
  //  on hardware) - must do this after we've set the defaults!
  cinfo.dct_method = JDCT_FASTEST;

  jpeg_set_quality( &cinfo, Q, TRUE );

  jpeg_start_compress( &cinfo, TRUE );

  // Add EXIF metadata
  writeExifMetadata();

  // Add XMP metadata
  writeXMPMetadata();

  // Embed ICC profile if one is supplied
  writeICCProfile();

  // Add an identifying comment
  const char *comment = "iipsrv/" VERSION;
  jpeg_write_marker( &cinfo, JPEG_COM, (const JOCTET*) comment, strlen(comment) );

  // Compress the image line by line
  JSAMPROW row[1];
  unsigned char* data = (unsigned char*) rawtile.data;
  int row_stride = width * channels;
  while( cinfo.next_scanline < cinfo.image_height ){
    row[0] = &data[ cinfo.next_scanline * row_stride ];
    jpeg_write_scanlines( &cinfo, row, 1 );
  }

  // Tidy up, get the compressed data size and de-allocate memory
  jpeg_finish_compress( &cinfo );

  if( dest->written > std::numeric_limits<uint32_t>::max() ){
    throw string( "JPEGCompressor :: Output too large" );
  }

  // Check that we have enough memory in our Rawtile for the JPEG data.
  // This can happen on small tiles with high quality factors. If so delete and reallocate memory.
  uint32_t dataLength = static_cast<uint32_t>( dest->written );

  if( dataLength > rawtile.capacity ){
    if( rawtile.memoryManaged ) rawtile.deallocate( rawtile.data );
    rawtile.allocate( dataLength );
  }
  
  // Copy memory back to the tile
  memcpy( rawtile.data, dest->source, dataLength );
  delete[] dest->source;
  dest->source = NULL;
  dest->owns_source = false;
  jpeg_destroy_compress( &cinfo );


  // Set the tile compression parameters
  rawtile.dataLength = dataLength;
  rawtile.compressionType = ImageEncoding::JPEG;
  rawtile.quality = Q;


  // Return the size of the data we have compressed
  return dataLength;

}



// Write DPI information
void JPEGCompressor::writeResolution()
{
  // Set our physical output resolution (JPEG only supports integers in the
  // range 0-65535). Mirror the validation used in injectMetadata() so that
  // malformed values (e.g. NaN or out-of-range) cannot be truncated.
  if( dpi_x > 0.0 || dpi_y > 0.0 ){
    const double rounded_x = round( dpi_x );
    const double rounded_y = round( dpi_y );
    const bool valid_x = dpi_x <= 0.0 || (isfinite( rounded_x ) && rounded_x <= 65535.0);
    const bool valid_y = dpi_y <= 0.0 || (isfinite( rounded_y ) && rounded_y <= 65535.0);
    cinfo.X_density = (dpi_x > 0.0 && valid_x) ? (UINT16) rounded_x : (valid_x ? 1 : 0);
    cinfo.Y_density = (dpi_y > 0.0 && valid_y) ? (UINT16) rounded_y : (valid_y ? 1 : 0);
    cinfo.density_unit = (dpi_units >= 0 && dpi_units <= 2) ? (UINT8) dpi_units : 0;
  }
}



// Calculate the number of bytes the embedded metadata segments will occupy,
// including the 2-byte marker code and 2-byte length field of each segment
unsigned long JPEGCompressor::metadataSize() const
{
  unsigned long size = 0;

  // ICC: split into APP2 markers, each with ICC_OVERHEAD_LEN bytes of overhead
  if( embedICC && !icc.empty() ){
    size_t num_markers = 1 + (icc.size() - 1) / MAX_DATA_BYTES_IN_MARKER;
    // Profiles requiring more than 255 markers cannot be represented and are skipped
    if( num_markers <= 255 ) size += icc.size() + num_markers * (4 + ICC_OVERHEAD_LEN);
  }

  // XMP: single APP1 marker
  if( embedXMP && !xmp.empty() && xmp.size() <= (MAX_BYTES_IN_MARKER - XMP_PREFIX_SIZE) )
    size += 4 + xmp.size() + XMP_PREFIX_SIZE;

  // EXIF: single APP1 marker
  if( embedEXIF && !exif.empty() && exif.size() <= (MAX_BYTES_IN_MARKER - EXIF_PREFIX_SIZE) )
    size += 4 + exif.size() + EXIF_PREFIX_SIZE;

  // Identifying comment marker
  size += 4 + strlen( "iipsrv/" VERSION );

  return size;
}



// Write ICC profile into JPEG header if profile has been set
// Function *must* be called AFTER calling jpeg_start_compress() and BEFORE
// the first call to jpeg_write_scanlines().
// (This ordering ensures that the APP2 marker(s) will appear after the
// SOI and JFIF or Adobe markers, but before all else.)
void JPEGCompressor::writeICCProfile()
{
  // Skip if profile embedding disabled
  if( !embedICC || icc.empty() ) return;

  size_t num_markers;           // total number of markers we'll write
  int cur_marker = 1;           // per spec, counting starts at 1
  size_t length;                // number of bytes to write in this marker

  size_t icc_data_len = icc.size();
  const char* icc_data_ptr = icc.c_str();
  
  // Calculate the number of markers we'll need, rounding up of course
  num_markers = icc_data_len / MAX_DATA_BYTES_IN_MARKER;
  if( num_markers * MAX_DATA_BYTES_IN_MARKER != icc_data_len ){
    num_markers++;
  }

  // The sequence and total-count fields are each a single byte, so a profile
  // requiring more than 255 APP2 markers cannot be represented. Skip it rather
  // than emit a stream with wrapped sequence/count values (matches injectMetadata()).
  if( num_markers > 255 ) return;

  // Marker payload: "ICC_PROFILE\0" id (12) + sequence (1) + total (1) + chunk
  static const char icc_id[12] = "ICC_PROFILE";
  unsigned char* payload = markerBuffer();   // max = ICC_OVERHEAD_LEN (14) + MAX_DATA_BYTES_IN_MARKER (65519)

  while( icc_data_len > 0 ){

    length = icc_data_len;
    if( length > MAX_DATA_BYTES_IN_MARKER ){
      length = MAX_DATA_BYTES_IN_MARKER;
    }
    icc_data_len -= length;

    // Assemble the full marker payload and write it in a single call,
    // rather than emitting one byte at a time via jpeg_write_m_byte.
    unsigned int payload_len = (unsigned int)( ICC_OVERHEAD_LEN + length );
    memcpy( payload, icc_id, 12 );
    payload[12] = (unsigned char) cur_marker;
    payload[13] = (unsigned char) num_markers;
    memcpy( payload + ICC_OVERHEAD_LEN, icc_data_ptr, length );

    jpeg_write_marker( &cinfo, ICC_MARKER, (const JOCTET*) payload, payload_len );

    icc_data_ptr += length;
    cur_marker++;
  }
}



void JPEGCompressor::writeXMPMetadata()
{
  // Make sure our XMP data has a valid size (namespace prefix is 29 bytes)
  if( !embedXMP || xmp.empty() || xmp.size() > (MAX_BYTES_IN_MARKER - XMP_PREFIX_SIZE) ) return;

  // The XMP data in a JPEG stream needs to be prefixed with a zero-terminated ID string
  // ref http://www.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/cs6/XMPSpecificationPart3.pdf (pp13-14)
  unsigned char* xmpstr = markerBuffer();      // per spec, XMP max size is 65502; namespace prefix/id is 29 bytes

  // Build the 29-byte prefix (28-byte namespace id + null terminator) followed by
  // the XMP packet itself. Use memcpy rather than snprintf so that any embedded
  // null bytes within the XMP data are preserved.
  memcpy( xmpstr, XMP_PREFIX, XMP_PREFIX_SIZE - 1 );
  xmpstr[XMP_PREFIX_SIZE - 1] = '\0';
  memcpy( xmpstr + XMP_PREFIX_SIZE, xmp.c_str(), xmp.size() );

  // Can't use regular addMetadata, because of the zero term after the namespace id; and the APP1 marker
  jpeg_write_marker( &cinfo, JPEG_APP0+1, (const JOCTET*) xmpstr, XMP_PREFIX_SIZE + xmp.size() );
}



void JPEGCompressor::writeExifMetadata()
{
  // Skip if EXIF embedding is disabled or no EXIF present
  if( !embedEXIF || exif.empty() ) return;

  size_t len = exif.size();

  // A single JPEG APP1 marker allows at most 65533 bytes of payload (including
  // the 2-byte length field). With the 6-byte "Exif\0\0" prefix, the EXIF data
  // itself is limited to 65527 bytes.
  if( len > (MAX_BYTES_IN_MARKER - EXIF_PREFIX_SIZE) ) return;

  // Need to add prefix (need do this with memcpy because of null bytes)
  unsigned char* exifstr = markerBuffer();
  memcpy( exifstr, EXIF_PREFIX, EXIF_PREFIX_SIZE );
  memcpy( exifstr+EXIF_PREFIX_SIZE, exif.c_str(), len );

  // Write out our marker
  jpeg_write_marker( &cinfo, JPEG_APP0+1, (const JOCTET*) exifstr, len+EXIF_PREFIX_SIZE );
}



// Inject metadata into an already-encoded JPEG bitstream. This modifies
// rawtile.data in place (for the physical resolution) and, when metadata
// segments are added, replaces the buffer with a newly allocated one. The tile
// must therefore own a writable buffer (memoryManaged == true) and must not be
// shared with the cache or another tile.
void JPEGCompressor::injectMetadata( RawTile& rawtile )
{
  unsigned char* data = (unsigned char*) rawtile.data;

  if( (data==NULL) || rawtile.dataLength < 4 || data[0] != 0xFF || data[1] != 0xD8 ) return;

  const bool update_resolution = dpi_x > 0.0 || dpi_y > 0.0;

  // Compute the number of bytes the metadata segments will add to the stream.
  // Each segment is: marker (2) + length (2) + payload. Payloads:
  //  EXIF: "Exif\0\0" prefix + EXIF data
  //  XMP:  XMP_PREFIX (incl. null terminator) + XMP data
  //  ICC:  "ICC_PROFILE\0" + sequence + total + profile data, split into chunks
  size_t added_length = 0;
  if( embedEXIF && !exif.empty() && exif.size() <= (MAX_BYTES_IN_MARKER - EXIF_PREFIX_SIZE) ){
    added_length += 2 + 2 + EXIF_PREFIX_SIZE + exif.size();
  }
  if( embedXMP && !xmp.empty() && xmp.size() <= (MAX_BYTES_IN_MARKER - XMP_PREFIX_SIZE) ){
    added_length += 2 + 2 + XMP_PREFIX_SIZE + xmp.size();
  }
  if( embedICC && !icc.empty() ){
    const size_t num_markers = 1 + (icc.size() - 1) / MAX_DATA_BYTES_IN_MARKER;
    // Maximum number of markers in JPEG is 255
    if( num_markers <= 255 ){
      added_length += icc.size() + num_markers * (4 + ICC_OVERHEAD_LEN);
    }
  }

  // Return if no valid metadata or resolution
  if( added_length == 0 && !update_resolution ) return;

  // The metadata segments are inserted directly into the JPEG stream after the
  // initial consecutive APPn/COM markers (i.e. after the SOI and any JFIF/Adobe
  // and existing metadata markers) and before the quantization/Huffman tables
  // and the SOS marker.

  // Find insertion point after consecutive APPn/COM markers. Do not modify a
  // JPEG whose leading metadata segment is truncated or has an invalid length.
  unsigned int pos = 2;
  unsigned int dlen = rawtile.dataLength;
  while( pos < dlen && data[pos] == 0xFF ){
    if( pos + 1 >= dlen ) return;
    if( !((data[pos+1] >= 0xE0 && data[pos+1] <= 0xEF) ||
          data[pos+1] == 0xFE) ) break;
    if( pos + 3 >= dlen ) return;
    unsigned int mlen = (data[pos+2] << 8) | data[pos+3];
    if( mlen < 2 || mlen > dlen - pos - 2 ) return;
    pos += 2 + mlen;
  }

  // Add the physical resolution in-place if specified, after validating the
  // leading marker sequence so malformed input remains untouched.
  if( (dpi_x > 0.0 || dpi_y > 0.0) && dlen >= 20 && data[2] == 0xFF && data[3] == 0xE0 &&
      ((data[4] << 8) | data[5]) >= 16 && ((data[4] << 8) | data[5]) <= dlen - 4 &&
      data[6] == 'J' && data[7] == 'F' && data[8] == 'I' && data[9] == 'F' && data[10] == 0x00 ){
    const double rounded_x = round( dpi_x );
    const double rounded_y = round( dpi_y );
    // Make sure the values are valid for JPEG, which can only handle 16bit values
    const bool valid_x = dpi_x <= 0.0 || (isfinite( rounded_x ) && rounded_x <= 65535.0);
    const bool valid_y = dpi_y <= 0.0 || (isfinite( rounded_y ) && rounded_y <= 65535.0);
    unsigned int xd = (dpi_x > 0.0 && valid_x) ? (unsigned int) rounded_x : (valid_x ? 1 : 0);
    unsigned int yd = (dpi_y > 0.0 && valid_y) ? (unsigned int) rounded_y : (valid_y ? 1 : 0);
    data[13] = (dpi_units >= 0 && dpi_units <= 2) ? (unsigned char) dpi_units : 0;
    data[14] = (xd >> 8) & 0xFF; data[15] = xd & 0xFF;
    data[16] = (yd >> 8) & 0xFF; data[17] = yd & 0xFF;
  }

  // Zero-copy fast path when no metadata segments need to be added.
  if( added_length == 0 ) return;

  const size_t max_length = std::numeric_limits<uint32_t>::max();
  size_t new_length = dlen;
  if( added_length > max_length - new_length ) return;
  new_length += added_length;

  // The passthrough buffer is sized exactly to the tile data, so there is no
  // spare capacity to insert metadata in place. Always allocate a new buffer
  // and copy the disjoint prefix, metadata segments and image data into it.
  unsigned char* buffer = new unsigned char[new_length];

  memcpy( buffer, data, pos );
  unsigned int offset = pos;

  // Write our EXIF APP1 segment: "Exif\0\0" + EXIF data
  if( embedEXIF && !exif.empty() && exif.size() <= (MAX_BYTES_IN_MARKER - EXIF_PREFIX_SIZE) ){
    size_t seg_len = 2 + EXIF_PREFIX_SIZE + exif.size();
    buffer[offset] = 0xFF; buffer[offset+1] = 0xE1;
    buffer[offset+2] = (seg_len >> 8) & 0xFF; buffer[offset+3] = seg_len & 0xFF;
    offset += 4;
    const unsigned char prefix[EXIF_PREFIX_SIZE] = {'E','x','i','f',0x00,0x00};
    memcpy( &buffer[offset], prefix, EXIF_PREFIX_SIZE );
    offset += EXIF_PREFIX_SIZE;
    memcpy( &buffer[offset], exif.c_str(), exif.size() );
    offset += exif.size();
  }

  // Write our XMP APP1 segment: XMP_PREFIX (incl. null terminator) + XMP data
  if( embedXMP && !xmp.empty() && xmp.size() <= (MAX_BYTES_IN_MARKER - XMP_PREFIX_SIZE) ){
    size_t seg_len = 2 + XMP_PREFIX_SIZE + xmp.size();
    buffer[offset] = 0xFF; buffer[offset+1] = 0xE1;
    buffer[offset+2] = (seg_len >> 8) & 0xFF; buffer[offset+3] = seg_len & 0xFF;
    offset += 4;
    memcpy( &buffer[offset], XMP_PREFIX, XMP_PREFIX_SIZE - 1 );
    buffer[offset + XMP_PREFIX_SIZE - 1] = 0x00;
    offset += XMP_PREFIX_SIZE;
    memcpy( &buffer[offset], xmp.c_str(), xmp.size() );
    offset += xmp.size();
  }

  // Write our ICC APP2 segment(s): "ICC_PROFILE\0" + sequence + total + profile data
  if( embedICC && !icc.empty() ){
    const size_t num_markers = 1 + (icc.size() - 1) / MAX_DATA_BYTES_IN_MARKER;
    if( num_markers <= 255 ){
      const unsigned char* icc_data = (const unsigned char*) icc.c_str();
      size_t icc_remaining = icc.size();
      for( size_t cur_marker = 1; icc_remaining > 0; cur_marker++ ){
        size_t length = icc_remaining;
        if( length > MAX_DATA_BYTES_IN_MARKER ) length = MAX_DATA_BYTES_IN_MARKER;
        icc_remaining -= length;
        size_t seg_len = 2 + ICC_OVERHEAD_LEN + length;
        buffer[offset] = 0xFF; buffer[offset+1] = 0xE2;
        buffer[offset+2] = (seg_len >> 8) & 0xFF; buffer[offset+3] = seg_len & 0xFF;
        offset += 4;
        static const char id[12] = "ICC_PROFILE";
        memcpy( &buffer[offset], id, 12 );
        offset += 12;
        buffer[offset] = (unsigned char) cur_marker;
        buffer[offset+1] = (unsigned char) num_markers;
        offset += 2;
        memcpy( &buffer[offset], icc_data, length );
        offset += length;
        icc_data += length;
      }
    }
  }

  // Copy the image data itself
  memcpy( &buffer[offset], &data[pos], dlen - pos );

  // Delete our original data buffer and re-assign our new buffer
  if( rawtile.memoryManaged ) rawtile.deallocate( rawtile.data );
  rawtile.data = buffer;
  rawtile.capacity = (uint32_t) new_length;
  rawtile.dataLength = (uint32_t) new_length;
  rawtile.memoryManaged = true;

}
