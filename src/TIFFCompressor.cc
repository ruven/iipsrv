/*  IIP TIFF Compressor Class:
    Handles 8, 16 or 32 bit data, alpha channels, ICC profiles and XMP metadata

    Copyright (C) 2024 Ruven Pillay

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

#include "TIFFCompressor.h"


using namespace std;

extern "C" {

  static int _close( thandle_t handle ){
    return 0;
  }

  // Read function - unimplemented
  static tsize_t _read( thandle_t handle, tdata_t buf, tsize_t size ){
    return size;
  }

  // Write function
  static tsize_t _write( thandle_t handle, tdata_t buf, tsize_t length ){

    tiff_mem *memtif = (tiff_mem *) handle;
    
    // Re-allocate extra memory if we don't have enough space
    if( (memtif->current + length) > memtif->capacity ){
      memtif->capacity = memtif->capacity + length;
      memtif->buffer = (unsigned char*) realloc( memtif->buffer, memtif->capacity );
    }

    // Copy across and update our position
    memcpy( &memtif->buffer[memtif->current], buf, length );
    memtif->current += length;

    // Make sure our end marker is up to date
    if( memtif->end < memtif->current ) memtif->end = memtif->current;

    return length;
  }

  // Seek function
  static toff_t _seek( thandle_t handle, toff_t offset, int whence ){

    tiff_mem *memtif = (tiff_mem *) handle;

    switch( whence ){
      // From beginning of stream
      case SEEK_SET:
	memtif->current = offset;
	break;
      // Relative to current position
      case SEEK_CUR:
	memtif->current = memtif->current + offset;
        break;
      // Relative to end of stream
      case SEEK_END:
	memtif->current = memtif->end + offset;
        break;
    }

    if( memtif->current > 0 && memtif->current < memtif->capacity ) return memtif->current;
    else return -1;
  }

  static toff_t _size( thandle_t handle ){
    tiff_mem *memtif = (tiff_mem *) handle;
    return memtif->capacity;
  }

  // Map function - unimplemented
  static int _map( thandle_t handle, tdata_t* base, toff_t* psiz ){
    return 0;
  }

  // Map function - unimplemented
  static void _unmap( thandle_t handle, tdata_t base, toff_t psize ){
  }

}



void TIFFCompressor::configure( const RawTile &rawtile )
{
  // Set basic TIFF metadata tags
  TIFFSetField( tiff, TIFFTAG_IMAGEWIDTH, rawtile.width );              // width of the image
  TIFFSetField( tiff, TIFFTAG_IMAGELENGTH, rawtile.height );            // height of the image
  TIFFSetField( tiff, TIFFTAG_SAMPLESPERPIXEL, rawtile.channels );      // number of channels per pixel
  TIFFSetField( tiff, TIFFTAG_BITSPERSAMPLE, rawtile.bpc );             // bits per channel
  TIFFSetField( tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT );       // origin of the image.
  TIFFSetField( tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );           // output in RGB

  // Assume 2 or 4 band images contain alpha channels
  if( rawtile.channels == 2 || rawtile.channels == 4 ){
    static const uint16_t alpha[1] = {EXTRASAMPLE_UNASSALPHA};
    TIFFSetField( tiff, TIFFTAG_EXTRASAMPLES, 1, alpha );
  }

  // Fixed or floating type
  uint16_t sampleFormat = (rawtile.sampleType == SampleType::FLOATINGPOINT) ? SAMPLEFORMAT_IEEEFP : SAMPLEFORMAT_UINT;
  TIFFSetField( tiff, TIFFTAG_SAMPLEFORMAT, sampleFormat );

  // Set compression type
  TIFFSetField( tiff, TIFFTAG_COMPRESSION, compression );

  // Set compression level for supported schemes
  if( compression == COMPRESSION_ADOBE_DEFLATE ) TIFFSetField( tiff, TIFFTAG_ZIPQUALITY, Q );
  else if( compression == COMPRESSION_ZSTD ) TIFFSetField( tiff, TIFFTAG_ZSTD_LEVEL, Q );
  else if( compression == COMPRESSION_WEBP ) TIFFSetField( tiff, TIFFTAG_WEBP_LEVEL, Q );
  else if( compression == COMPRESSION_JPEG ){
    TIFFSetField( tiff, TIFFTAG_JPEGQUALITY, Q );
    TIFFSetField( tiff, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB ); // Set YCbCr encoding
  }

  // Add contextual metadata
  writeMetadata();

  // Write DPI
  writeResolution();

  // Add any ICC profile
  writeICCProfile();

  // Add any XMP chunk
  writeXMPMetadata();
}



void TIFFCompressor::InitCompression( const RawTile &rawtile, unsigned int strip_height )
{
  // Initialize dest counters to zero
  dest.current = 0;
  dest.end = 0;

  // Calculate buffer size necessary - add 4K for TIFF and metadata overhead
  dest.capacity = (rawtile.width * strip_height * rawtile.channels * (rawtile.bpc/8)) + icc.size() + xmp.size() + 4096;

  // Initialize our buffer
  dest.buffer = (unsigned char*) malloc( dest.capacity );
  if( !dest.buffer ) throw string( "TIFFCompressor :: Unable to allocate memory for buffer" );

  // Set mode to write and disable memory mapping
  tiff = TIFFClientOpen( "_", "wm", (thandle_t) &dest,
			 _read,
			 _write,
			 _seek,
			 _close,
			 _size,
			 _map,
			 _unmap );

  // Setup encoding configuration
  configure( rawtile );

  // Encode as strips - multiply default (which aims for 8kB strips) by 64 for 512kB strips
  uint32_t strip_size = TIFFDefaultStripSize( tiff, 0 ) * 64;

  // We encode strip-wise (a strip can consist of multiple lines)
  TIFFSetField( tiff, TIFFTAG_ROWSPERSTRIP, strip_size );

  // Calculate number strips and number of bytes per strip
  tstrip_t nstrips = (tstrip_t)( (rawtile.height / strip_size) + ( (rawtile.height%strip_size==0) ? 0 : 1 ) );
  uint32_t nbytes = rawtile.width * rawtile.channels * (rawtile.bpc/8) * strip_size;

  int index = 0;
  for( tstrip_t n=0; n<nstrips; n++ ){
    // Write data as TIFF "strips"
    if( n == nstrips-1 ) nbytes = rawtile.dataLength - index;
    if( TIFFWriteEncodedStrip( tiff, n, (tdata_t) &((unsigned char*)rawtile.data)[index], nbytes ) != nbytes ){
      TIFFClose( tiff );
      throw string( "TIFFCompressor :: TIFFWriteEncodedStrip() error" );
    }
    index += nbytes;
  }

  // Finish encoding
  TIFFClose( tiff );

  height = rawtile.height;
  current_chunk = 0;
}



unsigned int TIFFCompressor::CompressStrip( unsigned char *source, unsigned char *output,
                                            unsigned int tile_height )
{
  // Initialize our chunk size only once at the start of the sequence
  if( current_chunk == 0 ) chunk_size = (unsigned int)( dest.end*tile_height/height );

  // Make sure we don't over-run our allocated memory
  if( (current_chunk + chunk_size) > (dest.end - 1) ) chunk_size = dest.end - current_chunk;

  // Copy our chunk of data to the given output buffer
  if( chunk_size > 0 ){
    unsigned char* data = (unsigned char*) dest.buffer;
    memcpy( output, &data[current_chunk], chunk_size );
    current_chunk += chunk_size;
  }

  return chunk_size;
}



unsigned int TIFFCompressor::Finish( unsigned char *output )
{
  // Output any remaining bytes
  if( current_chunk < dest.end - 1 ){
    unsigned char* data = (unsigned char*) dest.buffer;
    chunk_size = dest.end - current_chunk - 1;
    memcpy( output, &data[current_chunk], chunk_size );
  }
  else chunk_size = 0;

  // Free our memory buffer
  free( dest.buffer );

  return chunk_size;
}



unsigned int TIFFCompressor::Compress( RawTile& rawtile )
{
  // Estimate our size
  dest.current = 0;
  dest.end = 0;

  // Calculate buffer size necessary - add 1K for TIFF and metadata overhead
  dest.capacity = (rawtile.width * rawtile.height * rawtile.channels * (rawtile.bpc/8)) + icc.size() + xmp.size() + 1024;

  // Initialize our tile buffer
  dest.buffer = (unsigned char*) malloc( dest.capacity );
  if( !dest.buffer ) throw string( "TIFFCompressor :: Unable to allocate memory for buffer" );

  // Set mode to write and disable memory mapping
  tiff = TIFFClientOpen( "_", "wm", (thandle_t) &dest,
			 _read,
			 _write,
			 _seek,
			 _close,
			 _size,
			 _map,
			 _unmap );

  // Setup encoding configuration
  configure( rawtile );

  // In compress() function we encode in a single pass (strip)
  TIFFSetField( tiff, TIFFTAG_ROWSPERSTRIP, rawtile.height );

  // Number of pixels to write
  uint32_t len = rawtile.width * rawtile.height * rawtile.channels * (rawtile.bpc/8);

  // Write entire tile as a single TIFF "strip"
  if( TIFFWriteEncodedStrip( tiff, 0, (tdata_t) rawtile.data, len ) != len ){
    TIFFClose( tiff );
    throw string( "TIFFCompressor :: TIFFWriteEncodedStrip() error" );
  }

  // Finish up and close image
  TIFFClose( tiff );

  // Allocate the appropriate amount of memory if the encoded TIFF is larger than the raw image buffer
  len = dest.end;
  if( len > rawtile.capacity ){
    if( rawtile.memoryManaged ) rawtile.deallocate( rawtile.data );
    rawtile.allocate( len );
  }

  // Copy data back into tile
  memcpy( rawtile.data, dest.buffer, len );

  // Free our memory buffer
  free( dest.buffer );

  // Update data size and type
  rawtile.dataLength = len;
  rawtile.compressionType = ImageEncoding::TIFF;

  return rawtile.dataLength;

}



/// Write general metadata
void TIFFCompressor::writeMetadata()
{
  // Add optional metadata - create map of iipsrv index keys and their corresponding TIFF tags
  static const char *keys[8] = { "creator", "rights", "date", "description", "title", "pagename", "make", "model" };
  static const ttag_t tags[8] = { TIFFTAG_ARTIST, TIFFTAG_COPYRIGHT, TIFFTAG_DATETIME, TIFFTAG_IMAGEDESCRIPTION,
				  TIFFTAG_DOCUMENTNAME, TIFFTAG_PAGENAME, TIFFTAG_MAKE, TIFFTAG_MODEL };

  for( int n=0; n<8; n++ ){
    std::map<const std::string, const std::string> :: const_iterator it = metadata.find( keys[n] );
    if( it != metadata.end() ) TIFFSetField( tiff, tags[n], it->second.c_str() );
  }

  // Set iipsrv version
  TIFFSetField( tiff, TIFFTAG_SOFTWARE, "iipsrv/" VERSION );
}



/// Write ICC profile
void TIFFCompressor::writeICCProfile()
{
  // Skip if profile embedding disabled or if profile does not exist
  if( !embedICC || icc.empty() ) return;

  if( TIFFSetField( tiff, TIFFTAG_ICCPROFILE, icc.size(), icc.c_str() ) != 1 ){
    throw string( "TIFFCompressor :: Error writing ICC profile" );
  }
}



/// Write XMP profile
void TIFFCompressor::writeXMPMetadata()
{
  // Skip if XMP embedding disabled or if XMP chunk does not exist
  if( !embedXMP || xmp.empty() ) return;

  if( TIFFSetField( tiff, TIFFTAG_XMLPACKET, xmp.size(), xmp.c_str() ) != 1 ){
    throw string( "TIFFCompressor :: Error writing XMP tag" );
  }
}



/// Write DPI
void TIFFCompressor::writeResolution()
{
  // Set physical resolution
  if( dpi_x || dpi_y ){
    TIFFSetField( tiff, TIFFTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER );  // set resolution to cm
    TIFFSetField( tiff, TIFFTAG_XRESOLUTION, dpi_x );
    TIFFSetField( tiff, TIFFTAG_YRESOLUTION, dpi_y );
  }
}
