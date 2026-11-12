// Simple Wrapper to libMemcached

/*  IIP Image Server

    Copyright (C) 2010-2026 Ruven Pillay.

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

#ifndef _MEMCACHED_H
#define _MEMCACHED_H

#include <string>
#include <cstring>
#include <cstdlib>

#include "SHA256.h"

#ifndef VERSION
#define VERSION "999"
#endif

// Need to undefine _WIN32 on Windows to avoid compilation problems with winsock2.h and ws2def.h
// includes when using libmemcached-awesome                                                                           
#ifdef WIN32                                                                                                                                                                    
#undef _WIN32                                                                                                                                                                   
#endif  
#include <libmemcached/memcached.h>

#ifdef LIBMEMCACHED_VERSION_STRING
typedef memcached_return memcached_return_t;
#endif

/// Cache to store raw tile data

class Memcache {


 private:

  /// Memcached structure for writes (quiet / fire-and-forget)
  memcached_st *_memc;

  /// Separate Memcached structure for reads, so quiet write commands never
  /// corrupt the response state used by memcached_get()
  memcached_st *_memc_get;

  /// Memcached return value
  memcached_return_t _rc;

  /// Memcached servers
  memcached_server_st *_servers;

  /// Cache expiry set to 1 hour
  time_t _timeout;
 
  /// Length of data returned
  size_t _length;

  /// Flag whether we are connected
  bool _connected;

  /// Maximum size in bytes accepted by our memcached servers (default 1MB)
  size_t _max_item_size;

  /// Create Memcached key using a sha256 of the request URL
  /** @param request URL of request
   *  @return hashed key for use in Memcached
   */
  static std::string cacheKey( const std::string& request ){
    return std::string("iipsrv::") + VERSION + "::" + sha256::hex( request );
  }

  /// Callback for memcached_stat_execute(): capture the `item_size_max` limit
  /// from each server's `stats settings`, keeping the minimum across the pool
  static memcached_return_t maxItemSizeCallback( const memcached_instance_st*,
                                                 const char* key, size_t key_length,
                                                 const char* value, size_t value_length,
                                                 void* context ){
    static const char target[] = "item_size_max";
    if( key_length == sizeof(target) - 1 && std::memcmp( key, target, key_length ) == 0 ){
      size_t* minimum = static_cast<size_t*>( context );
      unsigned long long n = std::strtoull( value, NULL, 10 );
      if( n > 0 && n < *minimum ) *minimum = static_cast<size_t>( n );
    }
    return MEMCACHED_SUCCESS;
  }


 public:

  /// Constructor
  /** @param servernames list of memcached servers
      @param timeout memcached timeout - defaults to 1 hour (3600 seconds)
  */
  Memcache( const std::string& servernames, unsigned int timeout = 3600 ) {

    _length = 0;
    _connected = false;
    _max_item_size = 1024 * 1024;
    _memc = NULL;
    _memc_get = NULL;
    _servers = NULL;

    // Set our timeout
    _timeout =  timeout;

    // Create our memcached object for writes
    _memc = memcached_create(NULL);
    if( _memc == NULL ){
      _rc = MEMCACHED_MEMORY_ALLOCATION_FAILURE;
      return;
    }

    // Create a separate memcached object for reads, since the quiet write
    // commands issued with MEMCACHED_BEHAVIOR_NOREPLY would otherwise break
    // memcached_get() on the same connection
    _memc_get = memcached_create(NULL);
    if( _memc_get == NULL ){
      _rc = MEMCACHED_MEMORY_ALLOCATION_FAILURE;
      memcached_free( _memc );
      _memc = NULL;
      return;
    }

    // Create a list of servers
    _servers = memcached_servers_parse( servernames.c_str() );
    if( _servers == NULL ){
      _rc = MEMCACHED_INVALID_ARGUMENTS;
      return;
    }

    // Try to set some memcached behaviour settings for performance. For example,
    // using the binary protocol and non-blocking IO. Writes are fire-and-forget
    // (NOREPLY), so cache writes never block the response stream
    _rc =  memcached_behavior_set( _memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1 );
    _rc =  memcached_behavior_set( _memc, MEMCACHED_BEHAVIOR_NO_BLOCK, 1 );
    _rc =  memcached_behavior_set( _memc, MEMCACHED_BEHAVIOR_TCP_NODELAY, 1 );
    _rc =  memcached_behavior_set( _memc, MEMCACHED_BEHAVIOR_NOREPLY, 1 );

    // Reads must wait for their response, so no NOREPLY on the read connection
    _rc =  memcached_behavior_set( _memc_get, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1 );
    _rc =  memcached_behavior_set( _memc_get, MEMCACHED_BEHAVIOR_TCP_NODELAY, 1 );

    // Connect to the servers
    _rc = memcached_server_push( _memc, _servers );
    if( _rc != MEMCACHED_SUCCESS ) return;
    _rc = memcached_server_push( _memc_get, _servers );
    if( _rc != MEMCACHED_SUCCESS ) return;

    if( memcached_server_count( _memc ) == 0 ){
      _rc = MEMCACHED_NO_SERVERS;
      return;
    }

    _rc = memcached_version( _memc_get );
    _connected = ( _rc == MEMCACHED_SUCCESS );

    // Query `stats settings` for the lowest maximum item size accepted by the pool of servers
    if( _connected ){
      _rc = memcached_stat_execute( _memc_get, "settings", maxItemSizeCallback, &_max_item_size );
    }
  };


  /// Destructor
  ~Memcache() {
    // Disconnect from our servers and free our memcached structure
    if( _servers ) memcached_server_free(_servers); 
    if( _memc ) memcached_free(_memc);
    if( _memc_get ) memcached_free(_memc_get);
  }


  /// Insert data into our cache
  /** @param key key used for cache
      @param data pointer to the data to be stored
      @param length length of data to be stored
  */
  void store( const std::string& key, void* data, unsigned int length ){

    if( !_connected ) return;

    // Skip if the data would exceed the maximum size accepted by our servers
    if( length > _max_item_size ) return;
 
    std::string k = cacheKey( key );
    _rc = memcached_set( _memc, k.c_str(), k.length(),
			 (char*) data, length,
			 _timeout, 0 );
  }


  /// Retrieve data from our cache
  /** @param key key for cache data
      @return pointer to data
  */
  char* retrieve( const std::string& key ){

    if( !_connected ) return NULL;

    uint32_t flags;
    std::string k = cacheKey( key );
    return memcached_get( _memc_get, k.c_str(), k.length(), &_length, &flags, &_rc );
  }


  /// Get error string
  const char* error(){
    if( !_memc ) return "Memcached initialization failed";
    if( _rc != MEMCACHED_SUCCESS && memcached_last_error( _memc ) != MEMCACHED_SUCCESS ){
      const char* msg = memcached_last_error_message( _memc );
      if( msg != NULL ) return msg;
    }
    return memcached_strerror( _memc, _rc );
  };


  /// Return the number of bytes in the result
  unsigned int length(){ return _length; };


  /// Tell us whether we are connected to any memcached servers
  bool connected(){ return _connected; };


  /// Return the maximum value size (bytes) accepted by our memcached servers
  size_t maxItemSize(){ return _max_item_size; };


};



#endif
