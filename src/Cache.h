// Tile Cache Class

/*  IIP Image Server

    Copyright (C) 2005-2026 Ruven Pillay.

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


#include <unordered_map>
#include <list>
#include <string>
#include <cstddef>
#include <functional>
#include "RawTile.h"



/// LRU Cache to store raw tile data

class Cache {


 private:

  /// Cache key for RawTile storage and lookup
  /**
   * Allocation-free and trivially copyable. 'f' references the filename owned by
   * the cached RawTile itself (stored keys) or by the caller (transient lookup keys).
   * Safe because map entries and list nodes are always erased together and std::list
   * element addresses are stable.
   */
  struct TileKey {
    const std::string* f;   // image path (non-owning)
    int r;                  // resolution
    int t;                  // tile number
    int h;                  // hSequence
    int v;                  // vSequence
    ImageEncoding c;        // compression type
    int q;                  // quality

    bool operator==(const TileKey& k) const {
      return t == k.t && r == k.r && q == k.q && c == k.c &&
	h == k.h && v == k.v && (f == k.f || (f && k.f && *f == *k.f));
    }
  };

  /// Portable C++11 hash combiner (based on Boost's golden-ratio mixer)
  static void hash_combine(std::size_t& seed, std::size_t val) {
    seed ^= val + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  /// Hash functor for TileKey
  struct TileKeyHash {
    std::size_t operator()(const TileKey& k) const {
      std::size_t seed = k.f ? std::hash<std::string>()(*k.f) : 0;
      Cache::hash_combine(seed, static_cast<std::size_t>(k.t));
      Cache::hash_combine(seed, static_cast<std::size_t>(k.r));
      Cache::hash_combine(seed, static_cast<std::size_t>(k.q));
      Cache::hash_combine(seed, static_cast<std::size_t>(k.c));
      Cache::hash_combine(seed, static_cast<std::size_t>(k.h));
      Cache::hash_combine(seed, static_cast<std::size_t>(k.v));
      return seed;
    }
  };

  /// Basic object storage size
  int tileSize;

  /// Max memory size in bytes
  unsigned long maxSize;

  /// Current memory running total
  unsigned long currentSize;

  /// Main cache storage typedef
  typedef std::list < RawTile > TileList;

  /// Main cache list iterator typedef
  typedef TileList::iterator List_Iter;

  /// Index typedef
  typedef std::unordered_map < TileKey, List_Iter, TileKeyHash > TileMap;


  /// Main cache storage object
  TileList tileList;

  /// Main Cache storage index object
  TileMap tileMap;


  /// Internal touch function - moves an existing cache entry to the head of the list
  /** @param miter TileMap iterator pointing to the entry to touch */
  void _touch( TileMap::iterator miter ) {
    tileList.splice( tileList.begin(), tileList, miter->second );
  }


  /// Internal remove function - removes an entry from both map and list, safely maintaining lifetime order
  /**  @param miter TileMap iterator pointing to the entry to remove */
  void _remove( TileMap::iterator miter ) {
    List_Iter liter = miter->second;
    unsigned long entrySize = liter->capacity + liter->filename.capacity()*sizeof(char) + tileSize;
    currentSize = ( currentSize > entrySize ) ? ( currentSize - entrySize ) : 0;
    tileMap.erase( miter );
    tileList.erase( liter );
  }



 public:

  /// Constructor
  /** @param max Maximum cache size in MB */
  Cache( const float max ) {
    maxSize = (unsigned long)(max*1024000) ; currentSize = 0;
    tileSize = sizeof( RawTile ) + 2 * sizeof(void*) +
               sizeof( std::pair<const TileKey, List_Iter> ) + sizeof(void*);
  };


  /// Destructor
  ~Cache() {
    clear();
  }


  /// Empty the cache
  void clear() {
    tileList.clear();
    tileMap.clear();
    currentSize = 0;
  }


  /// Insert a tile
  /** @param r Tile to be inserted */
  void insert( const RawTile& r ) {

    if( maxSize == 0 ) return;

    // Reject tiles larger than the cache capacity to prevent evicting the entire cache
    if( r.capacity + r.filename.capacity() * sizeof(char) + tileSize > maxSize ) return;

    TileKey lookupKey{ &r.filename, r.resolution, r.tileNum,
                       r.hSequence, r.vSequence, r.compressionType, r.quality };

    TileMap::iterator miter = tileMap.find( lookupKey );

    // Check whether this tile exists in our cache
    if( miter != tileMap.end() ){
      // Check the timestamp and delete if necessary
      if( miter->second->timestamp < r.timestamp ){
	this->_remove( miter );
      }
      // If this index already exists and it is up to date, move it to the front of the list and return
      else{
	this->_touch( miter );
	return;
      }
    }

    // Insert tile at the front of the list
    tileList.push_front( r );
    List_Iter liter = tileList.begin();

    // Store key in map referencing the stable filename inside the list node
    TileKey storedKey{ &(liter->filename), liter->resolution, liter->tileNum,
                       liter->hSequence, liter->vSequence, liter->compressionType, liter->quality };
    const std::pair<TileMap::iterator,bool> inserted = tileMap.emplace( storedKey, liter );

    // Check for duplicate key in map and remove from list if found (should never happen)
    if( !inserted.second ){
      tileList.erase( liter );
      return;
    }

    // Update running total memory usage
    currentSize += ( liter->capacity + liter->filename.capacity()*sizeof(char) + tileSize );

    // Evict least recently used elements if exceeding max_size
    while( currentSize > maxSize && !tileList.empty() ){
      RawTile& back = tileList.back();
      TileKey evictKey{ &(back.filename), back.resolution, back.tileNum,
                        back.hSequence, back.vSequence, back.compressionType, back.quality };
      TileMap::iterator emiter = tileMap.find( evictKey );
      if( emiter != tileMap.end() ){
	this->_remove( emiter );
      }
      else{
	unsigned long entrySize = back.capacity + back.filename.capacity()*sizeof(char) + tileSize;
	currentSize = ( currentSize > entrySize ) ? ( currentSize - entrySize ) : 0;
	tileList.pop_back();
      }
    }

  }


  /// Return the number of tiles in the cache
  /** @return number of tiles */
  unsigned int getNumElements() const { return tileList.size(); }


  /// Return the number of MB stored
  /** @return approximate number of MB stored */
  float getMemorySize() const { return (float) ( currentSize / 1024000.0 ); }


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
  RawTile* getTile( const std::string& f, int r, int t, int h, int v, ImageEncoding c, int q ) {

    if( maxSize == 0 ) return NULL;

    TileKey key{ &f, r, t, h, v, c, q };

    TileMap::iterator miter = tileMap.find( key );
    if( miter == tileMap.end() ) return NULL;
    this->_touch( miter );

    return &(*miter->second);
  }

};


#endif
