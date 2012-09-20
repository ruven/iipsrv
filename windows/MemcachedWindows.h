// Simple Wrapper to MemCacheClient

/*  IIP Image Server

	Copyright (C) 2010 Ruven Pillay, Michal Becak.

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



#ifndef _MEMCACHED_WINDOWS_H
#define _MEMCACHED_WINDOWS_H

#include <string>
#include <sstream>
#include <vector>
#include "MemCacheClient.h"

#ifdef WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

/// Cache to store raw tile data

class Memcache {


 private:

  /// Memcached structure
  MemCacheClient *_memc;

  /// Memcached return value
  MCResult _rc;

  /// Memcached servers
  std::vector<std::string> *_servers;

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

	// Create our memcached object
	_memc = new MemCacheClient;

    // Set our timeout
    _timeout =  timeout;
	_memc->SetTimeout(timeout);

    // Create a list of servers
	_servers = new std::vector<std::string>();
		//split string on ',' and save chunks to vector
	std::stringstream ss(servernames);
    std::string serverItem;
    while(std::getline(ss, serverItem, ',')) {
		if (serverItem == "localhost")//MemCacheClient needs ip address, so we map localhost to 127.0.0.1
			serverItem = "127.0.0.1";
        _servers->push_back(serverItem);
    }

    // Connect to the servers
    _connected = false;
	for (int i = 0; i < _servers->size(); i++) {
		_connected = _connected || _memc->AddServer(_servers->at(i).c_str());
	}
  };


  /// Destructor
  ~Memcache() {
    // Disconnect from our servers and free our memcached structure
	if( _servers ) {
		delete _servers;
		_servers = NULL;
	}
    if( _memc ) _memc->~MemCacheClient();
  }


  /// Insert data into our cache
  /** @param key key used for cache
      @param data pointer to the data to be stored
      @param length length of data to be stored
  */
  void store( const std::string& key, void* data, unsigned int length ){

    if( !_connected ) return;

    MemCacheClient::MemRequest req;
	req.mKey = "iipsrv::" + key;
	req.mData.WriteBytes(data, length);
	_memc->Set(req);
	_rc = req.mResult;
  }


  /// Retrieve data from our cache
  /** @param key key for cache data
      @return pointer to data
  */
  char* retrieve( const std::string& key ){

    if( !_connected ) return NULL;

	MemCacheClient::MemRequest req;
	req.mKey = "iipsrv::" + key;
	int result = _memc->Get(req);
	if (result == 0)//if 0 items retrieved
		return NULL;
	_rc = req.mResult;
	_length = req.mData.GetReadSize();
	char * returnData = new char[_length];
	req.mData.ReadBytes(returnData,_length);
	return returnData;
  }


  /// Get error string
  const char* error(){
	  return _memc->ConvertResult(_rc);
  };


  /// Return the number of bytes in the result
  unsigned int length(){ return _length; };


  /// Tell us whether we are connected to any memcached servers
  bool connected(){ return _connected; };

};

#endif
