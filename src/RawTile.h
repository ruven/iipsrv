// RawTile class

/*  IIP Image Server

    Copyright (C) 2000-2016 Ruven Pillay.

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
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>

// convenience defines
#define byte unsigned char
#define uint unsigned int
#define ulong unsigned long

using namespace std;

extern ofstream logfile;

/// Colour spaces - GREYSCALE, sRGB and CIELAB
enum ColourSpaces { NONE, GREYSCALE, sRGB, CIELAB };

/// Compression Types
enum CompressionType { UNCOMPRESSED, JPEG, DEFLATE, PNG };

/// Sample Types
enum SampleType { FIXEDPOINT, FLOATINGPOINT };


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

  /// Tile timestamp
  time_t timestamp;

  /// Pointer to the image data
  void *data;

  /// This tracks whether we have allocated memory locally for data
  /// or whether it is simply a pointer
  /** This is used in the destructor to make sure we deallocate correctly */
  int memoryManaged;

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

  /// Sample format type (fixed or floating point)
  SampleType sampleType;

  /// Padded
  bool padded;


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
	   int w = 0, int h = 0, int c = 0, int b = 0 ) {
    width = w; height = h; bpc = b; dataLength = 0; data = NULL;
    tileNum = tn; resolution = res; hSequence = hs ; vSequence = vs;
    memoryManaged = 1; channels = c; compressionType = UNCOMPRESSED; quality = 0;
    timestamp = 0; sampleType = FIXEDPOINT; padded = false;
  };

  byte* serialize(unsigned long &bufferlength) {
    bufferlength =    sizeof(bool)
                    + sizeof(int)*9
                    + sizeof(unsigned int)*2
                    + sizeof(time_t)
                    + sizeof(SampleType)
                    + sizeof(CompressionType)
                    + dataLength
                    + filename.size();

    // allocate our buffer space
    byte* buff = new byte[bufferlength];

    long i = 0;
    memcpy(&buff[i], &padded,            sizeof(bool));              i+=sizeof(bool);
    memcpy(&buff[i], &tileNum,           sizeof(int));               i+=sizeof(int);
    memcpy(&buff[i], &resolution,        sizeof(int));               i+=sizeof(int);
    memcpy(&buff[i], &hSequence,         sizeof(int));               i+=sizeof(int);
    memcpy(&buff[i], &vSequence,         sizeof(int));               i+=sizeof(int);
    memcpy(&buff[i], &quality,           sizeof(int));               i+=sizeof(int);
    memcpy(&buff[i], &memoryManaged,     sizeof(int));               i+=sizeof(int);
    memcpy(&buff[i], &channels,          sizeof(int));               i+=sizeof(int);
    memcpy(&buff[i], &bpc,               sizeof(int));               i+=sizeof(int);
    memcpy(&buff[i], &width,             sizeof(unsigned int));      i+=sizeof(unsigned int);
    memcpy(&buff[i], &height,            sizeof(unsigned int));      i+=sizeof(unsigned int);
    memcpy(&buff[i], &timestamp,         sizeof(time_t));            i+=sizeof(time_t);
    memcpy(&buff[i], &sampleType,        sizeof(SampleType));        i+=sizeof(SampleType);
    memcpy(&buff[i], &compressionType,   sizeof(CompressionType));   i+=sizeof(CompressionType);
    memcpy(&buff[i], &dataLength,        sizeof(int));               i+=sizeof(int);
    memcpy(&buff[i], data,               dataLength);                i+=dataLength;
    memcpy(&buff[i], filename.c_str(),   filename.size());           i+=filename.size();

    // return the serialized structure to the caller
    if ( i == bufferlength ) {
      return buff;
    }

    // else Houston, we have a problem
    logfile << "ERROR: Could not serialize!!!" << endl;
    bufferlength = 0;
    if ( buff != NULL )
      delete[] buff;
    return NULL;
  }

  bool deserialize(byte *buff, unsigned long bufferlength) {
    long i = 0;
    memcpy(&padded,          &buff[i], sizeof(bool));              i+=sizeof(bool);
    memcpy(&tileNum,         &buff[i], sizeof(int));               i+=sizeof(int);
    memcpy(&resolution,      &buff[i], sizeof(int));               i+=sizeof(int);
    memcpy(&hSequence,       &buff[i], sizeof(int));               i+=sizeof(int);
    memcpy(&vSequence,       &buff[i], sizeof(int));               i+=sizeof(int);
    memcpy(&quality,         &buff[i], sizeof(int));               i+=sizeof(int);
    memcpy(&memoryManaged,   &buff[i], sizeof(int));               i+=sizeof(int);
    memcpy(&channels,        &buff[i], sizeof(int));               i+=sizeof(int);
    memcpy(&bpc,             &buff[i], sizeof(int));               i+=sizeof(int);
    memcpy(&width,           &buff[i], sizeof(unsigned int));      i+=sizeof(unsigned int);
    memcpy(&height,          &buff[i], sizeof(unsigned int));      i+=sizeof(unsigned int);
    memcpy(&timestamp,       &buff[i], sizeof(time_t));            i+=sizeof(time_t);
    memcpy(&sampleType,      &buff[i], sizeof(SampleType));        i+=sizeof(SampleType);
    memcpy(&compressionType, &buff[i], sizeof(CompressionType));   i+=sizeof(CompressionType);
    memcpy(&dataLength,      &buff[i], sizeof(int));               i+=sizeof(int);

    // get the tile data
    if (dataLength > 0) {
        data = new byte[dataLength];
        memcpy(data,         &buff[i], dataLength);                i+=dataLength;
        memoryManaged = 1;
    }
    else {
        data = NULL;
    }

    // get the filename
    long flen = bufferlength-i;
    char fname[flen+1];
    memcpy(fname,        &buff[i], flen);
    fname[flen] = '\0';
    filename = fname;
    i+=filename.size();

    // return the deserialized structure to the caller
    if ( i == bufferlength ) {
      return true;
    }

    // Houston, we have a problem
    logfile << "ERROR: Could not deserialize!!!" << endl;
    dataLength = 0;
    if (data != NULL)
      free( data );
    return false;
  }

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
    }
  }


  /// Copy constructor - handles copying of data buffer
  RawTile( const RawTile& tile ) {

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

    switch( bpc ){
      case 32:
	if( sampleType == FLOATINGPOINT ) data = new float[dataLength/4];
	else data = new unsigned int[dataLength/4];
	break;
      case 16:
	data = new unsigned short[dataLength/2];
	break;
      default:
	data = new unsigned char[dataLength];
	break;
    }

    if( data && (dataLength > 0) && tile.data ){
      memcpy( data, tile.data, dataLength );
      memoryManaged = 1;
    }
  }


  /// Copy assignment constructor
  RawTile& operator= ( const RawTile& tile ) {

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

    switch( bpc ){
      case 32:
	if( sampleType == FLOATINGPOINT ) data = new float[dataLength/4];
	else data = new int[dataLength/4];
	break;
      case 16:
	data = new unsigned short[dataLength/2];
	break;
      default:
	data = new unsigned char[dataLength];
	break;
    }

    if( data && (dataLength > 0) && tile.data ){
      memcpy( data, tile.data, dataLength );
      memoryManaged = 1;
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
