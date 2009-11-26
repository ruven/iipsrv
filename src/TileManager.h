// Tile Cache Manager Class

/*  IIP Image Server

    Copyright (C) 2005 Ruven Pillay.
    Based on an LRU cache by Patrick Audley <http://blackcat.ca/lifeline/query.php/tag=LRU_CACHE>
    Copyright (C) 2004 by Patrick Audley

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


#ifndef _TILEMANAGER_H
#define _TILEMANAGER_H


#include <fstream>

#include "RawTile.h"
#include "IIPImage.h"
#include "JPEGCompressor.h"
#include "Cache.h"
#include "Timer.h"



/// Class to manage access to the tile cache and tile cropping

class TileManager{


 private:

  Cache* tileCache;
  JPEGCompressor* jpeg;
  IIPImage* image;
  std::ofstream* logfile;
  int loglevel;
  Timer compression_timer, tile_timer, insert_timer;

  /// Get a new tile from the image file
  /**
   *  If the JPEG tile already exists in the cache, use that, otherwise check for
   *  an uncompressed tile. If that does not exist either, extract a tile from the
   *  image. If this is an edge tile, crop it.
   *  @param resolution resolution number
   *  @param tile tile number
   *  @param xangle horizontal sequence number
   *  @param yangle vertical sequence number
   *  @param number of quality layers within image to decode
   *  @param c CompressionType
   *  @return RawTile
   */
  RawTile getNewTile( int resolution, int tile, int xangle, int yangle, int layers, CompressionType c );


  /// Crop a tile to remove padding
  /** @param t pointer to tile to crop
   */
  void crop( RawTile* t );


 public:


  /// Constructor
  /**
   * @param tc pointer to tile cache object
   * @param im pointer to IIPImage object
   * @param j  pointer to JPEGCompressor object
   * @param s  pointer to output file stream
   * @param l  logging level
   */
  TileManager( Cache* tc, IIPImage* im, JPEGCompressor* j, std::ofstream* s, int l ){
    tileCache = tc; 
    image = im;
    jpeg = j;
    logfile = s ;
    loglevel = l;
  };



  /// Get a tile from the cache
  /**
   *  If the JPEG tile already exists in the cache, use that, otherwise check for
   *  an uncompressed tile. If that does not exist either, extract a tile from the
   *  image. If this is an edge tile, crop it.
   *  @param resolution resolution number
   *  @param tile tile number
   *  @param xangle horizontal sequence number
   *  @param yangle vertical sequence number
   *  @param layers number of quality layers within image to decode
   *  @param c CompressionType
   *  @return RawTile
   */
  RawTile getTile( int resolution, int tile, int xangle, int yangle, int layers, CompressionType c );


};


#endif
