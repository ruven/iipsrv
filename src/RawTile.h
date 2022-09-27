/// RawTile class

/*  IIPImage Server

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


#ifndef _RAWTILE_H
#define _RAWTILE_H

#include <cstring>
#include <string>
#include <cstdlib>
#include <ctime>

#if !( (__cplusplus >= 201103L) || ((defined(_MSC_VER) && _MSC_VER >= 1900)) )
#include <stdint.h>          // Required for C++98
#endif


/// Colour spaces - GREYSCALE, sRGB and CIELAB
enum ColourSpaces { NONE, GREYSCALE, sRGB, CIELAB, BINARY };

/// Compression Types
enum CompressionType { UNCOMPRESSED, JPEG, DEFLATE, PNG, WEBP };

/// Sample Types
enum SampleType { FIXEDPOINT, FLOATINGPOINT };



/// Class to represent a single image tile
class RawTile {

 public:

  /// Name of the file from which this tile comes
  std::string filename;

  /// The width in pixels of this tile
  unsigned int width;

  /// The height in pixels of this tile
  unsigned int height;

  /// The number of channels for this tile
  int channels;

  /// The number of bits per channel for this tile
  int bpc;

  /// Sample format type (fixed or floating point)
  SampleType sampleType;

  /// Compression type
  CompressionType compressionType;

  /// Compression rate or quality
  int quality;

  /// Tile timestamp
  time_t timestamp;

  /// The tile number for this tile
  int tileNum;

  /// The resolution number to which this tile belongs
  int resolution;

  /// The horizontal angle to which this tile belongs
  int hSequence;

  /// The vertical angle to which this tile belongs
  int vSequence;

  /// Wehther image is padded
  bool padded;

  /// Amount of memory actually allocated in bytes
  uint32_t capacity;

  /// The size of the data pointed to by the data pointer in bytes
  uint32_t dataLength;

  /// This tracks whether we have allocated memory locally for data
  /// or whether it is simply a pointer
  /** This is used in the destructor to make sure we deallocate correctly */
  int memoryManaged;

  /// Pointer to the image data
  void *data;


  /// Main constructor
  /** @param tn tile number
      @param res resolution
      @param hs horizontal sequence angle
      @param vs vertical sequence angle
      @param w tile width
      @param h tile height
      @param c number of channels
      @param b bits per channel per sample
  */
  RawTile( int tn = 0, int res = 0, int hs = 0, int vs = 0,
	   int w = 0, int h = 0, int c = 0, int b = 0 )
    : width( w ),
      height( h ),
      channels( c ),
      bpc( b ),
      sampleType( FIXEDPOINT ),
      compressionType( UNCOMPRESSED ),
      quality( 0 ),
      timestamp( 0 ),
      tileNum( tn ),
      resolution( res ),
      hSequence( hs ),
      vSequence( vs ),
      padded( false ),
      capacity( 0 ),
      dataLength( 0 ),
      memoryManaged( 1 ),
      data( NULL ) {};



  /// Destructor to free the data array if is has previously be allocated locally
  ~RawTile() {
    if( data && memoryManaged ){
      switch( bpc ){
        case 32:
	  if( sampleType == FLOATINGPOINT ) delete[] (float*) data;
	  else delete[] (unsigned int*) data;
	  break;
        case 16:
	  delete[] (unsigned short*) data;
	  break;
        default:
	  delete[] (unsigned char*) data;
	  break;
      }
      data = NULL;
      dataLength = 0;
      capacity = 0;
    }
  }



  /// Copy constructor - handles copying of data buffer
  RawTile( const RawTile& tile )
    : filename( tile.filename ),
      width( tile.width ),
      height( tile.height ),
      channels( tile.channels ),
      bpc( tile.bpc ),
      sampleType( tile.sampleType ),
      compressionType( tile.compressionType ),
      quality( tile.quality ),
      timestamp( tile.timestamp ),
      tileNum( tile.tileNum ),
      resolution( tile.resolution ),
      hSequence( tile.hSequence ),
      vSequence( tile.vSequence ),
      padded( tile.padded ),
      capacity( tile.capacity ),
      dataLength( tile.dataLength ),
      memoryManaged( tile.memoryManaged ),
      data( NULL )
  {

    if( tile.data && tile.dataLength > 0 ){
      allocate();
      memcpy( data, tile.data, tile.dataLength );
      memoryManaged = 1;
    }

  }



  /// Copy assignment constructor
  RawTile& operator= ( const RawTile& tile ) {

    if( this != &tile ){

      tileNum = tile.tileNum;
      resolution = tile.resolution;
      hSequence = tile.hSequence;
      vSequence = tile.vSequence;
      compressionType = tile.compressionType;
      quality = tile.quality;
      filename = tile.filename;
      timestamp = tile.timestamp;
      memoryManaged = tile.memoryManaged;
      dataLength = tile.dataLength;
      width = tile.width;
      height = tile.height;
      channels = tile.channels;
      bpc = tile.bpc;
      sampleType = tile.sampleType;
      padded = tile.padded;
      capacity = tile.capacity;

      if( tile.data && tile.dataLength > 0 ){
	allocate();
	memcpy( data, tile.data, tile.dataLength );
	memoryManaged = 1;
      }
    }

    return *this;
  }



  /// Allocate memory for the tile
  /** @param size size in bytes to allocate
   */
  void allocate( uint32_t size = 0 ) {

    if( size == 0 ) size = (uint32_t) width * height * channels * (bpc/8);

    switch( bpc ){
      case 32:
	if( sampleType == FLOATINGPOINT ) data = new float[size/4];
	else data = new int[size/4];
	break;
      case 16:
	data = new unsigned short[size/2];
	break;
      default:
	data = new unsigned char[size];
	break;
    }
    memoryManaged = 1;
    capacity = size;
  };



  /// Overloaded equality operator
  friend int operator == ( const RawTile& A, const RawTile& B ) {
    if( (A.tileNum == B.tileNum) &&
	(A.resolution == B.resolution) &&
	(A.hSequence == B.hSequence) &&
	(A.vSequence == B.vSequence) &&
	(A.compressionType == B.compressionType) &&
	(A.quality == B.quality) &&
	(A.filename == B.filename) ){
      return 1;
    }
    else return 0;
  }



  /// Overloaded non-equality operator
  friend int operator != ( const RawTile& A, const RawTile& B ) {
    if( (A.tileNum == B.tileNum) &&
	(A.resolution == B.resolution) &&
	(A.hSequence == B.hSequence) &&
	(A.vSequence == B.vSequence) &&
	(A.compressionType == B.compressionType) &&
	(A.quality == B.quality) &&
	(A.filename == B.filename) ){
      return 0;
    }
    else return 1;
  }



  // Memory efficient move operators possible in C++11 onwards
#if (__cplusplus >= 201103L) || ((defined(_MSC_VER) && _MSC_VER >= 1900))

  /// Move constructor
  RawTile( RawTile&& tile ) noexcept
    : filename( tile.filename ),
      width( tile.width ),
      height( tile.height ),
      channels( tile.channels ),
      bpc( tile.bpc ),
      sampleType( tile.sampleType ),
      compressionType( tile.compressionType ),
      quality( tile.quality ),
      timestamp( tile.timestamp ),
      tileNum( tile.tileNum ),
      resolution( tile.resolution ),
      hSequence( tile.hSequence ),
      vSequence( tile.vSequence ),
      padded( tile.padded ),
      capacity( tile.capacity ),
      dataLength( tile.dataLength ),
      memoryManaged( tile.memoryManaged ),
      data( NULL )
  {

    if( tile.memoryManaged == 1 ){

      // Transfer ownership of data
      data = tile.data;

      // Free data from other RawTile
      tile.data = nullptr;
      tile.dataLength = 0;
      tile.capacity = 0;
      tile.memoryManaged = 0;
    }
    // Need to copy data if other tile is not owner of its data
    else if( tile.data && dataLength>0 ){
      allocate();
      memcpy( data, tile.data, tile.dataLength );
      memoryManaged = 1;
    }
  }



  /// Move assignment operator
  RawTile& operator= ( RawTile&& tile ) noexcept {

    if( this != &tile ){

      if( data && dataLength>0 ) delete this;

      // Use move for std::string. The other fields are of basic type
      filename = std::move( tile.filename );
      tileNum = tile.tileNum;
      resolution = tile.resolution;
      hSequence = tile.hSequence;
      vSequence = tile.vSequence;
      compressionType = tile.compressionType;
      quality = tile.quality;
      timestamp = tile.timestamp;
      memoryManaged = tile.memoryManaged;
      capacity = tile.capacity;
      dataLength = tile.dataLength;
      width = tile.width;
      height = tile.height;
      channels = tile.channels;
      bpc = tile.bpc;
      sampleType = tile.sampleType;
      padded = tile.padded;

      if( tile.memoryManaged == 1 ){

	// Transfer ownership of raw data
	data = tile.data;

	// Free data from other tile
	tile.data = nullptr;
	tile.dataLength = 0;
	tile.capacity = 0;
	tile.memoryManaged = 0;
      }
      else if( tile.data && tile.dataLength>0 ){
	allocate();
	memcpy( data, tile.data, tile.dataLength );
	memoryManaged = 1;
      }
    }

    return *this;
  }

#endif

};

#endif
