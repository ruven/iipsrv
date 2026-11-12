/*  IIPImage EXIF metadata handling:
    Extracts EXIF and GPS data from a TIFF and writes it back as EXIF/GPS IFDs

    Copyright (C) 2026 Ruven Pillay

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


#include "EXIF.h"

#include <cstring>

using namespace std;

namespace {

  // In-memory TIFF stream used to build and parse EXIF blobs
  struct Memory {
    vector<unsigned char> data;
    toff_t position;
    Memory() : position(0) {}
  };


  // libtiff read callback for the in-memory stream
  static tmsize_t memoryRead( thandle_t handle, void* buffer, tmsize_t size ){
    if( size <= 0 ) return 0;
    Memory* memory = static_cast<Memory*>(handle);
    const size_t position = static_cast<size_t>(memory->position);
    if( position >= memory->data.size() ) return 0;
    const size_t available = memory->data.size() - position;
    const size_t count = static_cast<size_t>(size) < available ? static_cast<size_t>(size) : available;
    memcpy( buffer, memory->data.data() + position, count );
    memory->position += static_cast<toff_t>(count);
    return static_cast<tmsize_t>(count);
  }


  // libtiff write callback for the in-memory stream
  static tmsize_t memoryWrite( thandle_t handle, void* buffer, tmsize_t size ){
    if( size <= 0 ) return 0;
    Memory* memory = static_cast<Memory*>(handle);
    const size_t count = static_cast<size_t>(size);
    const size_t position = static_cast<size_t>(memory->position);
    const size_t end = position + count;
    if( end > memory->data.size() ) memory->data.resize(end);
    memcpy( memory->data.data() + position, buffer, count );
    memory->position = static_cast<toff_t>(end);
    return size;
  }


  // libtiff seek callback for the in-memory stream
  static toff_t memorySeek( thandle_t handle, toff_t offset, int whence ){
    Memory* memory = static_cast<Memory*>(handle);
    switch( whence ){
      case SEEK_SET:
        memory->position = offset;
        break;
      case SEEK_CUR:
        memory->position += offset;
        break;
      case SEEK_END:
        memory->position = static_cast<toff_t>(memory->data.size()) + offset;
        break;
    }
    return memory->position;
  }


  // libtiff close callback - nothing to release
  static int memoryClose( thandle_t ){
    return 0;
  }


  // libtiff size callback
  static toff_t memorySize( thandle_t handle ){
    return static_cast<toff_t>(static_cast<Memory*>(handle)->data.size());
  }


  // libtiff map callback - memory mapping is not supported
  static int memoryMap( thandle_t, void**, toff_t* ){
    return 0;
  }


  // libtiff unmap callback - nothing to do
  static void memoryUnmap( thandle_t, void*, toff_t ){
  }


  // Close a TIFF handle and clear the pointer
  static void closeTIFF( TIFF*& tiff ){
    if( tiff != NULL ){
      TIFF* handle = tiff;
      tiff = NULL;
      TIFFClose( handle );
    }
  }


  // Copy a single tag from one TIFF directory to another
  static bool copyTag( TIFF* source, TIFF* destination, uint32_t tag ){
    const TIFFField* field = TIFFFieldWithTag( source, tag );
    if( field == NULL ) return false;

    const int readcount = TIFFFieldReadCount( field );
    const int passcount = TIFFFieldPassCount( field );
    const TIFFDataType type = TIFFFieldDataType( field );

    if( passcount ){
      void* data = NULL;
      if( readcount == TIFF_VARIABLE2 || TIFFFieldSetGetCountSize( field ) == 4 ){
        uint32_t count = 0;
        if( !TIFFGetField( source, tag, &count, &data ) ) return false;
        return TIFFSetField( destination, tag, count, data ) == 1;
      }
      else {
        uint16_t count = 0;
        if( !TIFFGetField( source, tag, &count, &data ) ) return false;
        return TIFFSetField( destination, tag, count, data ) == 1;
      }
    }

    if( readcount == 1 ){
      switch( type ){
        case TIFF_BYTE:
        case TIFF_UNDEFINED: {
          uint8_t value = 0;
          if( !TIFFGetField( source, tag, &value ) ) return false;
          return TIFFSetField( destination, tag, value ) == 1;
        }
        case TIFF_SBYTE: {
          int8_t value = 0;
          if( !TIFFGetField( source, tag, &value ) ) return false;
          return TIFFSetField( destination, tag, value ) == 1;
        }
        case TIFF_SHORT: {
          uint16_t value = 0;
          if( !TIFFGetField( source, tag, &value ) ) return false;
          return TIFFSetField( destination, tag, value ) == 1;
        }
        case TIFF_SSHORT: {
          int16_t value = 0;
          if( !TIFFGetField( source, tag, &value ) ) return false;
          return TIFFSetField( destination, tag, value ) == 1;
        }
        case TIFF_LONG:
        case TIFF_IFD: {
          uint32_t value = 0;
          if( !TIFFGetField( source, tag, &value ) ) return false;
          return TIFFSetField( destination, tag, value ) == 1;
        }
        case TIFF_SLONG: {
          int32_t value = 0;
          if( !TIFFGetField( source, tag, &value ) ) return false;
          return TIFFSetField( destination, tag, value ) == 1;
        }
        case TIFF_LONG8:
        case TIFF_IFD8: {
          uint64_t value = 0;
          if( !TIFFGetField( source, tag, &value ) ) return false;
          return TIFFSetField( destination, tag, value ) == 1;
        }
        case TIFF_SLONG8: {
          int64_t value = 0;
          if( !TIFFGetField( source, tag, &value ) ) return false;
          return TIFFSetField( destination, tag, value ) == 1;
        }
        case TIFF_RATIONAL:
        case TIFF_SRATIONAL: {
          if( TIFFFieldSetGetSize( field ) == 8 ){
            double value = 0.0;
            if( !TIFFGetField( source, tag, &value ) ) return false;
            return TIFFSetField( destination, tag, value ) == 1;
          }
          else {
            float value = 0.0f;
            if( !TIFFGetField( source, tag, &value ) ) return false;
            return TIFFSetField( destination, tag, value ) == 1;
          }
        }
        case TIFF_FLOAT: {
          float value = 0.0f;
          if( !TIFFGetField( source, tag, &value ) ) return false;
          return TIFFSetField( destination, tag, value ) == 1;
        }
        case TIFF_DOUBLE: {
          double value = 0.0;
          if( !TIFFGetField( source, tag, &value ) ) return false;
          return TIFFSetField( destination, tag, value ) == 1;
        }
        case TIFF_ASCII: {
          char* value = NULL;
          if( !TIFFGetField( source, tag, &value ) ) return false;
          return TIFFSetField( destination, tag, value ) == 1;
        }
        default:
          return false;
      }
    }

    // Fixed-count arrays (readcount > 1): TIFFGetField returns a pointer
    // to internal storage, which we pass directly to TIFFSetField
    if( readcount > 1 ){
      void* data = NULL;
      if( !TIFFGetField( source, tag, &data ) ) return false;
      return TIFFSetField( destination, tag, data ) == 1;
    }

    // TIFF_VARIABLE without passcount — undefined get/set semantics, skip
    return false;
  }


  // Endian-aware readers for TIFF header and IFD fields
  static uint16_t read16( const unsigned char* p, bool little ){
    return little ? static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8)
                  : (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
  }

  static uint32_t read32( const unsigned char* p, bool little ){
    if( little ){
      return static_cast<uint32_t>(p[0]) |
             (static_cast<uint32_t>(p[1]) << 8) |
             (static_cast<uint32_t>(p[2]) << 16) |
             (static_cast<uint32_t>(p[3]) << 24);
    }
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
  }

  static uint64_t read64( const unsigned char* p, bool little ){
    uint64_t value = 0;
    if( little ){
      for( int i = 0; i < 8; ++i ) value |= static_cast<uint64_t>(p[i]) << ( 8 * i );
    }
    else {
      for( int i = 0; i < 8; ++i ) value = ( value << 8 ) | static_cast<uint64_t>(p[i]);
    }
    return value;
  }


  // Patch the first-IFD offset in a classic TIFF header (little or big endian).
  // Extraction always emits a classic TIFF, so BigTIFF is not handled here.
  static void setHeaderOffset( vector<unsigned char>& data, uint64_t offset ){
    if( data.size() < 8 ) return;

    const uint32_t value = static_cast<uint32_t>(offset);
    if( data[0] == 'I' && data[1] == 'I' ){
      data[4] = value & 0xff;
      data[5] = (value >> 8) & 0xff;
      data[6] = (value >> 16) & 0xff;
      data[7] = (value >> 24) & 0xff;
    }
    else if( data[0] == 'M' && data[1] == 'M' ){
      data[4] = (value >> 24) & 0xff;
      data[5] = (value >> 16) & 0xff;
      data[6] = (value >> 8) & 0xff;
      data[7] = value & 0xff;
    }
  }


  // Read the first-IFD offset from a classic or BigTIFF header
  static toff_t headerOffset( const vector<unsigned char>& data ){
    if( data.size() < 8 ) return 0;

    const bool little = data[0] == 'I' && data[1] == 'I';
    const bool big = data[0] == 'M' && data[1] == 'M';
    if( !little && !big ) return 0;

    const uint16_t magic = read16( data.data() + 2, little );
    if( magic == 42 ){
      return static_cast<toff_t>( read32( data.data() + 4, little ) );
    }
    if( magic == 43 ){
      if( data.size() < 16 ) return 0;
      return static_cast<toff_t>( read64( data.data() + 8, little ) );
    }
    return 0;
  }


  // Offsets of the EXIF and GPS sub-IFDs referenced by an IFD0
  struct SubIFDOffsets {
    toff_t exif;
    toff_t gps;
    SubIFDOffsets() : exif(0), gps(0) {}
  };


  // Parse the IFD at ifd_offset and locate the EXIF (0x8769) and GPS (0x8825)
  // sub-IFD pointers. If neither pointer is present the IFD is assumed to be a
  // legacy standalone EXIF IFD, so its offset is returned as the EXIF offset.
  // Both classic TIFF (12-byte entries) and BigTIFF (20-byte entries) are handled.
  static SubIFDOffsets resolveSubIFDs( const vector<unsigned char>& data, toff_t ifd_offset ){
    SubIFDOffsets result;
    if( data.size() < 8 || ifd_offset == 0 || ifd_offset >= data.size() ) return result;

    const bool little = (data[0] == 'I' && data[1] == 'I');
    const bool big = (data[0] == 'M' && data[1] == 'M');
    if( !little && !big ) return result;

    const uint16_t magic = read16( data.data() + 2, little );
    const bool bigtiff = ( magic == 43 );
    if( magic != 42 && !bigtiff ) return result;

    const size_t offset_val = static_cast<size_t>(ifd_offset);
    const size_t count_size = bigtiff ? 8 : 2;
    const size_t entry_size = bigtiff ? 20 : 12;
    const size_t value_offset = bigtiff ? 12 : 8;

    if( data.size() - offset_val < count_size ) return result;

    const uint64_t count = bigtiff
        ? read64( data.data() + offset_val, little )
        : static_cast<uint64_t>( read16( data.data() + offset_val, little ) );

    // Ensure the entire entry table is in bounds without overflowing
    if( count > ( data.size() - offset_val - count_size ) / entry_size ) return result;

    bool has_exif_pointer = false;
    bool has_gps_pointer = false;
    for( uint64_t i = 0; i < count; ++i ){
      const size_t entry_offset = offset_val + count_size + static_cast<size_t>(i) * entry_size;
      const uint16_t tag = read16( data.data() + entry_offset, little );
      if( tag != 0x8769 && tag != 0x8825 ) continue;

      if( tag == 0x8769 ) has_exif_pointer = true;
      else has_gps_pointer = true;

      const uint64_t subifd_offset = bigtiff
          ? read64( data.data() + entry_offset + value_offset, little )
          : static_cast<uint64_t>( read32( data.data() + entry_offset + value_offset, little ) );
      if( subifd_offset == 0 || subifd_offset >= data.size() ) continue;

      if( tag == 0x8769 ) result.exif = static_cast<toff_t>(subifd_offset);
      else result.gps = static_cast<toff_t>(subifd_offset);
    }

    // No sub-IFD pointers: treat this as a legacy standalone EXIF IFD
    if( !has_exif_pointer && !has_gps_pointer ){
      result.exif = ifd_offset;
    }
    return result;
  }


  // Sub-IFD pointer tags must not be copied verbatim: their values are file
  // offsets into the source and would dangle in the destination.
  static bool isPointerTag( uint32_t tag ){
    return tag == 0x8769 || tag == 0x8825 || tag == 0xA005 || tag == 0x014A;
  }


  // Copy all non-pointer tags from the current source directory into the
  // current destination directory
  static void copyDirectoryTags( TIFF* source, TIFF* destination ){
    const int tag_count = TIFFGetTagListCount( source );
    for( int i = 0; i < tag_count; ++i ){
      const uint32_t tag = TIFFGetTagListEntry( source, i );
      if( isPointerTag( tag ) ) continue;
      if( TIFFFieldWithTag( destination, tag ) == NULL ) continue;
      copyTag( source, destination, tag );
    }
  }

}


/// Extract the EXIF and GPS directories from a TIFF as a standalone EXIF blob
bool exif::extract( TIFF* source, vector<unsigned char>& data,
                    uint64_t exif_offset, uint64_t gps_offset )
{
  data.clear();

  // Older libtiff versions have no GPS directory API, so ignore GPS entirely
#ifndef HAVE_TIFF_GPS
  gps_offset = 0;
#endif

  if( source == NULL || ( exif_offset == 0 && gps_offset == 0 ) ) return false;

  Memory memory;
  TIFF* destination = TIFFClientOpen( "iipsrv-exif", "w", &memory,
                                      memoryRead, memoryWrite, memorySeek,
                                      memoryClose, memorySize, memoryMap, memoryUnmap );
  if( destination == NULL ) return false;

  uint64_t dest_exif_offset = 0;
#ifdef HAVE_TIFF_GPS
  uint64_t dest_gps_offset = 0;
#endif

  // Copy the EXIF IFD, if present
  if( exif_offset != 0 ){
    if( TIFFReadEXIFDirectory( source, exif_offset ) != 1 ){
      closeTIFF( destination );
      return false;
    }
    TIFFCreateEXIFDirectory( destination );
    copyDirectoryTags( source, destination );
    if( TIFFWriteCustomDirectory( destination, &dest_exif_offset ) != 1 ){
      closeTIFF( destination );
      return false;
    }
  }

#ifdef HAVE_TIFF_GPS
  // Copy the GPS IFD, if present.  GPS is best effort: if it cannot be read we
  // still return any EXIF data that was successfully copied.
  if( gps_offset != 0 && TIFFReadGPSDirectory( source, gps_offset ) == 1 ){
    TIFFCreateGPSDirectory( destination );
    copyDirectoryTags( source, destination );
    if( TIFFWriteCustomDirectory( destination, &dest_gps_offset ) != 1 ){
      dest_gps_offset = 0;
    }
  }
#endif

  // Build an IFD0 that references the copied sub-IFDs
  TIFFCreateDirectory( destination );
  if( dest_exif_offset != 0 ) TIFFSetField( destination, TIFFTAG_EXIFIFD, dest_exif_offset );
#ifdef HAVE_TIFF_GPS
  if( dest_gps_offset != 0 ) TIFFSetField( destination, TIFFTAG_GPSIFD, dest_gps_offset );
#endif

  uint64_t ifd0_offset = 0;
  if( TIFFWriteCustomDirectory( destination, &ifd0_offset ) != 1 ){
    closeTIFF( destination );
    return false;
  }

  TIFFCreateDirectory( destination );
  closeTIFF( destination );
  data = memory.data;
  setHeaderOffset( data, ifd0_offset );
  return true;
}


/// Write an EXIF blob into a TIFF as EXIF and GPS IFDs
bool exif::write( TIFF* destination, const string& data,
                  uint64_t& exif_offset, uint64_t& gps_offset )
{
  exif_offset = 0;
  gps_offset = 0;
  if( destination == NULL || data.empty() ) return false;

  size_t start = 0;
  if( data.size() >= 6 && memcmp( data.data(), "Exif\0\0", 6 ) == 0 ) start = 6;
  if( start >= data.size() ) return false;

  Memory memory;
  memory.data.assign( data.begin() + start, data.end() );

  TIFF* source = TIFFClientOpen( "iipsrv-exif-source", "rhm", &memory,
                                 memoryRead, memoryWrite, memorySeek,
                                 memoryClose, memorySize, memoryMap, memoryUnmap );
  if( source == NULL ) return false;

  const toff_t ifd_offset = headerOffset( memory.data );
  SubIFDOffsets offsets = resolveSubIFDs( memory.data, ifd_offset );

  // Older libtiff versions have no GPS directory API, so ignore GPS entirely
#ifndef HAVE_TIFF_GPS
  offsets.gps = 0;
#endif

  if( offsets.exif == 0 && offsets.gps == 0 ){
    closeTIFF( source );
    return false;
  }

  bool wrote_exif = false;
  bool wrote_gps = false;

  if( offsets.exif != 0 ){
    if( TIFFReadEXIFDirectory( source, offsets.exif ) != 1 ){
      closeTIFF( source );
      return false;
    }
    TIFFCreateEXIFDirectory( destination );
    copyDirectoryTags( source, destination );
    if( TIFFWriteCustomDirectory( destination, &exif_offset ) != 1 ){
      closeTIFF( source );
      TIFFCreateDirectory( destination );
      return false;
    }
    wrote_exif = true;
  }

  // GPS is best effort: if it cannot be read or written we still keep the EXIF IFD
#ifdef HAVE_TIFF_GPS
  if( offsets.gps != 0 && TIFFReadGPSDirectory( source, offsets.gps ) == 1 ){
    TIFFCreateGPSDirectory( destination );
    copyDirectoryTags( source, destination );
    if( TIFFWriteCustomDirectory( destination, &gps_offset ) == 1 ){
      wrote_gps = true;
    }
    else {
      gps_offset = 0;
    }
  }
#endif

  closeTIFF( source );
  TIFFCreateDirectory( destination );
  return wrote_exif || wrote_gps;
}
