/*
    IIP Generic Output Writer Classes

    Copyright (C) 2006-2022 Ruven Pillay.

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
#include <cstring>


/// Virtual base class for various writers
class Writer {

 public:

  virtual ~Writer() = 0;

  /// Write out a binary string
  /** \param msg message string
      \param len message string length
  */
  virtual int putStr( const char* msg, int len ) = 0;

  /// Write out a string
  /** \param msg message string */
  virtual int putS( const char* msg ) = 0;

  /// Write out a string
  /** \param msg message string */
  virtual int printf( const char* msg ) = 0;

  /// Flush the output buffer
  virtual int flush() = 0;

};



/// FCGI Writer Class
class FCGIWriter {

 private:

  
  FCGX_Stream *out;
  static const unsigned int bufsize = 65536;

  /// Add the message to our buffer
  void cpy2buf( const char* msg, size_t len ){
    if( sz+len > bufsize ) buffer = (char*) realloc( buffer, sz+len );
    if( buffer ){
      memcpy( &buffer[sz], msg, len );
      sz += len;
    }
  };


 public:

  char* buffer;
  size_t sz;

  /// Constructor
  FCGIWriter( FCGX_Stream* o ){
    out = o;
    buffer = (char*) malloc(bufsize);
    sz = 0;
  };

  /// Destructor
  ~FCGIWriter(){ if(buffer) free(buffer); };

  int putStr( const char* msg, int len ){
    cpy2buf( msg, len );
    return FCGX_PutStr( msg, len, out );
  };
  int putS( const char* msg ){
    int len = (int) strlen( msg );
    cpy2buf( msg, len );
    if( FCGX_PutStr( msg, len, out ) != len ) return -1;
    return len;
  }
  int printf( const char* msg ){
    cpy2buf( msg, strlen(msg) );
    return FCGX_FPrintF( out, msg );
  };
  int flush(){
    return FCGX_FFlush( out );
  };

};



/// File Writer Class
class FileWriter {

 private:

  FILE* out;

 public:

  FileWriter( FILE* o ){ out = o; };

  int putStr( const char* msg, int len ){
    return fwrite( (void*) msg, sizeof(char), len, out );
  };
  int putS( const char* msg ){
    return fputs( msg, out );
  }
  int printf( const char* msg ){
    return fprintf( out, "%s", msg );
  };
  int flush(){
    return fflush( out );
  };

};
  


#endif
