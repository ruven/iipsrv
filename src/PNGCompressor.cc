/*  PNG class wrapper to libpng library

    Copyright (C) 2012-2024 Ruven Pillay

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


#include "PNGCompressor.h"

using namespace std;


// Minimum buffer size for output data
#define MX 65536


// Handle libpng versions that don't have itxt support
#ifdef PNG_WRITE_iTXt_SUPPORTED
#define iTXt_COMPRESSION PNG_ITXT_COMPRESSION_NONE
#else
#define iTXt_COMPRESSION PNG_TEXT_COMPRESSION_NONE
#endif


// ICC profile definitions
#define ICC_PROFILE_NAME "ICC"  // PNG requires a name for the ICC profile name
#define ICC_OVERHEAD_SIZE 5     // ICC overhead for PNG = profile name + 2 bytes: 1 for null byte + 1 for compression type


// XMP definitions
#define XMP_PREFIX "XML:com.adobe.xmp"
#define XMP_OVERHEAD_SIZE 18    // XMP overhead for PNG = prefix size + null byte


/// Check for little endianness
inline bool byte_order_little_endian() {
  long one = 1;
  return (*((char*)(&one)));
}


static void png_write_data( png_structp png_ptr, png_bytep buffer, png_size_t length )
{
  png_destination_ptr dest = (png_destination_ptr)( png_get_io_ptr(png_ptr) );

  if( dest->written + length > dest->output_size ){
    unsigned int new_size = dest->output_size * 2;
    unsigned char *output = new unsigned char[new_size];
    memcpy( output, dest->output, dest->output_size );
    delete[] dest->output;
    dest->output = output;
    dest->output_size = new_size;
  }

  if( !memcpy( &(dest->output[dest->written]), buffer, length ) ){
    png_destroy_write_struct( &(dest->png_ptr), &(dest->info_ptr) );
    throw string( "PNGCompressor :: png_write_data: error writing to buffer");
  }

  dest->written += length;
}


// Need to declare function even if it does nothing to avoid libpng using it's own function
static void png_flush( png_structp png_ptr ){
  // No need to flush
}


static void png_cexcept_error( const png_structp png_ptr, const png_const_charp msg ){
  png_destination_ptr dest = (png_destination_ptr)( png_get_io_ptr(png_ptr) );
  png_destroy_write_struct( &(dest->png_ptr), &(dest->info_ptr) );

  throw string( string("PNGCompressor :: ") + msg );
}


// Catch PNG warning messages
static void png_warning_fn( png_structp png_ptr, png_const_charp msg ){
  // Do nothing for now
}


void PNGCompressor::InitCompression( const RawTile& rawtile, unsigned int strip_height )
{

  // Set up the correct width and height for this particular tile
  width = rawtile.width;
  height = rawtile.height;
  channels = rawtile.channels;

  // Calculate our metadata storage requirements
  unsigned int metadata_size =
    (icc.size()>0 ? (icc.size()+ICC_OVERHEAD_SIZE) : 0) +
    (xmp.size()>0 ? (xmp.size()+XMP_OVERHEAD_SIZE) : 0) +
    (exif.size()>0 ? exif.size() : 0);
 
  // Allocate enough memory for our header and metadata
  unsigned long output_size = metadata_size + MX;
  header = new unsigned char[output_size];
  dest.output = header;
  dest.output_size = output_size;     
  dest.written = 0;
  dest.bytes_per_pixel = (unsigned int) (rawtile.bpc/8);


  // Make sure we only try to compress images with 1 or 3 channels with ir without alpha
  if( !( (channels==1) || (channels==2) || (channels==3) || (channels==4) ) ){
    throw string( "PNGCompressor:: only 1-4 channels are supported" );
  }


  // Create PNG write structure
  dest.png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL,
                                          (png_error_ptr) png_cexcept_error, (png_error_ptr) png_warning_fn );
  if( !dest.png_ptr ) throw string( "PNGCompressor :: Error allocating png_struct" );


  // Create info structure
  dest.info_ptr = png_create_info_struct( dest.png_ptr );
  if( !dest.info_ptr ){
    png_destroy_write_struct( &dest.png_ptr, (png_infopp) NULL );
    throw string( "PNGCompressor :: Error creating png_info_struct" );
  }


  // Set up our own write function
  png_set_write_fn( dest.png_ptr, (png_voidp) &dest, png_write_data, png_flush );


  // Set basic metadata
  png_set_IHDR(
	       dest.png_ptr, dest.info_ptr,
	       width, height,
	       rawtile.bpc,
	       ( (channels<3) ? ( (channels==2) ? PNG_COLOR_TYPE_GRAY_ALPHA : PNG_COLOR_TYPE_GRAY): ( (channels==4) ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB)),
	       PNG_INTERLACE_NONE,
	       PNG_COMPRESSION_TYPE_BASE, 
	       PNG_FILTER_TYPE_BASE
		);


  // Set compression parameters - Deflate compression level (0-9) and a pre-processing filter
  png_set_compression_level( dest.png_ptr, Q );
  png_set_filter( dest.png_ptr, 0, filterType );


  // Add metadata
  writeMetadata();


  // Write out the encoded header
  png_write_info( dest.png_ptr, dest.info_ptr );


  // Handle endian issues for 16bit PNG: Must be called *after* png_write_info
  if( rawtile.bpc == 16 && byte_order_little_endian() ){
    png_set_swap( dest.png_ptr );
  }


  // Deterine header size
  header_size = dest.written;

}



unsigned int PNGCompressor::CompressStrip( unsigned char* input, unsigned char* output, unsigned int strip_height )
{
  // First delete our header if we have one
  if( header_size > 0 && header ){
    delete[] header;
    header = NULL;
    header_size = 0;
  }


  // Take into account extra bytes needed for 16bit images
  png_uint_32 ulRowBytes = width * channels * dest.bytes_per_pixel;

  // Set up our buffer management
  dest.output = output;
  dest.output_size = strip_height*width*channels*dest.bytes_per_pixel + MX;
  dest.written = 0;

  // Compress row by row
  for( unsigned int i = 0; i < strip_height; i++ ) {
    png_write_row( dest.png_ptr, (png_byte*) &input[i*ulRowBytes] ); 
  }

  return dest.written;
}



unsigned int PNGCompressor::Finish( unsigned char *output )
{  
  dest.output = output;
  dest.written = 0;

  // Write any additional chunks to the PNG file 
  png_write_end( dest.png_ptr, dest.info_ptr );

  // Clean up after the write, and free any memory allocated
  png_destroy_write_struct( &(dest.png_ptr), &(dest.info_ptr) );

  dest.png_ptr = NULL;
  dest.info_ptr = NULL;

  return dest.written;
}



unsigned int PNGCompressor::Compress( RawTile& rawtile )
{

  // Make sure we only try to compress images with 1 or 3 channels plus alpha channels
  if( !( (rawtile.channels==1) || (rawtile.channels==2) || (rawtile.channels==3) || (rawtile.channels==4) ) ){
    throw string( "PNGCompressor:: only 1-4 channels are supported" );
  }


  // Set up the correct width and height for this particular tile
  width = rawtile.width;
  height = rawtile.height;
  channels = rawtile.channels;


  // Calculate our metadata storage requirements
  unsigned int metadata_size =
    (icc.size()>0 ? (icc.size()+ICC_OVERHEAD_SIZE) : 0) +
    (xmp.size()>0 ? (xmp.size()+XMP_OVERHEAD_SIZE) : 0) +
    (exif.size()>0) ? exif.size() : 0;

  // Allocate enough memory for our compressed output data - make sure there is extra buffering
  // Note that compressed images at overly high quality factors can be larger than raw data
  dest.bytes_per_pixel = (unsigned int) (rawtile.bpc/8);
  unsigned long output_size = (unsigned long)( (unsigned long)(width*height*channels*1.5*dest.bytes_per_pixel) + metadata_size + MX);
  dest.output = new unsigned char[output_size];
  dest.output_size = output_size;
  dest.written = 0;


  // Create PNG write structure
  dest.png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL,
                                          (png_error_ptr) png_cexcept_error, (png_error_ptr) png_warning_fn );
  if( !dest.png_ptr ) throw string( "PNGCompressor :: Error allocating png_struct" );


  // Create info structure
  dest.info_ptr = png_create_info_struct( dest.png_ptr );
  if( !dest.info_ptr ){
    png_destroy_write_struct( &dest.png_ptr, (png_infopp) NULL );
    throw string( "PNGCompressor :: Error creating png_info_ptr" );
  }


  // Set up our own write function
  png_set_write_fn( dest.png_ptr, (png_voidp) &dest, png_write_data, png_flush );


  // Set basic metadata
  png_set_IHDR( 
	       dest.png_ptr, dest.info_ptr,
	       width, height,
	       rawtile.bpc,
	       ( (channels<3) ? ( (channels==2) ? PNG_COLOR_TYPE_GRAY_ALPHA : PNG_COLOR_TYPE_GRAY): ( (channels==4) ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB)), 
	       PNG_INTERLACE_NONE, 
	       PNG_COMPRESSION_TYPE_BASE, 
	       PNG_FILTER_TYPE_BASE 
		);


  // Set compression parameters - Deflate compression level (0-9) and a pre-processing filter
  png_set_compression_level( dest.png_ptr, Q );
  png_set_filter( dest.png_ptr, 0, filterType );


  // Write metadata
  writeMetadata();


  // Write out the encoded header
  png_write_info( dest.png_ptr, dest.info_ptr );


  // Handle endian issues for 16bit PNG: MUST BE below png_write_info
  if( rawtile.bpc == 16 && byte_order_little_endian() ){
    png_set_swap( dest.png_ptr );
  }
    

  png_uint_32 ulRowBytes;
  
  // row_bytes is the width x number of channels
  ulRowBytes = width * channels * dest.bytes_per_pixel;


  // Compress row by row
  unsigned char* data = (unsigned char*) rawtile.data;
  for( unsigned int i = 0; i < height; i++ ) {
    png_write_row( dest.png_ptr, (png_byte*) &data[i*ulRowBytes] );
  }  
  
  // Write the additional chunks to the PNG file
  png_write_end( dest.png_ptr, dest.info_ptr );

  // Clean up after the write, and free any memory allocated
  png_destroy_write_struct( &(dest.png_ptr), &(dest.info_ptr) );

 
  // Allocate the appropriate amount of memory if the encoded PNG is larger than the raw image buffer
  if( dest.written > rawtile.capacity ){
    if( rawtile.memoryManaged ) delete[] (unsigned char*) rawtile.data;
    rawtile.data = new unsigned char[dest.written];
    rawtile.capacity = dest.written;
  }

  rawtile.dataLength = dest.written;
  memcpy( rawtile.data, dest.output, rawtile.dataLength );
  delete[] dest.output;

  // Set the tile compression type
  rawtile.compressionType = ImageEncoding::PNG;
  rawtile.quality = Q;

  // Return the size of the data we have compressed
  return rawtile.dataLength;

}



void PNGCompressor::writeMetadata()
{
  // Set some basic text metadata
  std::map<const std::string, const std::string> :: const_iterator it;
  png_text text[6];
  int n = 0;

  text[n].compression = PNG_TEXT_COMPRESSION_NONE;
  text[n].key = (png_charp) "Software";
  text[n].text = (png_charp) "iipsrv/" VERSION;
  text[n].text_length = 7 + strlen(VERSION);
  n++;

  it = metadata.find("title");
  if( it != metadata.end() ){
    text[n].compression = iTXt_COMPRESSION;
    text[n].key = (png_charp) "Title";
    text[n].text = (png_charp) it->second.c_str();
    text[n].text_length = it->second.size();
    n++;
  }

  it = metadata.find("creator");
  if( it != metadata.end() ){
    text[n].compression = iTXt_COMPRESSION;
    text[n].key = (png_charp) "Author";
    text[n].text = (png_charp) it->second.c_str();
    text[n].text_length = it->second.size();
    n++;
  }

  it = metadata.find("description");
  if( it != metadata.end() ){
    text[n].compression = iTXt_COMPRESSION;
    text[n].key = (png_charp) "Description";
    text[n].text = (png_charp) it->second.c_str();
    text[n].text_length = it->second.size();
    n++;
  }

  it = metadata.find("rights");
  if( it != metadata.end() ){
    text[n].compression = iTXt_COMPRESSION;
    text[n].key = (png_charp) "Copyright";
    text[n].text = (png_charp) it->second.c_str();
    text[n].text_length = it->second.size();
    n++;
  }

  it = metadata.find("date");
  if( it != metadata.end() ){
    text[n].compression = PNG_TEXT_COMPRESSION_NONE;
    text[n].key = (png_charp) "Creation Time";
    text[n].text = (png_charp) it->second.c_str();
    text[n].text_length = it->second.size();
    n++;
  }

  png_set_text( dest.png_ptr, dest.info_ptr, text, n );

  // Write DPI
  writeResolution();

  // Write our ICC profile
  writeICCProfile();

  // Write XMP chunk
  writeXMPMetadata();

  // Write EXIF chunk
  writeExifMetadata();
}



void PNGCompressor::writeResolution()
{
  // Set physical resolution - convert from inches or cm to meters
  if( dpi_x || dpi_y ){
    png_uint_32 res_x = (dpi_units==2) ? dpi_x*10 : ( (dpi_units==1) ? dpi_x*25.4 : dpi_x );
    png_uint_32 res_y = (dpi_units==2) ? dpi_y*10 : ( (dpi_units==1) ? dpi_y*25.4 : dpi_y );
    png_set_pHYs( dest.png_ptr, dest.info_ptr, res_x, res_y, ((dpi_units==0) ? PNG_RESOLUTION_UNKNOWN : PNG_RESOLUTION_METER) );
  }
}



void PNGCompressor::writeICCProfile()
{
  // Skip if profile embedding disabled or no profile exists
  if( !embedICC || icc.empty() ) return;

  const char* icc_data_ptr = icc.c_str();

  // Account for changes in PNG library from 1.2 to 1.5
#if PNG_LIBPNG_VER < 10500
  png_charp icc_profile_buf_png = (png_charp) icc_data_ptr;
#else
  png_const_bytep icc_profile_buf_png = (png_const_bytep) icc_data_ptr;
#endif

  // Avoid "iCCP: known incorrect sRGB profile" errors
#if defined(PNG_SKIP_sRGB_CHECK_PROFILE) && defined(PNG_SET_OPTION_SUPPORTED)
  png_set_option( dest.png_ptr, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON );
#endif

  // Disable profile illuminant errors such as "'ICC': 0h: PCS illuminant is not D50 errors"
#if PNG_LIBPNG_VER >= 10420
  png_set_benign_errors( dest.png_ptr, 1 );
#endif

  // Write out ICC profile
  png_set_iCCP( dest.png_ptr, dest.info_ptr, ICC_PROFILE_NAME, PNG_COMPRESSION_TYPE_BASE, icc_profile_buf_png, icc.size() );

}



void PNGCompressor::writeXMPMetadata()
{
  // Skip if XMP embedding disabled or no XMP chunk exists
  if( !embedXMP || xmp.empty() ) return;

  png_text text;
  text.key = (png_charp) XMP_PREFIX;
  text.text = (png_charp) xmp.c_str();
  text.compression = PNG_TEXT_COMPRESSION_NONE;
  text.text_length = xmp.size();

  // Write out our XMP chunk
  png_set_text( dest.png_ptr, dest.info_ptr, &text, 1 );

}



void PNGCompressor::writeExifMetadata()
{
  // Skip if EXIF embedding is disabled or no EXIF present
  if( !embedEXIF || exif.empty() ) return;

#ifdef PNG_eXIf_SUPPORTED
  // Write out EXIF chunk
  png_set_eXIf_1( dest.png_ptr, dest.info_ptr, exif.size(), (png_bytep) exif.c_str() );
#endif

}
