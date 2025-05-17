/*
    IIP Response Handler Class

    Copyright (C) 2003-2025 Ruven Pillay.

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


#ifndef _IIPRESPONSE_H
#define _IIPRESPONSE_H

#ifndef VERSION
#define VERSION "999"
#endif

// Fix missing snprintf in Windows
#if defined _MSC_VER && _MSC_VER<1900
#define snprintf _snprintf
#endif


#include <string>


/// Class to handle non-image IIP responses including errors

class IIPResponse{


 private:

  std::string server;              // Server header
  std::string powered;             // Powered By header
  std::string modified;            // Last modified header
  std::string cacheControl;        // Cache control header
  std::string mimeType;            // Mime type header
  std::string eof;                 // End of response delimitter eg "\r\n"
  std::string protocol;            // IIP protocol version
  std::string responseBody;        // The main response
  std::string error;               // Error message
  std::string allow;               // Allow header
  std::string cors;                // CORS (Cross-Origin Resource Sharing) setting
  std::string contentDisposition;  // File name to use for Content Disposition header
  std::string status;              // HTTP status code
  bool _cachable;                  // Indicate whether response should be cached
  bool _sent;                      // Indicate whether a response has been sent


 public:

  /// Constructor
  IIPResponse();


  /// Set the IIP protocol version
  /** @param p IIP protocol version */
  void setProtocol( const std::string& p ) { protocol = p; };


  /// Set the Mime Type
  /** @param m Mime Type */
  void setMimeType( const std::string& m ) { mimeType = "Content-Type: " + m; };


  /// Set the Last Modified header
  /** @param m Last modified date as a HTTP RFC 1123 formatted timestamp */
  void setLastModified( const std::string& m ) { modified = "Last-Modified: " + m; };


  /// Set Content Disposition header
  /** @param name file name for Content-disposition header
      @param type content type (inline by default)
   */
  void setContentDisposition( const std::string& name, const std::string& type = "inline" ) {
    contentDisposition = "Content-Disposition: " + type + "; filename=\"" + name + "\"";
  }


  /// Add a response string
  /** @param r response string */
  void addResponse( const std::string& r );


  /// Add a response string
  /** @param c response string */
  void addResponse( const char* c );


  /// Add a response string
  /** @param c response string
      @param a integer value
   */
  void addResponse( const char* c, int a );


  /// Add a response string
  /** @param c response string
      @param a string reply
   */
  void addResponse( std::string c, const std::string& a );


  /// Add a response string
  /** @param c response string
      @param a integer value
      @param b another integer value
   */
  void addResponse( const char* c, int a, int b );


  /// Set an error
  /** @param code error code
      @param arg the argument supplied by the client
   */
  void setError( const std::string& code, const std::string& arg );


  /// Set CORS setting
  /** @param c setting */
  void setCORS( const std::string& c ){
    if(!c.empty()){
      cors =
	"Access-Control-Allow-Origin: " + c + eof +
	"Access-Control-Allow-Methods: GET, POST, OPTIONS" + eof +
	"Access-Control-Allow-Headers: Accept, Content-Type, X-Requested-With, If-Modified-Since" + eof +
	"Access-Control-Max-Age: 86400";
    }
  };


  /// Get CORS setting
  std::string getCORS(){ return cors; };


  /// Set Cache-Control value
  /** @param c Cache-Control setting */
  void setCacheControl( const std::string& c ){ cacheControl = "Cache-Control: " + c; };


  /// Set whether the response should be cached
  /** @param cachable Whether this reponse should be cached or not */
  void setCachability( bool cachable ){ _cachable = cachable; };


  /// Is response cachable?
  /** @return Whether response should be cached */
  bool cachable(){ return _cachable; };


  /// Get Cache-Control value
  std::string getCacheControl(){ return cacheControl; };


  /// Set HTTP status code
  /** @param s HTTP status code string */
  void setStatus( const std::string& s ){ status = "Status: " + s; }


  /// Get a formatted string to send back
  std::string formatResponse();


  /// Indicate whether this object has had any arguments passed to it
  bool isSet(){
    if( error.length() || responseBody.length() || protocol.length() ) return true;
    else return false;
  }


  /// Indicate whether we have an error message
  bool errorIsSet(){
    if( error.length() ) return true;
    else return false;
  }


  /// Set the sent flag indicating that some sort of response has been sent
  void setImageSent() { _sent = true; };


  /// Indicate whether a response has been sent
  bool imageSent() { return _sent; };


  /// Display our advertising banner ;-)
  /** @return HTML string */
  std::string getAdvert();


  /// Convenience function to generate HTTP header fields
  /** @param mimeType MIME type of output
      @param timeStamp formatted timestamp
      @param contentLength optional Content-Length value
      @return formatted HTTP header
   */
  std::string createHTTPHeader( const std::string& mimeType, const std::string& timeStamp, unsigned int contentLength=0 );


  /// Convenience function to generate a header-only response
  /** @param addCORS whether a CORS header should be added
      @return formatted HTTP header string
   */
  std::string getHeaderResponse( bool addCORS=false );

};

#endif
