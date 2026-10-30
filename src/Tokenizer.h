/*
    Simple String Tokenizer Class

    Copyright (C) 2000-2026 Ruven Pillay.

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


#ifndef _TOKENIZER_H
#define _TOKENIZER_H

#include <string>


/// Simple utility class to split a string into tokens

class Tokenizer{

 private:
  std::string arg;
  std::string delim;
  std::string _nextToken();

 public:

  /// Default constructor: empty tokenizer
  Tokenizer() : arg(), delim() {}

  /// Constructor
  /** @param s string to be split
      @param d delimitter
   */
  Tokenizer( const std::string& s, const std::string& d ) : arg( s ), delim( d ) {}

  /// Return the next token
  /** @return next token */
  std::string nextToken();

  /// Indicate whether there are any tokens remaining
  /** @return whether there are more tokens */
  bool hasMoreTokens() const;

};



inline std::string Tokenizer::_nextToken()
{
  size_t n;
  std::string result;

  n = arg.find( delim );

  // No token in string, so return original
  if( n == std::string::npos ){
    result = arg;
    arg = std::string();
  }
  else{
    result = arg.substr( 0, n );
    arg = arg.substr( n + delim.length() );
  }

  return result;
}



inline std::string Tokenizer::nextToken()
{
  std::string result;
  do{
    result = _nextToken();
  }
  while( result.empty() && !arg.empty() );

  return result;

}


inline bool Tokenizer::hasMoreTokens() const
{
  return arg.find_first_not_of( delim ) != std::string::npos;
}



#endif
