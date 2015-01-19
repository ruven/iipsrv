/*
    Simple URL decoder Class

    Copyright (C) 2014-2015 Ruven Pillay.

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


/// Simple utility class to decode and filter URLs

class URL{

 private:

  /// URL string
  std::string url;

  /// Warning
  std::string warning_message;

  // Internal utility function to decode hex values
  char hexToChar( char first, char second );

 public:

  /// Constructor
  /** @param s input url string */
  URL( std::string s ){ url = s; };

  /// Decode and filter URL
  std::string decode();

  /// String escaping for JSON etc
  std::string Escape();

  /// Return any warning message
  std::string warning(){ return warning_message; };

};


inline char URL::hexToChar( char first, char second ){
  int digit;
  digit = (first >= 'A' ? ((first & 0xDF) - 'A') + 10 : (first - '0'));
  digit *= 16;
  digit += (second >= 'A' ? ((second & 0xDF) - 'A') + 10 : (second - '0'));
  return static_cast<char>(digit);
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
      if( std::distance(iter, url.end()) >= 2 &&
          std::isxdigit(*(iter + 1)) && std::isxdigit(*(iter + 2)) ){

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
      // Just pass the % through untouched
      else {
	argument.append(1,'%');
      }
      break;

    default:
      argument.append(1,*iter);
      break;
    }
  }

  return argument;
}


// Escape strings for JSON etc.
inline std::string URL::Escape()
{
  std::string json;

  for( unsigned int i=0; i<url.length(); i++ ){
    char c = url[i];
    switch(c){
      case '\\':
	json += "\\\\";
	break;
      case '"':
	json += "\\\"";
	break;
      default:
	json += c;
    }
  }
  return json;
}


#endif
