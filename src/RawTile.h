// RawTile class

/*  IIP Image Server

    Copyright (C) 2000-2009 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifndef _RAWTILE_H
#define _RAWTILE_H

#include <cstring>
#include <string>
#include <cstdlib>


/// Colour spaces - GREYSCALE, sRGB and CIELAB
enum ColourSpaces { GREYSCALE, sRGB, CIELAB };

/// Compression Types
enum CompressionType { UNCOMPRESSED, JPEG, DEFLATE };



/// Class to represent a single image tile

class RawTile{

 public:

  /// The tile number for this tile
  int tileNum;

  /// The resolution to which this tile belongs
  int resolution;

  /// The horizontal angle to which this tile belongs
  int hSequence;

  /// The vertical angle to which this tile belongs
  int vSequence;

  /// Compression type
  CompressionType compressionType;

  /// Compression rate or quality
  int quality;

  /// Name of the file from which this tile comes
  std::string filename;


 public:


  /// Pointer to the image data
  void *data;

  /// This tracks whether we have allocated memory locally for data
  /// or whether it is simply a pointer
  /** This is used in the destructor to make sure we deallocate correctly */
  int localData;

  /// The size of the data pointed to by data
  int dataLength;

  /// The width in pixels of this tile
  unsigned int width;

  /// The height in pixels of this tile
  unsigned int height;

  /// The number of channels for this tile
  int channels;

  /// The number of bits per channel for this tile
  int bpc;


  /// Main constructor
  /** \param tn tile number
      \param res resolution
      \param hs horizontal sequence angle
      \param vs vertical sequence angle
      \param w tile width
      \param h tile height
      \param c number of channels
      \param b bits per channel per sample
  */
  RawTile( int tn = 0, int res = 0, int hs = 0, int vs = 0,
	   int w = 0, int h = 0, int c = 0, int b = 0 ) {
    width = w; height = h; bpc = b; dataLength = 0; data = NULL;
    tileNum = tn; resolution = res; hSequence = hs ; vSequence = vs;
    localData = 0; channels = c; compressionType = UNCOMPRESSED; quality = 0;
  };


  /// Destructor to free the data array if is has previously be allocated locally
  ~RawTile() {
    if( data && localData ){
      if(bpc==16) delete[] (unsigned short*) data;
      else delete[] (unsigned char*) data;
    }
  }


  /// Copy constructor - handles copying of data buffer
  RawTile( const RawTile& tile ) {

    dataLength = tile.dataLength;
    width = tile.width;
    height = tile.height;
    channels = tile.channels;
    bpc = tile.bpc;
    tileNum = tile.tileNum;
    resolution = tile.resolution;
    hSequence = tile.hSequence;
    vSequence = tile.vSequence;
    compressionType = tile.compressionType;
    quality = tile.quality;
    filename = tile.filename;

    if( bpc == 16 ) data = new unsigned short[dataLength/2];
    else data = new unsigned char[dataLength];

    if( data && (dataLength > 0) && tile.data ){
      memcpy( data, tile.data, dataLength );
      localData = 1;
    }
  }


  /// Copy assignment constructor
  RawTile& operator= ( const RawTile& tile ) {

    dataLength = tile.dataLength;
    width = tile.width;
    height = tile.height;
    channels = tile.channels;
    bpc = tile.bpc;
    tileNum = tile.tileNum;
    resolution = tile.resolution;
    hSequence = tile.hSequence;
    vSequence = tile.vSequence;
    compressionType = tile.compressionType;
    quality = tile.quality;
    filename = tile.filename;

    if( bpc == 16 ) data = new unsigned short[dataLength/2];
    else data = new unsigned char[dataLength];

    if( data && (dataLength > 0) && tile.data ){
      memcpy( data, tile.data, dataLength );
      localData = 1;
    }

    return *this;
  }


  /// Return the size of the data
  int size() { return dataLength; }


  /// Overloaded equality operator
  friend int operator == ( const RawTile& A, const RawTile& B ) {
    if( (A.tileNum == B.tileNum) &&
	(A.resolution == B.resolution) &&
	(A.hSequence == B.hSequence) &&
	(A.vSequence == B.vSequence) &&
	(A.compressionType == B.compressionType) &&
	(A.quality == B.quality) &&
	(A.filename == B.filename) ){
      return( 1 );
    }
    else return( 0 );
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
      return( 0 );
    }
    else return( 1 );
  }


};


#endif
