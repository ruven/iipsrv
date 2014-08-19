/*

    IIIF Request Command Handler Class Member Function

    Copyright (C) 2014 Ruven Pillay

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include "Environment.h"
#include "Task.h"
#include "Tokenizer.h"
#include "Transforms.h"
#include "URL.h"

#if _MSC_VER
#include "../windows/Time.h"
#endif


// Define several IIIF strings
#define IIIF_SYNTAX "IIIF syntax is {identifier}/{region}/{size}/{rotation}/{quality}{.format}"
//#define IIIF_PROFILE "http://iiif.io/api/image/1.1/compliance.html#level1"
#define IIIF_PROFILE "http://iiif.io/api/image/2/level1.json"
//#define IIIF_CONTEXT "http://iiif.io/image-api/1.1/context.json"
#define IIIF_CONTEXT "http://iiif.io/api/image/2/context.json"
#define IIIF_PROTOCOL "http://iiif.io/api/image"


using namespace std;



// The request is in the form {identifier}/{region}/{size}/{rotation}/{quality}{.format}
//     eg. filename.tif/full/full/0/native.jpg
// or in the form {identifier}/info.json
//    eg. filename.jp2/info.json

void IIIF::run( Session* session, const string& src ){

  if( session->loglevel >= 3 ) *(session->logfile) << "IIIF handler reached" << endl;

  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // Various string variables
  string suffix, filename, params;


  // First filter and decode our URL
  URL url( src );
  string argument = url.decode();

  if( session->loglevel >=1 ){
    if( url.warning().length() > 0 ) *(session->logfile) << "IIIF :: " << url.warning() << endl;
    if( session->loglevel >= 5 ){
      *(session->logfile) << "IIIF :: URL decoded to " << argument << endl;
    }
  }


  // Check if there is slash in argument and if it is not last / first character, extract identifier and suffix
  size_t lastSlashPos = argument.find_last_of("/");
  if( lastSlashPos < argument.length() && lastSlashPos > 0 ){

    suffix = argument.substr( lastSlashPos+1, string::npos );

    // If we have an info command, file name must be everything 
    if( suffix.substr(0,4) == "info" ){
      filename = argument.substr(0,lastSlashPos);
    }
    else{
      size_t positionTmp = lastSlashPos;
      for( int i=0; i<3; i++ ){
        positionTmp = argument.substr(0,positionTmp).find_last_of("/");
      }
      if( positionTmp > 0 ){
        filename = argument.substr(0,positionTmp);
        params = argument.substr(positionTmp + 1, string::npos);
      }
      else{
	// No extra parameters
	throw invalid_argument( "IIIF: Not enough parameters" );
      }
    }
  }
  else{
    // No parameters, so redirect to info request
    string id;
    string host = Environment::getBaseURL();
    if( host.length() > 0 ){
      id = host + (session->headers["QUERY_STRING"]).substr(5,string::npos);
    }
    else{
      string request_uri = session->headers["REQUEST_URI"];
      request_uri.erase( request_uri.length() - suffix.length() - 1, string::npos );
      id = "http://" + session->headers["HTTP_HOST"] + request_uri;
    }
    string header = string( "Status: 303 See Other\r\n" )
      + "Location: " + id + "/info.json\r\n"
      + "Server: iipsrv/" + VERSION + "\r\n"
      + "\r\n";
    session->out->printf( (const char*) header.c_str() );
    session->response->setImageSent();
    if( session->loglevel >= 2 ){
      *(session->logfile) << "IIIF :: Sending HTTP 303 See Other : " << id + "/info.json" << endl;
    }
    return;
  }


  // Check whether requested image exists
  FIF fif;
  fif.run( session, filename );

  // Reload our filename
  filename = (*session->image)->getImagePath();

  // Get the information about image, that can be shown in info.json
  unsigned int width = (*session->image)->getImageWidth();
  unsigned int height = (*session->image)->getImageHeight();
  unsigned tw = (*session->image)->getTileWidth();
  unsigned th = (*session->image)->getTileHeight();
  unsigned numResolutions = (*session->image)->getNumResolutions();

  session->view->setImageSize( width, height );
  session->view->setMaxResolutions( numResolutions );


  // PARSE INPUT PARAMETERS

  // info.json
  if( suffix == "info.json" ){

    // Our string buffer
    stringstream infoStringStream;

    // Encoded file name
    string escapedFilename;

    // Determine the URL used to access us
    // Generate our @id - use our BASE_URL environment variable if we are
    //  behind a web server rewrite function
    string id;
    string host = Environment::getBaseURL();
    if( host.length() > 0 ){
      id = host + (session->headers["QUERY_STRING"]).substr(5,string::npos);
    }
    else{
      string request_uri = session->headers["REQUEST_URI"];
      request_uri.erase( request_uri.length() - suffix.length() - 1, string::npos );
      id = "http://" + session->headers["HTTP_HOST"] + request_uri;
    }

    // JSON encoding of filename
    for( unsigned int i=0; i<id.length(); i++ ){
      char c = id[i];
      switch(c){
      case '\\':
	escapedFilename += "\\\\";
	break;
      case '"':
	escapedFilename += "\\\"";
	break;
      default:
	escapedFilename += c;
      }
    }


    infoStringStream << "{" << endl
		     << "  \"@context\" : \"" << IIIF_CONTEXT << "\"," << endl
 		     << "  \"@id\" : \"" << escapedFilename << "\"," << endl
		     << "  \"protocol\" : \"" << IIIF_PROTOCOL << "\"," << endl
		     << "  \"width\" : " << width << "," << endl
		     << "  \"height\" : " << height << "," << endl
		     << "  \"tiles\" : [{ \"width\" : " << tw << ", \"height\" : " << th
		     << ", \"scale_factors\" : [ 1"; // Scale 1 is original image

    for( unsigned int i=1; i < numResolutions; i++ ){
      infoStringStream << ", " << pow(2.0,i);
    }

    infoStringStream << " ] }]," << endl
		     << "  \"profile\" : [" << endl
		     << "     \"" << IIIF_PROFILE << "\"," << endl
		     << "     { \"formats\" : [ \"jpg\" ]," << endl
		     << "      \"qualities\" : [ \"native\",\"color\",\"gray\" ]," << endl
		     << "      \"supports\" : [\"region_by_pct\",\"size_by_forced_wh\",\"size_by_wh\",\"size_above_full\",\"rotation_by_90s\",\"mirroring\",\"gray\"] }" << endl
		     << "   ]" << endl
		     << "}";


// 1.1 info format
//     infoStringStream << "{" << endl
// 		     << "  \"@context\" : \"" << IIIF_CONTEXT << "\"," << endl
//  		     << "  \"@id\" : \"" << escapedFilename << "\"," << endl
// 		     << "  \"profile\" : \"" << IIIF_PROFILE << "\"," << endl
// 		     << "  \"width\" : " << width << "," << endl
// 		     << "  \"height\" : " << height << "," << endl
// 		     << "  \"scale_factors\" : [ 1"; // Scale 1 is original image

//     for( unsigned int i=1; i < numResolutions; i++ ){
//       infoStringStream << ", " << pow(2.0,i);
//     }

//     infoStringStream << " ]," << endl
// 		     << "  \"tile_width\" : " << tw << "," << endl
// 		     << "  \"tile_height\" : " << th << "," << endl
// 		     << "  \"formats\" : [ \"jpg\" ]," << endl
// 		     << "  \"qualities\" : [ \"native\",\"color\",\"grey\" ]" << endl
// 		     << "}";

    // Get our Access-Control-Allow-Origin value, if any
    string cors = Environment::getCORS();
    string eof = "\r\n";

    // Now output the info text
    stringstream header;
    header << "Server: iipsrv/" << VERSION << eof
	   << "Content-Type: application/ld+json" << eof
	   << "Cache-Control: max-age=" << MAX_AGE << eof
	   << "Last-Modified: " << (*session->image)->getTimestamp() << eof
	   << "Link: <" << IIIF_PROFILE << ">;rel=\"profile\"" << eof;
    if( !cors.empty() ) header << "Access-Control-Allow-Origin: " << cors << eof;
    header << eof << infoStringStream.str();

    session->out->printf( (const char*) header.str().c_str() );
    session->response->setImageSent();

    return;

  }


  // Parse image request - any other than info requests are considered image requests
  else{

    // IIIF requests are / separated with no CGI style '&' separators
    Tokenizer izer( params, "/" );

    // Keep track of the number of parameters than have been given
    int numOfTokens = 0;


    // Region Parameter: { "full"; "x,y,w,h"; "pct:x,y,w,h" }
    if( izer.hasMoreTokens() ){

      // Our region parameters
      float region[4];

      // Get our region string and convert to lower case if necessary
      string regionString = izer.nextToken();
      transform( regionString.begin(), regionString.end(), regionString.begin(), ::tolower );

      // Full export request
      if( regionString == "full" ){
        region[0] = 0.0;
        region[1] = 0.0;
        region[2] = 1.0;
        region[3] = 1.0;
      }

      // Region export request
      else{

        // Check for pct (%) and strip it from the beginning
        bool isPCT = false;
        if( regionString.substr(0,4) == "pct:" ){
          isPCT = true;
	  // Strip this from our string
	  regionString.erase( 0, 4 );
        }

        // Extract x,y,w,h tokenizing on ","
        Tokenizer regionIzer(regionString, ",");
        int n = 0;

	// Extract our region values
	while( regionIzer.hasMoreTokens() && n < 4 ){
	  region[n++] = atof( regionIzer.nextToken().c_str() );
	}

	// Define our denominators as our session view expects a ratio, not pixel values
	float wd = (float)width;
	float hd = (float)height;

	if( isPCT ){
	  wd = 100.0;
	  hd = 100.0;
	}

	session->view->setViewLeft( region[0] / wd );
	session->view->setViewTop( region[1] / hd );
	session->view->setViewWidth( region[2] / wd );
	session->view->setViewHeight( region[3] / hd );

        // Incorrect region request
        if( regionIzer.hasMoreTokens() || n < 4 ){
	  throw invalid_argument( "IIIF: incorrect region format: " + regionString );
        }

      } // end of else - end of parsing x,y,w,h

      numOfTokens++;

      if( session->loglevel > 4 ){
        *(session->logfile) << "IIIF :: Requested Region: x:" << region[0] << ", y:" << region[1]
			    << ", w:" << region[2] << ", h:" << region[3] << endl;
      }

    }



    // Size Parameter: { "full"; "w,"; ",h"; "pct:n"; "w,h"; "!w,h" }
    if( izer.hasMoreTokens() ){

      string sizeString = izer.nextToken();
      transform( sizeString.begin(), sizeString.end(), sizeString.begin(), ::tolower );

      // Calculate the width and height of our region
      unsigned int requested_width = session->view->getViewWidth();
      unsigned int requested_height = session->view->getViewHeight();


      // "full" request
      if( sizeString == "full" ){
	// No need to do anything
      }

      // "pct:n" request
      else if( sizeString.substr(0,4) == "pct:" ){

	float scale;
	istringstream i( sizeString.substr( sizeString.find_first_of(":")+1, string::npos ) );
	if( !(i >> scale) ) throw invalid_argument( "invalid size" );

	requested_width = round( requested_width * scale / 100.0 );
	requested_height = round( requested_height * scale / 100.0 );
      }

      // "w,h", "w,", ",h", "!w,h" requests
      else{

        // Indicates that before width is exclamation mark
        bool isExclamationMark = false;

        // !w,h request - remove !, remember it and continue as if w,h request
        if( sizeString.substr(0,1) == "!" ) {
          isExclamationMark = true;
          sizeString.erase(0,1);
        }
	// Otherwise tell our view to break aspect ratio
	else session->view->maintain_aspect = false;

	size_t pos = sizeString.find_first_of(",");

	// If no comma, size is invalid
	if( pos == string::npos ){
	  throw invalid_argument( "invalid size: no comma found" );
	}

	// If comma is at the beginning, we have a ",height" request
	else if( pos == 0 ){
	  istringstream i( sizeString.substr( 1, string::npos ) );
	  if( !(i >> requested_height) ) throw invalid_argument( "invalid height" );
	  requested_width = round( (float)requested_height*width / (float)height );
	}

	// If comma is not at the beginning, we must have a "width,height" or "width," request
	// Test first for the "width," request
	else if( pos == sizeString.length()-1 ){
	  istringstream i( sizeString.substr( 0, string::npos - 1 ) );
	  if( !(i >> requested_width ) ) throw invalid_argument( "invalid width" );
	  requested_height = round( (float)requested_width*height / (float)width );
	}

	// Remaining case is "width,height"
	else{
	  istringstream i( sizeString.substr( 0, pos ) );
	  if( !(i >> requested_width) ) throw invalid_argument( "invalid width" );
	  i.clear();
	  i.str( sizeString.substr( pos+1, string::npos ) );
	  if( !(i >> requested_height) ) throw invalid_argument( "invalid height" );

	  // Fit within requested size
	  if( isExclamationMark ){
	    // Make sure the requested image fits *within* the requested dimensions
	    if( ((float)requested_height/(float)session->view->getViewHeight()) >
		((float)requested_width/(float)session->view->getViewWidth()) ){
	      requested_height = (unsigned int) round((((float)requested_width/(float)session->view->getViewWidth()) * session->view->getViewHeight()));
	    }
	    else if( ((float)requested_width/(float)session->view->getViewWidth()) >
		     ((float)requested_height/(float)session->view->getViewHeight()) ){
	      requested_width = (unsigned int) round((((float)requested_height/(float)session->view->getViewHeight()) * session->view->getViewWidth()));
	    }
	  }
	}
      }


      if( requested_width==0 || requested_height==0 ){
	throw invalid_argument( "IIIF: invalid size" );
      }

      session->view->setRequestWidth( requested_width );
      session->view->setRequestHeight( requested_height );

      numOfTokens++;

      if( session->loglevel >= 4 ){
        *(session->logfile) << "IIIF :: Requested Size: " << requested_width << "x" << requested_height << endl;
      }

    }



    // Rotation Parameter
    if( izer.hasMoreTokens() ){

      string rotationString = izer.nextToken();

      // Flip requests (IIIF 2.0 API)
      if( rotationString.substr(0,1) == "!" ){
	session->view->flip = 1;
	rotationString.erase(0,1);
      }


      // Convert our string to a float
      float rotation = 0;
      istringstream i( rotationString );
      if( !(i >> rotation) ) throw invalid_argument( "IIIF: invalid rotation" );

 
      // Check if converted value is supported
      if(!( rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270 || rotation == 360 )){
	throw invalid_argument( "IIIF: currently implemented rotation angles are 0, 90, 180 and 270 degrees" );
      }

      // Set rotation - watch for a '!180' request, which is simply a vertical flip
      if( rotation == 180 && session->view->flip == 1 ) session->view->flip = 2;
      else session->view->setRotation( rotation );

      numOfTokens++;

      if( session->loglevel >= 4 ){
        *(session->logfile) << "IIIF :: Requested Rotation: " << rotation << " degrees";
	if( session->view->flip != 0 ) *(session->logfile) << " with horizontal flip";
	*(session->logfile) << endl;
      }

    }



    // Quality and Format Parameters
    if( izer.hasMoreTokens() ){

      string format = "jpg";
      string quality = izer.nextToken();
      transform( quality.begin(), quality.end(), quality.begin(), ::tolower );

      size_t pos = quality.find_last_of(".");

      // Format - if dot is not present, we use default and currently only supported format - JPEG
      if( pos != string::npos ){
        format = quality.substr( pos+1, string::npos );
        quality.erase( pos, string::npos );
	if( format != "jpg" ){
	  throw invalid_argument( "IIIF :: Only JPEG output supported" );
	}
      }

      // Quality
      if( quality == "native" || quality == "color" || quality == "default" ){
	// Do nothing
      }
      else if( quality == "grey" || quality == "gray" ){
	session->view->colourspace = GREYSCALE;
      }
      else{
	throw invalid_argument( "unsupported quality parameter - must be one of native, color or grey" );
      }

      numOfTokens++;

      if( session->loglevel >= 4 ){
        *(session->logfile) << "IIIF :: Requested Quality: " << quality << " with format: " << format << endl;
      }
    }



    // TOO MANY PARAMETERS, tell it to user and show him his request
    if( izer.hasMoreTokens() ){
      throw invalid_argument( "IIIF: Query has too many parameters. " IIIF_SYNTAX );
    }


    // NOT ENOUGH PARAMETERS
    if( numOfTokens < 4 ){
      throw invalid_argument( "IIIF: Query has too few parameters. " IIIF_SYNTAX );
    }

  }
  // End of parsing input parameters



  // Write info about request to log
  if( session->loglevel >= 3 ){
    if( suffix == "info.json" ){
      *(session->logfile) << "IIIF :: " << suffix << " request for " << (*session->image)->getImagePath() << endl;
    }
    else{
      *(session->logfile) << "IIIF :: image request for " << (*session->image)->getImagePath()
			  << " with arguments: region: " << session->view->getViewLeft() << "," << session->view->getViewTop() << ","
			  << session->view->getViewWidth() << "," << session->view->getViewHeight()
			  << "; size: " << session->view->getRequestWidth() << "x" << session->view->getRequestHeight()
			  << "; rotation: " << session->view->getRotation()
			  << endl;
    }
  }



  // Define our separator depending on the OS
#ifdef WIN32
  const string separator = "\\";
#else
  const string separator = "/";
#endif



  // Get most suitable resolution and recalculate width and height of region in this resolution
  int requested_res = session->view->getResolution();

  unsigned int im_width = (*session->image)->image_widths[numResolutions-requested_res-1];
  unsigned int im_height = (*session->image)->image_heights[numResolutions-requested_res-1];

  unsigned int view_left, view_top, view_width, view_height;

  if( session->view->viewPortSet() ){
    // Set the absolute viewport size and extract the co-ordinates
    view_left = session->view->getViewLeft();
    view_top = session->view->getViewTop();
    view_width = session->view->getViewWidth();
    view_height = session->view->getViewHeight();
  }
  else{
    view_left = 0;
    view_top = 0;
    view_width = im_width;
    view_height = im_height;
  }

  unsigned int resampled_width = session->view->getRequestWidth();
  unsigned int resampled_height = session->view->getRequestHeight();

  *(session->logfile) << "IIIF :: view: " << view_width << "x" << view_height << endl;


  // Determine whether this is a tile request which coincides with our tile boundaries
  if( view_width == tw && view_height == th && (view_left%tw == 0) && (view_top%th == 0) ){

    // Get the width and height for last row and column tiles
    unsigned int rem_x = im_width % tw;

    // Calculate the number of tiles in each direction
    unsigned int ntlx = (im_width / tw) + (rem_x == 0 ? 0 : 1);

    // Calculate tile index
    unsigned int i = view_left/tw;
    unsigned int j = view_top/th;
    unsigned int tile = (j*ntlx) + i;

    // Simply pass this on to our JTL send command
    JTL jtl;
    jtl.send( session, requested_res, tile );

  }
  else{
    // Otherwise do a CVT style region request
    CVT cvt;
    cvt.send( session );
  }


  // Total IIIF response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "IIIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
