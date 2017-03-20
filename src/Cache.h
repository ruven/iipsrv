// Tile Cache Class

/*  IIP Image Server

    Copyright (C) 2005-2014 Ruven Pillay.
    Based on an LRU cache by Patrick Audley <http://blackcat.ca/lifeline/query.php/tag=LRU_CACHE>
    Copyright (C) 2004 by Patrick Audley

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



#ifndef _CACHE_H
#define _CACHE_H


// Fix missing snprintf in Windows
#if _MSC_VER
#define snprintf _snprintf
#endif

// Test for available map types. Try to use an efficient hashed map type if possible
// and define this as HASHMAP, which we can then use elsewhere.
#if defined(HAVE_UNORDERED_MAP)
#include <unordered_map>
#define HASHMAP std::unordered_map

#elif defined(HAVE_TR1_UNORDERED_MAP)
#include <tr1/unordered_map>
#define HASHMAP std::tr1::unordered_map

// Use the gcc hash_map extension if we have it
#elif defined(HAVE_EXT_HASH_MAP)
#include <ext/hash_map>
#define HASHMAP __gnu_cxx::hash_map


/* Explicit template specialization of hash of a string class,
   which just uses the internal char* representation as a wrapper.
   Required for older versions of gcc as hashing on a string is
   not supported.
 */
namespace __gnu_cxx {
  template <>
    struct hash<std::string> {
      size_t operator() (const std::string& x) const {
	return hash<const char*>()(x.c_str());
      }
    };
}

// If no hash type available, just use map
#else
#include <map>
#define HASHMAP std::map

#endif // End of #if defined



// Try to use the gcc high performance memory pool allocator (available in gcc >= 3.4)
#ifdef HAVE_EXT_POOL_ALLOCATOR
#include <ext/pool_allocator.h>
#endif


#include <iostream>
#include <list>
#include <string>
#include "RawTile.h"
#include "Timer.h"
#include "IIPImage.h"

#ifdef HAVE_MEMCACHED
#ifdef WIN32
#include "../windows/MemcachedWindows.h"
#else
#include "Memcached.h"
#endif
#endif

using namespace std;

/// Cache to store raw tile data

class Cache {


 private:


  /// Basic object storage size
  int tileSize;

  /// Max memory size in bytes
  unsigned long maxSize;

  /// Current memory running total
  unsigned long currentSize;

#ifdef HAVE_MEMCACHED
  /// Memcache rather than per-process cache
  Memcache* memcached;
#endif

  /// Main cache storage typedef
#ifdef HAVE_EXT_POOL_ALLOCATOR
  typedef std::list < std::pair<const std::string,RawTile>,
    __gnu_cxx::__pool_alloc< std::pair<const std::string,RawTile> > > TileList;
#else
  typedef std::list < std::pair<const std::string,RawTile> > TileList;
#endif

  /// Main cache list iterator typedef
  typedef std::list < std::pair<const std::string,RawTile> >::iterator List_Iter;

  /// Index typedef
#ifdef HAVE_EXT_POOL_ALLOCATOR
  typedef HASHMAP < std::string, List_Iter,
    __gnu_cxx::hash< const std::string >,
    std::equal_to< const std::string >,
    __gnu_cxx::__pool_alloc< std::pair<const std::string, List_Iter> >
    > TileMap;
#else
  typedef HASHMAP < std::string,List_Iter > TileMap;
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
   *  @warning miter is no longer usable after being passed to this function.
   */
  void _remove( const TileMap::iterator &miter ) {
    // Reduce our current size counter
    currentSize -= ( (miter->second->second).dataLength +
		     ( (miter->second->second).filename.capacity() + (miter->second->first).capacity() )*sizeof(char) +
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

  void initCache(float max) {
    maxSize = (unsigned long)(max*1024000) ; currentSize = 0;
    // 64 chars added at the end represents an average string length
    tileSize = sizeof( RawTile ) + sizeof( std::pair<const std::string,RawTile> ) +
      sizeof( std::pair<const std::string, List_Iter> ) + sizeof(char)*64 + sizeof(List_Iter);
  }

 public:


  /// Constructor
  /** @param max Maximum cache size in MB */
  Cache( float max ) {
    initCache(max);
  };

#ifdef HAVE_MEMCACHED
  /// Constructor accepting memcache - used in cases when MAX CACHE is set to 0 (default)
  Cache( float max, Memcache* memcache ) {
    initCache(max);
    memcached = memcache;
  };
#endif

  /// Destructor
  ~Cache() {
    tileList.clear();
    tileMap.clear();
  }


  /// Insert a tile
  /** @param r Tile to be inserted */
  void insert( const RawTile &r ) {

    // generate key into the cache regardless of the cache we're using
    std::string key = this->getKey( r.filename, r.resolution, r.tileNum,
                                    r.hSequence, r.vSequence, r.compressionType, r.quality );

#ifdef HAVE_MEMCACHED
    if ( maxSize <= 0 && ( memcached == NULL || !memcached->connected() ) )
      return;

    if ( maxSize <= 0 ) {
      // USE MEMCACHE SINCE MEMORY CACHE SIZE = 0
      if ( memcached->connected() ) {
        // Timer memcached_timer;
        // memcached_timer.start();

        unsigned char *buffer;
        unsigned long bufferlength;
        buffer = ((RawTile) r).serialize(bufferlength);
        if (buffer != NULL && bufferlength > 0)
          memcached->storeblob( key, buffer, bufferlength );

        // free the memory associated with the serialized representation
        if (buffer != NULL)
          free(buffer);

        // logfile << "TileCache :: stored " << bufferlength << " bytes in key:" << key << endl;
      }
    }

#endif

    // USE IN MEMORY CACHE
    if ( maxSize <= 0)
      return;

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

    // Update our total current size variable. Use the string::capacity function
    // rather than length() as std::string can allocate slightly more than necessary
    // The +1 is for the terminating null byte
    currentSize += (r.dataLength + (r.filename.capacity()+key.capacity())*sizeof(char) + tileSize);

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

    std::string key = this->getKey( f, r, t, h, v, c, q );

#ifdef HAVE_MEMCACHED
    if ( maxSize <= 0 && ( memcached == NULL || !memcached->connected() ) )
      return NULL;

    if ( maxSize <= 0 ) {
      // Check whether this exists in memcached
      unsigned char *buffer = NULL;
      unsigned long bufflen = 0;
      if (  buffer = memcached->retrieveblob( key, bufflen ) ) {
        if ( bufflen > 0 ) {
          // cache hit
          // logfile << "TileCache :: MemCache hit for key:"<< key << endl;

          // create a new empty tile object and attempt to deserialize into it
          RawTile *newTile = new RawTile();
          if ( !newTile->deserialize(buffer, bufflen) ) {
              logfile << "ERROR: TileCache :: !!! UNSUCCESSFUL MemCache deserialization of " << bufflen << " bytes for key:"<< key << endl;
              delete newTile;
              newTile = NULL;
          }
          free(buffer);
          return newTile;
        }
      }
    }

#endif

    // CHECK IN MEMORY CACHE
    if ( maxSize == 0)
      return NULL;

    TileMap::iterator miter = tileMap.find( key );
    if( miter == tileMap.end() ) return NULL;
    this->_touch( key );

    return &(miter->second->second);
  }


  /// Create a hash key for the structure of the RawTile
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
  std::string getKey( std::string f, int r, int t, int h, int v, CompressionType c, int q ) {
    char tmp[1024];
    snprintf( tmp, 1024, "%s:%d:%d:%d:%d:%d:%d", f.c_str(), r, t, h, v, c, q );
    return std::string( tmp );
  }

  /// Cleanup memory in case we're using memcache rather than in memory cache
  void cleanup(RawTile **rawtile) {
#ifdef HAVE_MEMCACHED
    if ( maxSize <= 0 && memcached != NULL && memcached->connected() ) {
      delete *rawtile;
      *rawtile = NULL;
    }
#endif
  }

};

#endif
