/*
    Simple String Tokenizer Class

    Copyright (C) 2000-2013 Ruven Pillay.

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

  /// Constructor
  /** \param s string to be split
      \param d delimitter
   */
  Tokenizer( const std::string& s, const std::string& d );

  /// Return the next token
  std::string nextToken();

  /// Indicate whether there are any tokens remaining
  int hasMoreTokens();

};



inline Tokenizer::Tokenizer( const std::string& s, const std::string& t )
{
  arg = s;
  delim = t;
}



inline std::string Tokenizer::_nextToken()
{
  int n;
  std::string result;

  n = arg.find( delim );

  // No token in string, so return original
  if( n < 0 ){
    result = arg;
    arg = std::string();
  }
  else{
    result = arg.substr( 0, n );
    arg = arg.substr( n + delim.length(), arg.length() );
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


inline int Tokenizer::hasMoreTokens()
{
  int n = arg.find_first_not_of( delim, 0 );
  if( n >= 0 ) return 1;
  else return 0;
}



#endif
