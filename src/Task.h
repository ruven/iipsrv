/*
    IIP Generic Task Class

    Copyright (C) 2006-2007 Ruven Pillay.

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


#ifndef _TASK_H
#define _TASK_H



#include <string>
#include <fstream>
#include "IIPImage.h"
#include "IIPResponse.h"
#include "JPEGCompressor.h"
#include "View.h"
#include "TileManager.h"
#include "Timer.h"
#include "Writer.h"
#include "Cache.h"


// Use the hashmap extensions if we are using >= gcc 3.1
#ifdef __GNUC__

#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 1) || (__GNUC__ >= 4)
#define USE_HASHMAP 1
#endif

// And the high performance memory pool allocator if >= gcc 3.4
#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || (__GNUC__ >= 4)
#define USE_POOL_ALLOCATOR 1
#endif

#endif



#ifdef USE_HASHMAP
#include <ext/hash_map>

#ifdef USE_POOL_ALLOCATOR
#include <ext/pool_allocator.h>
typedef __gnu_cxx::hash_map < const std::string, IIPImage,
			      __gnu_cxx::hash< const std::string >,
			      std::equal_to< const std::string >,
			      __gnu_cxx::__pool_alloc< std::pair<const std::string,IIPImage> >
			      > imageCacheMapType;
#else
typedef __gnu_cxx::hash_map <const std::string,IIPImage> imageCacheMapType;
#endif

#else
typedef std::map<const std::string,IIPImage> imageCacheMapType;
#endif




/// Structure to hold our session data
struct Session {
  IIPImage **image;
  JPEGCompressor* jpeg;
  View* view;
  IIPResponse* response;
  int loglevel;
  std::ofstream* logfile;

  imageCacheMapType *imageCache;
  Cache* tileCache;

#ifdef DEBUG
  FileWriter* out;
#else
  FCGIWriter* out;
#endif

};




/// Generic class to encapsulate various commands
class Task {

 protected:

  /// Timer for each task
  Timer command_timer;

  /// Pointer to our session data
  Session* session;

  /// Argument supplied to the task
  std::string argument;


 public:

  /// Virtual destructor
  virtual ~Task() {;};   

  /// Main public function
  virtual void run( Session* session, const std::string& argument ) {;};

  /// Factory function
  /** @param type command type */
  static Task* factory( const std::string& type );


  /// Check image
  void checkImage();

};




/// OBJ commands
class OBJ : public Task {

 public:

  void run( Session* session, const std::string& argument );

  void iip();
  void iip_server();
  void max_size();
  void resolution_number();
  void colorspace( std::string arg );
  void tile_size();
  void bits_per_channel();
  void horizontal_views();
  void vertical_views();
  void metadata( std::string field );

};


/// JPEG Quality Command
class QLT : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// SDS Command
class SDS : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// Contrast Command
class CNT : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// CVT Width Command
class WID : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// CVT Height Command
class HEI : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// CVT Region Command
class RGN : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// FIF Command
class FIF : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// JPEG Tile Command
class JTL : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// JPEG Tile Sequence Command
class JTLS : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// Tile Command
class TIL : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// CVT Command
class CVT : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// ICC Profile Command
class ICC : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// Shading Command
class SHD : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// Zoomify Request Command
class Zoomify : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// SPECTRA Request Command
class SPECTRA : public Task {
 public:
  void run( Session* session, const std::string& argument );
};


/// Quality Layers Command
class LYR : public Task {
 public:
  void run( Session* session, const std::string& argument );
};

#endif
