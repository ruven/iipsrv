// Tile Cache Class

/*  IIP Image Server

    Copyright (C) 2005-2009 Ruven Pillay.
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



#ifndef _CACHE_H
#define _CACHE_H


// Use the hashmap extensions if we are using >= gcc 3.1
#ifdef __GNUC__

#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 1) || (__GNUC__ >= 4)
#define USE_HASHMAP 1
#include <ext/hash_map>
namespace __gnu_cxx
{
  template<> struct hash< const std::string > {
    size_t operator()( const std::string& x ) const {
      return hash< const char* >()( x.c_str() );
    }
  };
}
#endif

// And the high performance memory pool allocator if >= gcc 3.4
#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || (__GNUC__ >= 4)
#define POOL_ALLOCATOR 1
#include <ext/pool_allocator.h>
#endif

#endif


#ifndef USE_HASHMAP
#include <map>
#endif

#include <iostream>
#include <list>
#include <string>
#include "RawTile.h"



/// Cache to store raw tile data

class Cache {


 private:

  /// Max memory size in bytes
  unsigned long maxSize;

  /// Basic object storage size
  int tileSize;

  /// Current memory running total
  unsigned long currentSize;

  /// Main cache storage typedef
#ifdef POOL_ALLOCATOR
  typedef std::list < std::pair<const std::string,RawTile>,
    __gnu_cxx::__pool_alloc< std::pair<const std::string,RawTile> > > TileList;
#else
  typedef std::list < std::pair<const std::string,RawTile> > TileList;
#endif

  /// Main cache list iterator typedef
  typedef std::list < std::pair<const std::string,RawTile> >::iterator List_Iter;

  /// Index typedef
#ifdef USE_HASHMAP
#ifdef POOL_ALLOCATOR
  typedef __gnu_cxx::hash_map < const std::string, List_Iter,
    __gnu_cxx::hash< const std::string >,
    std::equal_to< const std::string >,
    __gnu_cxx::__pool_alloc< std::pair<const std::string, List_Iter> >
    > TileMap;
#else
  typedef __gnu_cxx::hash_map < const std::string,List_Iter > TileMap;
#endif
#else
  typedef std::map < const std::string,List_Iter > TileMap;
#endif

  /// Main cache storage object
  TileList tileList;

  /// Main Cache storage index object
  TileMap tileMap;


  /// Internal touch function
  /** Touches a key in the Cache and makes it the most recently used
   *  @param key to be touched
   *  @return a Map_Iter pointing to the key that was touched.
   */
  TileMap::iterator _touch( const std::string &key ) {
    TileMap::iterator miter = tileMap.find( key );
    if( miter == tileMap.end() ) return miter;
    // Move the found node to the head of the list.
    tileList.splice( tileList.begin(), tileList, miter->second );
    return miter;
  }


  /// Interal remove function
  /**
   *  @param miter Map_Iter that points to the key to remove
   *  @warning miter is now longer usable after being passed to this function.
   */
  void _remove( const TileMap::iterator &miter ) {
    // Reduce our current size counter
    currentSize -= ( (miter->second->second).dataLength +
		     (miter->second->second).filename.length()*sizeof(char) +
		     tileSize );
    tileList.erase( miter->second );
    tileMap.erase( miter );
  }


  /// Interal remove function
  /** @param key to remove */
  void _remove( const std::string &key ) {
    TileMap::iterator miter = tileMap.find( key );
    this->_remove( miter );
  }



 public:

  /// Constructor
  /** @param max Maximum cache size in MB */
  Cache( float max ) {
    maxSize = (unsigned long)(max*1024000) ; currentSize = 0;
    // 128 added at the end represents 2*average strings lengths
    tileSize = sizeof( RawTile ) + sizeof( std::pair<const std::string,RawTile> ) +
      sizeof( std::pair<const std::string, List_Iter> ) + 128;
  };


  /// Destructor
  ~Cache() {
    tileList.clear();
    tileMap.clear();
  }


  /// Insert a tile
  /** @param r Tile to be inserted */
  void insert( const RawTile& r ) {

    if( maxSize == 0 ) return;

    std::string key = this->getIndex( r.filename, r.resolution, r.tileNum,
				      r.hSequence, r.vSequence, r.compressionType, r.quality );

    // Touch the key, if it exists
    TileMap::iterator miter = this->_touch( key );

    // Check whether this tile exists in our cache
    if( miter != tileMap.end() ){
      // Check the timestamp and delete if necessary
      if( miter->second->second.timestamp < r.timestamp ){
	this->_remove( miter );
      }
      // If this index already exists and it is up to date, do nothing
      else return;
    }

    // Store the key if it doesn't already exist in our cache
    // Ok, do the actual insert at the head of the list
    tileList.push_front( std::make_pair(key,r) );

    // And store this in our map
    List_Iter liter = tileList.begin();
    tileMap[ key ] = liter;

    // Update our total current size variable
    currentSize += (r.dataLength + r.filename.length()*sizeof(char) + tileSize);

    // Check to see if we need to remove an element due to exceeding max_size
    while( currentSize > maxSize ) {
      // Remove the last element
      liter = tileList.end();
      --liter;
      this->_remove( liter->first );
    }

  }


  /// Return the number of tiles in the cache
  unsigned int getNumElements() { return tileList.size(); }


  /// Return the number of MB stored
  float getMemorySize() { return (float) ( currentSize / 1024000.0 ); }


  /// Get a tile from the cache
  /** 
   *  @param f filename
   *  @param r resolution number
   *  @param t tile number
   *  @param h horizontal sequence number
   *  @param v vertical sequence number
   *  @param c compression type
   *  @param q compression quality
   *  @return pointer to data or NULL on error
   */
  RawTile* getTile( std::string f, int r, int t, int h, int v, CompressionType c, int q ) {

    if( maxSize == 0 ) return NULL;

    std::string key = this->getIndex( f, r, t, h, v, c, q );

    TileMap::iterator miter = tileMap.find( key );
    if( miter == tileMap.end() ) return NULL;
    this->_touch( key );

    return &(miter->second->second);
  }


  /// Create a hash index
  /** 
   *  @param f filename
   *  @param r resolution number
   *  @param t tile number
   *  @param h horizontal sequence number
   *  @param v vertical sequence number
   *  @param c compression type
   *  @param q compression quality
   *  @return string
   */
  std::string getIndex( std::string f, int r, int t, int h, int v, CompressionType c, int q ) {
    char tmp[1024];
    snprintf( tmp, 1024, "%s:%d:%d:%d:%d:%d:%d", f.c_str(), r, t, h, v, c, q );
    return std::string( tmp );
  }



};



#endif
