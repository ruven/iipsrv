/*
    Basic Header-Only Logging Class

    Copyright (C) 2019-2023 Ruven Pillay

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


#ifndef _LOGGER_H
#define _LOGGER_H

#include <ostream>
#include <fstream>
#include <streambuf>
#include <string>



#ifdef HAVE_SYSLOG_H

#include <syslog.h>

/// Syslog stream class
class SyslogStream : public std::streambuf {

 private:
  std::string _buf;
  int _level;


 public:

  /// Constructor - set our default level to debug
  SyslogStream() : _level( LOG_DEBUG ) { };

  /// Open a syslog connection
  void open(){
    openlog( "iipsrv", LOG_NDELAY | LOG_PID, LOG_USER );
  }

  /// Close syslog connection
  void close(){ closelog(); };

  /// Override streambuf sync() function
  int sync(){
    if (_buf.size()) {
      syslog( _level, "%s",  _buf.c_str() );
      _buf.erase();
    }
    return 0;
  }

  /// Override streambufer overflow() function
  int_type overflow( int_type c ){
    if( c == traits_type::eof() ) sync();
    else _buf += static_cast<char>(c);
    return c;
  }

};

#endif


/// Logger class - handles ofstreams and syslog
class Logger : public std::ostream {

 private:

#ifdef HAVE_SYSLOG_H
  /// Syslog Stream buffer
  SyslogStream _syslogStream;
#endif

  /// File stream
  std::ofstream _fstream;

  /// Supported output types
  enum Type {
#ifdef HAVE_SYSLOG_H
    SYSLOG,
#endif
    FILE
  };
  Type _type;


 public:

  /// Constructor - derived from std::ostream
  Logger() : std::ostream( NULL ) {};


  /// Destructor - close our logging stream
  ~Logger() { this->close(); };


  /// Open our logging output
  /** @param file input file name
   */
  void open( const std::string& file ){

#ifdef HAVE_SYSLOG_H
    // Open a syslog connection - assign syslog stream to our stream buffer
    if( file == "syslog" ){
      _type = SYSLOG;
      this->rdbuf( &_syslogStream );
      _syslogStream.open();
    }
    // Create an output file stream and assign it to our stream buffer
    else{
#endif
      _type = FILE;
      _fstream.open( file.c_str(), ios_base::app );
      std::streambuf *buffer = _fstream.rdbuf();
      this->rdbuf( buffer );
#ifdef HAVE_SYSLOG_H
    }
#endif
  };


  /// Close depending on type
  void close(){
    switch( _type ){
#ifdef HAVE_SYSLOG_H
      case SYSLOG:
        _syslogStream.close();
        break;
#endif
      default:
	_fstream.close();
    }
  };


  /// Provide a list of available logging types
  /** @return std::string list of available logging types
   */
  std::string types(){
    std::string types = "file";
#ifdef HAVE_SYSLOG_H
    types += ", syslog";
#endif
    return types;
  };

};

#endif
