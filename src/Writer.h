/*
    IIP Generic Output Writer Classes

    Copyright (C) 2006-2010 Ruven Pillay.

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


#ifndef _WRITER_H
#define _WRITER_H


#include <fcgiapp.h>
#include <cstdio>


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

 public:

  FCGIWriter( FCGX_Stream* o ){ out = o; };

  int putStr( const char* msg, int len ){
    return FCGX_PutStr( msg, len, out );
  };
  int putS( const char* msg ){
    return FCGX_PutS( msg, out );
  }
  int printf( const char* msg ){
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
