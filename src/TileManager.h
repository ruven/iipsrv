// Tile Cache Manager Class

/*  IIP Image Server

    Copyright (C) 2005-2023 Ruven Pillay.

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


#ifndef _TILEMANAGER_H
#define _TILEMANAGER_H


#include "RawTile.h"
#include "IIPImage.h"
#include "JPEGCompressor.h"
#include "TIFFCompressor.h"
#ifdef HAVE_PNG
#include "PNGCompressor.h"
#endif
#ifdef HAVE_WEBP
#include "WebPCompressor.h"
#endif
#ifdef HAVE_AVIF
#include "AVIFCompressor.h"
#endif
#include "Cache.h"
#include "Timer.h"
#include "Watermark.h"
#include "Logger.h"


/// Class to manage access to the tile cache

class TileManager{


 private:

  Cache* tileCache;
  Compressor* compressor;
  IIPImage* image;
  Watermark* watermark;
  Logger* logfile;
  int loglevel;
  Timer compression_timer, tile_timer, insert_timer;

  /// Get a new tile from the image file
  /**
   *  If the encoded tile already exists in the cache, use that, otherwise check for
   *  an uncompressed tile. If that does not exist either, extract a tile from the
   *  image.
   *  @param resolution resolution number
   *  @param tile tile number
   *  @param xangle horizontal sequence number
   *  @param yangle vertical sequence number
   *  @param number of quality layers within image to decode
   *  @param e tile encoding
   *  @return RawTile
   */
  RawTile getNewTile( int resolution, int tile, int xangle, int yangle, int layers, ImageEncoding e );


 public:


  /// Constructor
  /**
   * @param tc pointer to tile cache object
   * @param im pointer to IIPImage object
   * @param w  pointer to watermark object
   * @param c  pointer to Compressor object
   * @param s  pointer to Logger object
   * @param l  logging level
   */
  TileManager( Cache* tc, IIPImage* im, Watermark* w, Compressor* c, Logger* s, int l ){
    tileCache = tc; 
    image = im;
    watermark = w;
    compressor = c;
    logfile = s ;
    loglevel = l;
  };



  /// Get a tile from the cache
  /**
   *  If the encoded tile already exists in the cache, use that, otherwise check for
   *  an uncompressed tile. If that does not exist either, extract a tile from the
   *  image.
   *  @param resolution resolution number
   *  @param tile tile number
   *  @param xangle horizontal sequence number
   *  @param yangle vertical sequence number
   *  @param layers number of quality layers within image to decode
   *  @param c Compression
   *  @return RawTile
   */
  RawTile getTile( int resolution, int tile, int xangle, int yangle, int layers, ImageEncoding c );



  /// Generate a complete region
  /**
   *  Build up an arbitrary region by extracting tiles from the cache by using getTile function.
   *  Data returned as uncompressed raw data.
   *  @param res resolution number
   *  @param xangle horizontal sequence number
   *  @param yangle vertical sequence number
   *  @param layers number of quality layers within image to decode
   *  @param x left offset with respect to full image
   *  @param y top offset with respect to full image
   *  @param w width of region requested
   *  @param h height of region requested
   *  @return RawTile
   */
    RawTile getRegion( unsigned int res, int xangle, int yangle, int layers, unsigned int x, unsigned int y, unsigned int w, unsigned int h );

};


#endif
