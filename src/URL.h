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

  /// Internal utility function to sanitise a decoded filesystem path:
  /// remove ./ and ../ components, collapse repeated '/' and bound the length
  /** @param path path string
      @return sanitized path
   */
  std::string sanitize( const std::string& path ) const;

 public:

  /// Constructor
  /** @param s input url string */
  URL( const std::string& s ){ url = s; };

  /// Decode and filter URL
  /** @param sanitize sanitize paths if true: remove ./ and ../, collapse repeated '/' and bound the length
      @return sanitized string
   */
  std::string decode( bool sanitize = false );

  /// String escaping for JSON etc
  /** @return escaped string */
  std::string escape();

  /// Return any warning message
  /** @return warning */
  const std::string& warning() const { return warning_message; };

};


/// Decode a hex-encoded string
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


// Sanitize a decoded filesystem path: remove ./ and ../ components (the
// latter to prevent escaping the filesystem prefix), collapse repeated '/'
// and bound the total length
inline std::string URL::sanitize( const std::string& path ) const
{
  std::string filtered;
  filtered.reserve( path.length() );

  bool last_was_slash = false;
  for( std::size_t i=0; i<path.length(); ){
    if( i+2 < path.length() && path[i]=='.' && path[i+1]=='.' && path[i+2]=='/' ){
      i += 3;                       // skip "../"
      last_was_slash = true;        // removed component: swallow next '/'
      continue;
    }
    if( i+1 < path.length() && path[i]=='.' && path[i+1]=='/' ){
      i += 2;                       // skip "./"
      last_was_slash = true;        // removed component: swallow next '/'
      continue;
    }
    // Trailing ".." component without a slash to prevent traversal
    if( i+2 == path.length() && path[i]=='.' && path[i+1]=='.' && (last_was_slash || filtered.empty()) ){
      if( !filtered.empty() && filtered.back() == '/' ) filtered.pop_back();
      i += 2;                       // skip trailing ".."
      continue;
    }
    const char c = path[i++];
    if( c == '/' ){
      if( last_was_slash ) continue;
      last_was_slash = true;
    }
    else last_was_slash = false;
    filtered += c;
  }

  // PATH_MAX is 4096, but this is only the user-supplied part of the path:
  // IIPImage::getFileName() assembles prefix + path + pattern + suffix into
  // a 1024-byte buffer, and NAME_MAX is 255 for an individual component.
  const std::size_t max_length = 512;
  if( filtered.length() > max_length ) filtered.erase( max_length );
  return filtered;
}


// The argument is a URL path, which may contain spaces or other hex encoded characters.
// So, first decode and filter this path (implementation taken from GNU cgicc: http://www.cgicc.org)
inline std::string URL::decode( bool sanitize )
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

  if( sanitize ) argument = this->sanitize( argument );
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
