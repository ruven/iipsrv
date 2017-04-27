/*  PNG class wrapper to libpng library

    Copyright (C) 2012-2013 Ruven Pillay with extensions by Dave Beaudet

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
#include "Compressor.h"
#include "Timer.h"
#include "zlib.h"

static void png_write_data(png_structp png_ptr, png_bytep payload, png_size_t length)
{

  /* some debugging code that spits the png out to a file
  //ofstream ofs;
  //ofs.open ("/tmp/test.png", ofstream::out | ofstream::binary | ofstream::app);
  //ofs.write((const char*) payload, length);
  //ofs.close();
  // logfile << "png_write_data called with " << length << " bytes to write to mem location" << &payload << endl;
  */

  png_destination_ptr dest = (png_destination_ptr) (png_get_io_ptr(png_ptr));

  if ( dest->size + length > dest->mx ) {
    dest->mx+= (10240>length ? 10240 : length);
    dest->data=(unsigned char*)realloc(dest->data, dest->mx); 
  }
  if (!memcpy(&(dest->data[dest->size]), payload, length))
    throw string( "png_write_data: error writing to buffer.");
  dest->size += length;

}

static void png_flush(png_structp png_ptr) { 
    // logfile << "png flush has been called" << endl;
}

static void
png_cexcept_error(png_structp png_ptr, png_const_charp msg)
{
#ifndef PNG_NO_CONSOLE_IO
#endif
   {
     throw string(msg);
   }
}

void PNGCompressor::InitCompression( const RawTile& rawtile, unsigned int strip_height ) throw (string)
{

  ofstream ofs;
  // create an empty test file first
  ofs.open ("/tmp/test.png", ofstream::out | ofstream::binary );
  ofs.close();

  // Set up the correct width and height for this particular tile
  width = rawtile.width;
  height = rawtile.height;
  channels = rawtile.channels;

  // see png_write_data where data structure, size, and mx are dealt with
  dest.mx = 1024;                           // allocated a reasonably sized header
  dest.data = new unsigned char[dest.mx];   // data will be resized during writing if necessary
  dest.size = 0;                            // starts with no actual data written

  // Make sure we only try to compress images with 1 or 3 channels with ir without alpha
  if( ! ( (channels==1) || (channels==2) || (channels==3) || (channels==4))  ){
    throw string( "PNGCompressor:: currently only either 1 or 3 channels are supported with or without alpha values." );
  }

  const int ciBitDepth = 8;

  dest.png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL,
                                          (png_error_ptr)png_cexcept_error, (png_error_ptr)NULL );


  if (!dest.png_ptr) throw string( "PNGCompressor:: Error allocacating png_structp." );

  dest.info_ptr = png_create_info_struct(dest.png_ptr);
  if (! dest.info_ptr) {
    png_destroy_write_struct(&dest.png_ptr, (png_infopp) NULL);
    throw string( "PNGCompressor:: Error creating png_infop." );
  }

  /*********************************************************
   NOTES: enabling compression to Z_BEST_SPEED results in a 3x slowdown
   e.g. png_set_compression_level( dest.png_ptr, Z_BEST_SPEED );
   I have chosen speed vs size intially, but it probably makes sense to provide
   a tunable configuration for this - @beaudet
  **********************************************************/
  png_set_compression_level( dest.png_ptr, compressionLevel );
 
  /******************************************************************
   NOTES: filters are used to prepare the PNG for optimal compression
   I choose no filter here because I'm defaulting to no compression
   but it probably makes sense to provide a tunable configuration for this
  *******************************************************************/
  png_set_filter(dest.png_ptr, 0, filterType );

  png_set_write_fn( dest.png_ptr, (png_voidp) &dest, png_write_data, png_flush );

  // We're going to write a very simple 3x8 bit RGB image
  png_set_IHDR( 
    dest.png_ptr,  
    dest.info_ptr, 
    width, 
    height, 
    ciBitDepth,
    ( (channels<3) ? ( (channels==2) ? PNG_COLOR_TYPE_GRAY_ALPHA : PNG_COLOR_TYPE_GRAY): ( (channels==4) ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB)), 
    PNG_INTERLACE_NONE, 
    PNG_COMPRESSION_TYPE_BASE, 
    PNG_FILTER_TYPE_BASE 
  );

  // Write the header information at this point
  png_write_info(dest.png_ptr, dest.info_ptr);

}


/* We use a separate tile_height from the predefined strip_height because
   the tile height for the final row can be different
   o = output buffer - not used - just here for compliance with Compressor class which isn't 
       the best implementation so we should probably change that
 */
unsigned int PNGCompressor::CompressStrip( unsigned char* inputbuff, unsigned char* outputbuff, unsigned long outputbufflen, unsigned int tile_height ) throw (string)
{

  Timer partoffunction;
  Timer entirefunction;
  entirefunction.start();

  png_uint_32     ulRowBytes = width * channels;
  png_byte        **ppbRowPointers = NULL;

  if( (ppbRowPointers = (png_bytepp) malloc(tile_height * sizeof(png_bytep))) == NULL ){
    throw "PNGCompressor:: Out of memory";
  }

  // ensure that the output buffer is set properly
  setOutputBuffer(outputbuff, outputbufflen);

  for( unsigned int i = 0; i < tile_height; i++ ) {
    png_write_row( dest.png_ptr, (png_byte*) &inputbuff[i*ulRowBytes] ); 
  }

  return dest.size;
}

unsigned int PNGCompressor::Finish(unsigned char *o, unsigned long olen) throw (string) {

  setOutputBuffer(o, olen);
  
  // Write any additional chunks to the PNG file 
  png_write_end(dest.png_ptr,  dest.info_ptr);

  // Clean up after the write, and free any memory allocated
  png_destroy_write_struct(&(dest.png_ptr), (png_infopp) NULL);

  return dest.size ;
}


int PNGCompressor::Compress( RawTile& rawtile ) throw (string) {

  logfile << "PNGCompressor::Compress - !!!!!!!! NOT IMPLEMENTED - IIP DOESN'T !!!!!!" << endl;

/*************************************************************************************************************
 If the IIP Protocol is going to be extended with a PTL command for producing PNG tiles, this would have to be
 implemented.  For now, I only need support via the CVT operation so I'm leaving this out - but CompressStrip
 and the CVT command already provide pretty much everything that would have to happen and Ruven has already
 implemented most of it below
*************************************************************************************************************/
 
/*
 PNG OUTPUT SUPPORT
  Timer partoffunction;
  Timer entirefunction;
  entirefunction.start();

  png_byte    **ppbRowPointers = NULL;
  const int   ciBitDepth = 8;
  png_uint_32 ulRowBytes;

  // Set up the correct width and height for this particular tile
  width = rawtile.width;
  height = rawtile.height;
  channels = rawtile.channels;

  // Make sure we only try to compress images with 1 or 3 channels
  if( ! ( (channels==1) || (channels==2) || (channels==3) || (channels==4))  ){
    throw string( "PNGCompressor:: currently only either 1 or 3 channels are supported with or without alpha values." );
  }

  dest.png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
                                         (png_error_ptr)png_cexcept_error, (png_error_ptr)NULL);

  if( !dest.png_ptr ) throw string( "PNGCompressor:: Error allocacating png_structp" );

  dest.info_ptr = png_create_info_struct(dest.png_ptr);
  if (! dest.info_ptr) {
    png_destroy_write_struct(&(dest.png_ptr), (png_infopp) NULL);
    throw string( "PNGCompressor:: Error creating png_infop" );
  }

  dest.size = 0;
  dest.mx =  rawtile.dataLength;  //initial PNG size
  dest.data = new unsigned char[width*height*channels];


  png_set_write_fn( dest.png_ptr, (png_voidp) &dest, png_write_data, png_flush );

  // We're going to write a very simple 3x8 bit RGB image
  png_set_IHDR( dest.png_ptr, dest.info_ptr, width, height, ciBitDepth,
                (channels<3) ? ((channels==2) ? PNG_COLOR_TYPE_GRAY_ALPHA : PNG_COLOR_TYPE_GRAY): ((channels==4) ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB),
                PNG_INTERLACE_NONE, 
                PNG_COMPRESSION_TYPE_BASE,
                PNG_FILTER_TYPE_BASE );

  // Set the zlib compression level
  //png_set_compression_level( dest.png_ptr, Z_BEST_SPEED );
  png_set_compression_level( dest.png_ptr, Z_NO_COMPRESSION );
  png_set_filter(dest.png_ptr, NULL, PNG_NO_FILTERS);

  // Write the file header information
  png_write_info( dest.png_ptr, dest.info_ptr );

  // row_bytes is the width x number of channels
  ulRowBytes = width * channels;

  try {

    // Allocate memory for an array of row-pointers
    if ((ppbRowPointers = (png_bytepp) malloc(height * sizeof(png_bytep))) == NULL)
      throw "PNGCompressor:: Out of memory";

    // Set the individual row-pointers to point at the correct offsets
    for (int i = 0; i < height; i++){
      ppbRowPointers[i] = (png_byte*)((unsigned char*)rawtile.data + i *ulRowBytes);
    }

    // Write out the entire image data in one call
    png_write_rows(dest.png_ptr, ppbRowPointers,height);

    // Write the additional chunks to the PNG file (not really needed)
    png_write_end(dest.png_ptr,  dest.info_ptr);

    // We're done
    free (ppbRowPointers);
    ppbRowPointers = NULL;

    // Clean up after the write, and free any memory allocated
    png_destroy_write_struct(&(dest.png_ptr), (png_infopp) NULL);

  }
  catch (char * msg){
    png_destroy_write_struct(&(dest.png_ptr), (png_infopp) NULL);
    if(ppbRowPointers) free (ppbRowPointers);
    return 0;
  }

  // If dest is bigger, then realloate
  if( dest.size > rawtile.dataLength ){
    delete[] (unsigned char*) rawtile.data;
    rawtile.data = new unsigned char[dest.size];
  }

  rawtile.dataLength = dest.size;
  memcpy( rawtile.data, dest.data, rawtile.dataLength );
  delete[] dest.data;
  dest.data = NULL;
  dest.size = 0;
  dest.mx   = 0;

  // Set the tile compression type
  rawtile.compressionType = PNG;
  rawtile.quality = 100;  // PNG is not compressed

  //logfile << "PNGCOMPRESSOR::Compress Total compression time " << entirefunction.getTime() << " microseconds" << endl;

  // Return the size of the data we have compressed
  return rawtile.dataLength;
*/
  return 0;
}

void PNGCompressor::addXMPMetadata( const string& xmp_metadata ){
  /*************************************************************************************************
     according to http://dev.exiv2.org/projects/exiv2/wiki/The_Metadata_in_PNG_files
     embedding of XMP metadata can be done by writing an iTXt text chunk with the keyword XML:com.adobe.xmp
     and a value containing the XML.  iTXt chunk == PNG_iTXt in png.h

     png_write_chunk(png_structp png_ptr, png_bytep chunk_name, png_bytep data, png_size_t length);

     e.g. png_write_chunk(png_ptr, PNG_iTXt, metadata_ptr, xmp_metadata_length_bytes);
  ***************************************************************************************************/

  /************************************************************
  PNG SPECS for iTXt chunk type
     Keyword:             1-79 bytes (character string)
     Null separator:      1 byte
     Compression flag:    1 byte
     Compression method:  1 byte
     Language tag:        0 or more bytes (character string)
     Null separator:      1 byte
     Translated keyword:  0 or more bytes
     Null separator:      1 byte
     Text:                0 or more bytes
  ************************************************************/

  string xmp_keyword = "XML:com.adobe.xmp";

  int chunksize = xmp_keyword.size() + 5 + xmp_metadata.size() + 1;
  png_byte chunk[chunksize];
  png_byte chunktype[4] = { 'i', 'T', 'X', 't' };

  int i = 0;
  memcpy(&chunk[i], xmp_keyword.c_str(), xmp_keyword.size());       i += xmp_keyword.size();        // keyword
  chunk[i] = '\0';                                                  i += 1;                         // null string terminated
  chunk[i] = 0;                                                     i += 1;                         // compression flag set to 0
  chunk[i] = PNG_ITXT_COMPRESSION_NONE;                             i += 1;                         // compression method
  chunk[i] = '\0';                                                  i += 1;                         // language tag is empty string with null termination
  chunk[i] = '\0';                                                  i += 1;                         // translated keyword is empty string with null termination
  memcpy(&chunk[i], xmp_metadata.c_str(), xmp_metadata.size() );    i += xmp_metadata.size();

  png_write_chunk( dest.png_ptr, chunktype, chunk, chunksize );

}

/********************************************************************************************************************
void PNGCompressor.writeICCProfile(...) {

From: http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html#C.iCCP
4.2.2.4. iCCP Embedded ICC profile

If the iCCP chunk is present, the image samples conform to the color space represented by the embedded ICC profile as defined by the International Color Consortium [ICC]. The color space of the ICC profile must be an RGB color space for color images (PNG color types 2, 3, and 6), or a monochrome grayscale color space for grayscale images (PNG color types 0 and 4).

The iCCP chunk contains:

   Profile name:       1-79 bytes (character string)
   Null separator:     1 byte
   Compression method: 1 byte
   Compressed profile: n bytes
The format is like the zTXt chunk. (see the zTXt chunk specification). The profile name can be any convenient name for referring to the profile. It is case-sensitive and subject to the same restrictions as the keyword in a text chunk: it must contain only printable Latin-1 [ISO/IEC-8859-1] characters (33-126 and 161-255) and spaces (32), but no leading, trailing, or consecutive spaces. The only value presently defined for the compression method byte is 0, meaning zlib datastream with deflate compression (see Deflate/Inflate Compression). Decompression of the remainder of the chunk yields the ICC profile.

An application that writes the iCCP chunk should also write gAMA and cHRM chunks that approximate the ICC profile's transfer function, for compatibility with applications that do not use the iCCP chunk.

When the iCCP chunk is present, applications that recognize it and are capable of color management [ICC] should ignore the gAMA and cHRM chunks and use the iCCP chunk instead, but applications incapable of full-fledged color management should use the gAMA and cHRM chunks if present.

A file should contain at most one embedded profile, whether explicit like iCCP or implicit like sRGB.

If the iCCP chunk appears, it must precede the first IDAT chunk, and it must also precede the PLTE chunk if present.
}
********************************************************************************************************************/

