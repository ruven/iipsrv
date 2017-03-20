// Simple Wrapper to libMemcached

/*  IIP Image Server

    Copyright (C) 2010-2013 Ruven Pillay.

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
#include <libmemcached/memcached.h>

#ifdef LIBMEMCACHED_VERSION_STRING
typedef memcached_return memcached_return_t;
#endif

/// Cache to store raw tile data

class Memcache {


 private:

  /// Memcached structure
  memcached_st *_memc;

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


 public:

  /// Constructor
  /** @param servernames list of memcached servers
      @param timeout memcached timeout - defaults to 1 hour (3600 seconds)
  */
  Memcache( const std::string& servernames = "localhost", unsigned int timeout = 3600 ) {

    _length = 0;

    // Set our timeout
    _timeout =  timeout;

    // Create our memcached object
    _memc = memcached_create(NULL);

    // Create a list of servers
    _servers = memcached_servers_parse( servernames.c_str() );

    // Try to set some memcached behaviour settings for performance. For example,
    // using the binary protocol, non-blocking IO, and no reply for add commands
    _rc =  memcached_behavior_set( _memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1 );
    _rc =  memcached_behavior_set( _memc, MEMCACHED_BEHAVIOR_NO_BLOCK, 1 );
    _rc =  memcached_behavior_set( _memc, MEMCACHED_BEHAVIOR_TCP_NODELAY, 1 );
    _rc =  memcached_behavior_set( _memc, MEMCACHED_BEHAVIOR_NOREPLY, 1 );

    // Connect to the servers
    _rc = memcached_server_push( _memc, _servers );
    if(_rc == MEMCACHED_SUCCESS ) _connected = true;
    else _connected = false;

    if( memcached_server_count(_memc) > 0 ) _connected = true;
    else _connected = false;
  };


  /// Destructor
  ~Memcache() {
    // Disconnect from our servers and free our memcached structure
    if( _servers ) memcached_server_free(_servers); 
    if( _memc ) memcached_free(_memc);
  }


  /// Insert data into our cache
  /** @param key key used for cache
      @param data pointer to the data to be stored
      @param length length of data to be stored
  */
  void store( const std::string& key, void* data, unsigned int length ){

    if( !_connected ) return;
 
    std::string k = "iipsrv::" + key;
    _rc = memcached_set( _memc, k.c_str(), k.length(),
                        (char*) data, length,
                        _timeout, 0 );
  }

  /// Insert binary data into our cache
  /** @param key key used for cache
      @param data pointer to the data to be stored
      @param length length of data to be stored
  */
  void storeblob( const std::string key, unsigned char* buff, unsigned long bufflen ){

    if( !_connected ) return;
 
    std::string k = "iipsrv::" + key;
    _rc = memcached_set( _memc, k.c_str(), k.length(),
                        reinterpret_cast<const char*>(buff), bufflen,
                        _timeout, 0 );
  }


  /// Retrieve data from our cache
  /** @param key key for cache data
      @return pointer to data
  */
  void* retrieve( const std::string& key ){

    if( !_connected ) return NULL;

    uint32_t flags;
    std::string k = "iipsrv::" + key;
    return memcached_get( _memc, k.c_str(), k.length(), &_length, &flags, &_rc );
  }

  /// Retrieve data from our cache
  /** @param key key for cache data
      @return pointer to data
  */
  unsigned char* retrieveblob( std::string key, unsigned long &bufflen ){

    if( !_connected ) return NULL;

    uint32_t flags;
    std::string k = "iipsrv::" + key;
    unsigned char* blob = reinterpret_cast<unsigned char*>(memcached_get( _memc, k.c_str(), k.length(), &_length, &flags, &_rc ));
    bufflen = _length;
    return blob;
  }

  /// Get error string
  const char* error(){
    return memcached_strerror( _memc, _rc );
  };


  /// Return the number of bytes in the result
  unsigned int length(){ return _length; };


  /// Tell us whether we are connected to any memcached servers
  bool connected(){ return _connected; };


};



#endif
