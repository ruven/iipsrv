/*
    Simple URL decoder Class

    Copyright (C) 2014-2026 Ruven Pillay.

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


#ifndef _URL_H
#define _URL_H

#include <string>
#include <iterator>
#include <cctype>
#include <cstddef>


/// Simple utility class to decode and filter URLs

class URL{

 private:

  /// URL string
  std::string url;

  /// Warning
  std::string warning_message;

  // Internal utility function to decode hex values
  char hexToChar( char first, char second ) const;

 public:

  /// Constructor
  /** @param s input url string */
  URL( const std::string& s ){ url = s; };

  /// Decode and filter URL
  std::string decode();

  /// String escaping for JSON etc
  std::string escape();

  /// Return any warning message
  const std::string& warning() const { return warning_message; };

};


inline char URL::hexToChar ( char first, char second ) const {
  const unsigned char high = static_cast<unsigned char>(first);
  const unsigned char low = static_cast<unsigned char>(second);

  const int high_digit = (high >= '0' && high <= '9') ? high - '0' :
                         (high >= 'A' && high <= 'F') ? high - 'A' + 10 :
                                                        high - 'a' + 10;
  const int low_digit = (low >= '0' && low <= '9') ? low - '0' :
                        (low >= 'A' && low <= 'F') ? low - 'A' + 10 :
                                                     low - 'a' + 10;

  return static_cast<char>((high_digit << 4) + low_digit);
}


// The argument is a URL path, which may contain spaces or other hex encoded characters.
// So, first decode and filter this path (implementation taken from GNU cgicc: http://www.cgicc.org)
inline std::string URL::decode()
{
  std::string argument;
  std::string::iterator iter;
  char c;

  for(iter = url.begin(); iter != url.end(); ++iter) {
    switch(*iter) {
    case '+':
      argument.append(1,' ');
      break;
    case '%':
      // Don't assume well-formed input
      if( std::distance(iter, url.end()) >= 3 &&
          std::isxdigit(static_cast<unsigned char>(*(iter + 1))) &&
          std::isxdigit(static_cast<unsigned char>(*(iter + 2))) ){

	// Filter out embedded NULL bytes of the form %00 from the URL
	if( (*(iter+1)=='0' && *(iter+2)=='0') ){
	  warning_message = "Warning! Detected embedded NULL byte in URL: " + url;
	  // Wind forward our iterator
	  iter+=2;
	}
	// Otherwise decode the character
	else{
	  c = *++iter;
	  argument.append(1,hexToChar(c,*++iter));
	}
      }
      // Warn but preserve malformed escapes
      else {
	warning_message = "Warning! Malformed percent escape in URL: " + url;
	argument.append(1,'%');
      }
      break;

    default:
      // Filter raw NULL bytes like %00, but warn
      if( *iter == '\0' ){
	warning_message = "Warning! Detected raw NULL byte in URL: " + url;
      }
      else{
	argument.append(1,*iter);
      }
      break;
    }
  }

  return argument;
}


// Escape strings for JSON etc.
inline std::string URL::escape()
{
  std::string json;
  std::string input = this->decode();
  json.reserve(input.size());

  static const char hex[] = "0123456789abcdef";

  for( std::size_t i=0; i<input.size(); i++ ){
    const unsigned char c = static_cast<unsigned char>(input[i]);
    switch(c){
      case '\\':
	json += "\\\\";
	break;
      case '"':
	json += "\\\"";
	break;
      case '\b':
	json += "\\b";
	break;
      case '\f':
	json += "\\f";
	break;
      case '\n':
	json += "\\n";
	break;
      case '\r':
	json += "\\r";
	break;
      case '\t':
	json += "\\t";
	break;
      default:
	if( c < 0x20 ){
	  json += "\\u00";
	  json += hex[(c >> 4) & 0x0f];
	  json += hex[c & 0x0f];
	}
	else{
	  json += static_cast<char>(c);
	}
    }
  }
  return json;
}


#endif
