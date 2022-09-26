/*
    IIP Response Handler Class

    Copyright (C) 2003-2022 Ruven Pillay.

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

#include "IIPResponse.h"
#include <cstdio>
#include <cstring>
#include <sstream>

using namespace std;



IIPResponse::IIPResponse(){

  responseBody = "";
  error = "";
  protocol = "";
  server = "Server: iipsrv/" + string(VERSION);
  powered = "X-Powered-By: IIPImage";
  modified = "";
  mimeType = "Content-Type: application/vnd.netfpx";
  cors = "";
  eof = "\r\n";
  _sent = false;
  _cachable = true;
}


void IIPResponse::addResponse( const string& r ){

  responseBody.append( r );
  responseBody.append( eof );
}


void IIPResponse::addResponse( const char* c ){

  responseBody.append( c );
  responseBody.append( eof );
}


void IIPResponse::addResponse( const char* c, int a ){

  char tmp[64];
  snprintf( tmp, 64, "%s:%d", c, a );
  responseBody.append( tmp );
  responseBody.append( eof );
}


void IIPResponse::addResponse( string arg, const string& s ){

  char tmp[8];
  snprintf( tmp, 8, "/%d:", (int) s.size() );
  responseBody.append( arg );
  responseBody.append( tmp );
  responseBody.append( s );
  responseBody.append( eof );
}


void IIPResponse::addResponse( const char* c, int a, int b ){

  char tmp[64];
  snprintf( tmp, 64, "%s:%d %d", c, a, b );
  responseBody.append( tmp );
  responseBody.append( eof );
}


void IIPResponse::setError( const string& code, const string& arg ){

  char tmp[32];
  snprintf( tmp, 32, "Error/%ld:%s %s", (long)(code.length() + arg.length() + 1), code.c_str(), arg.c_str() );
  error += tmp + eof;
}


string IIPResponse::formatResponse(){

  /* We always need 2 sets of eof after the headers before body/response
   */
  string response;
  if( error.length() ){
    response = server + eof + "Cache-Control: no-cache" + eof + mimeType + eof;
    if( !cors.empty() ) response += cors + eof;
    response += status + eof +
      "Content-Disposition: inline;filename=\"IIPisAMadGameClosedToOurUnderstanding.netfpx\"" +
      eof + eof + error;
  }
  else{
    response = server + eof + powered + eof + cacheControl + eof + modified + eof + mimeType + eof;
    if( !cors.empty() ) response += cors + eof;
    response += eof + protocol + eof + responseBody;
  }

  return response;
}



string IIPResponse::getAdvert(){

  string advert = server + eof + "Content-Type: text/html" + eof;
  advert += "Content-Disposition: inline;filename=\"iipsrv.html\"" + eof;
  advert += status + eof + eof;
  advert += "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"/><title>IIPImage Server</title><meta name=\"DC.creator\" content=\"Ruven Pillay &lt;ruven@users.sourceforge.net&gt;\"/><meta name=\"DC.title\" content=\"IIPImage Server\"/><meta name=\"DC.source\" content=\"https://iipimage.sourceforge.io\"/></head><body style=\"font-family:Helvetica,sans-serif; margin:4em\"><center><h1>IIPImage Server</h1><h2>Version "
    + string( VERSION ) +
    "</h2><br/><h3>Project Home Page: <a href=\"https://iipimage.sourceforge.io\">https://iipimage.sourceforge.io</a></h3><br/><h4>by<br/>Ruven Pillay</h4></center></body></html>";

  return advert;

}


string IIPResponse::createHTTPHeader( const string& mimeType, const string& timeStamp, unsigned int contentLength ){

  string _mimeType = mimeType;
  if( mimeType.find("/") == string::npos ) _mimeType = "application/" + mimeType;

  stringstream header;
  header << "Server: iipsrv/" << VERSION << eof
         << "X-Powered-By: IIPImage" << eof
         << "Content-Type: " << _mimeType << eof
         << "Last-Modified: " << timeStamp << eof
	 << cacheControl << eof;

  if( contentLength > 0 ) header << "Content-Length: " << contentLength << eof;

  // Add CORS header if we have one
  if ( !cors.empty() ) header << cors << eof;

  // Need extra EOF separator
  header << eof;

  return header.str();
}
