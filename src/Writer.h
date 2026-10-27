/*
    IIP Generic Output Writer Classes

    Copyright (C) 2006-2026 Ruven Pillay.

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


#ifndef _WRITER_H
#define _WRITER_H

// Windows vcpkg requires prefix for include
#ifdef WIN32
#include <fastcgi/fcgiapp.h>
#else
#include <fcgiapp.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>


/// Virtual base class for various writers
class Writer {

 protected:
  bool error;

 public:

  Writer() : error(false) {};
  virtual ~Writer() {};

  /// Has an error occurred
  /** @return error status */
  bool getError() const { return error; };

  /// Write out a binary string
  /** @param msg message string
      @param len message string length
   */
  virtual int putStr( const char* msg, int len ) = 0;

  /// Write out a string using puts()
  /** @param msg message string
      @return number of bytes written
   */
  virtual int putS( const char* msg ) = 0;

  /// Write out a string using printf()
  /** @param msg message string
      @return number of bytes written
   */
  virtual int printf( const char* msg ) = 0;

  /// Flush the output buffer
  /** @return 0 = success, 1 = fail */
  virtual int flush() = 0;

};



/// FCGI Writer Class
class FCGIWriter : public Writer {

 private:

  /// FCGI stream output
  FCGX_Stream *out;

  /// Buffer size
  size_t bufsize;

  /// Add the message to our buffer
  /** @param msg message string
      @param len message length in bytes
   */
  void cpy2buf( const char* msg, size_t len ){

    // Abort if an error has occurred or if no buffering has been disabled
    if( error || bufsize==0 ) return;

    // Abort buffer that cannot be handled by Memcached
    if( sz+len > bufsize ){
      error = true;
      return;
    }
    if( buffer ){
      memcpy( &buffer[sz], msg, len );
      sz += len;
    }
  };


 public:

  char* buffer;        ///< Buffer for network caches
  size_t sz;           ///< Size of buffer

  /// Constructor
  /** @param o FCGI stream pointer
   *  @param bs max internal buffer size in bytes (default 1MB, 0 disables buffering)
   */
  FCGIWriter( FCGX_Stream* o, size_t bs = 1048576 ) : Writer(), out(o), bufsize(bs), buffer(NULL), sz(0) {
    if( bufsize > 0 ) buffer = (char*) malloc(bufsize);
  };

  /// Destructor
  ~FCGIWriter(){
    if( buffer ) free(buffer);
    sz = 0;
  };

  /// Add the message to our buffer
  /** @param msg message string
      @param len message length in bytes
      @return number of bytes written
   */
  int putStr( const char* msg, int len ){
    cpy2buf( msg, len );
    return FCGX_PutStr( msg, len, out );
  };

  /// Write out a string using puts()
  /** @param msg message string
      @return number of bytes written
   */
  int putS( const char* msg ){
    int len = (int) strlen( msg );
    cpy2buf( msg, len );
    if( FCGX_PutStr( msg, len, out ) != len ) return -1;
    return len;
  }

  /// Write out a string using printf()
  /** @param msg message string
      @return number of bytes written
   */
  int printf( const char* msg ){
    cpy2buf( msg, strlen(msg) );
    return FCGX_FPrintF( out, msg );
  };

  /// Flush the output buffer
  /** @return 0 = success, 1 = fail */
  int flush(){
    return FCGX_FFlush( out );
  };

};



/// File Writer Class
class FileWriter : public Writer {

 private:

  /// File output pointer
  FILE* out;

 public:

  /// Constructor
  /** @param o FILE pointer */
  FileWriter( FILE* o ){ out = o; };

  /// Add the message to our buffer
  /** @param msg message string
      @param len message length in bytes
      @return number of bytes written
   */
  int putStr( const char* msg, int len ){
    return fwrite( (const void*) msg, sizeof(char), len, out );
  };

  /// Write out a string using puts()
  /** @param msg message string
      @return number of bytes written
   */
  int putS( const char* msg ){
    return fputs( msg, out );
  }

  /// Write out a string using printf()
  /** @param msg message string
      @return number of bytes written
   */
  int printf( const char* msg ){
    return fprintf( out, "%s", msg );
  };

  /// Flush the output buffer
  /** @return 0 = success, 1 = fail */
  int flush(){
    return fflush( out );
  };

};
  

#endif
