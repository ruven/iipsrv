/*
    IIP FIF Command Handler Class Member Function

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


#include "Task.h"
#include "Environment.h"
#include "TPTImage.h"

#ifdef HAVE_KAKADU
#include "KakaduImage.h"
#endif

using namespace std;



// Internal utility function
static char hexToChar( char first, char second ){
  int digit;
  digit = (first >= 'A' ? ((first & 0xDF) - 'A') + 10 : (first - '0'));
  digit *= 16;
  digit += (second >= 'A' ? ((second & 0xDF) - 'A') + 10 : (second - '0'));
  return static_cast<char>(digit);
}



void FIF::run( Session* session, const string& src ){

  if( session->loglevel >= 3 ) *(session->logfile) << "FIF handler reached" << endl;

  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // The argument is a URL path, which may contain spaces or other characters
  // encoded URL form.
  // So, first decode this path (implementation taken from GNU cgicc: http://www.cgicc.org)

  string argument;
  string::const_iterator iter;
  char c;

  for(iter = src.begin(); iter != src.end(); ++iter) {
    switch(*iter) {
    case '+':
      argument.append(1,' ');
      break;
    case '%':
      // Don't assume well-formed input
      if( std::distance(iter, src.end()) >= 2 &&
	  std::isxdigit(*(iter + 1)) && std::isxdigit(*(iter + 2)) ){

	// Filter out embedded NULL bytes of the form %00 from the URL
	if( (*(iter+1)=='0' && *(iter+2)=='0') ){
	  if( session->loglevel >= 1 ){
	    *(session->logfile) << "FIF :: Warning! Detected embedded NULL byte in URL: " << src << endl;
	  }
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

  // Filter out any ../ to prevent users by-passing any file system prefix
  unsigned int n;
  while( (n=argument.find("../")) < argument.length() ) argument.erase(n,3);


  if( session->loglevel >= 5 ){
    *(session->logfile) << "FIF :: URL decoding/filtering: " << src << " => " << argument << endl;
  }


  IIPImage test;

  // Get our image pattern variable
  string filesystem_prefix = Environment::getFileSystemPrefix();

  // Get our image pattern variable
  string filename_pattern = Environment::getFileNamePattern();
  


  // Put the image opening into a try block so that we can set
  // a meaningful error
  try{

    // TODO: Try to use a reference to this list, so that we can
    //  keep track of the current sequence between runs

    if( session->imageCache->empty() ){

      if( session->loglevel >= 1 ) *(session->logfile) << "FIF :: Image cache initialisation" << endl;
      test = IIPImage( argument );
      test.setFileNamePattern( filename_pattern );
      test.setFileSystemPrefix( filesystem_prefix );
      test.Initialise();

      (*session->imageCache)[argument] = test;
    }

    else{

      if( session->imageCache->find(argument) != session->imageCache->end() ){
	test = (*session->imageCache)[ argument ];
	if( session->loglevel >= 2 ){
	  *(session->logfile) << "FIF :: Image cache hit. Number of elements: " << session->imageCache->size() << endl;
	}
      }
      else{
	if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: Image cache miss" << endl;
	test = IIPImage( argument );
	test.setFileNamePattern( filename_pattern );
	test.setFileSystemPrefix( filesystem_prefix );
	test.Initialise();
	if( session->imageCache->size() >= 100 ) session->imageCache->erase( session->imageCache->end() );
	(*session->imageCache)[argument] = test;
      }
    }



    /***************************************************************
	      Test for different image types - only TIFF is native for now
    ***************************************************************/

    string imtype = test.getImageType();

    // Transform the suffix to lower case
    transform( imtype.begin(), imtype.end(), imtype.begin(), ::tolower );

    if( imtype=="tif" || imtype=="tiff" || imtype=="ptif" || imtype=="dat" ){
      if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: TIFF image requested" << endl;
      *session->image = new TPTImage( test );
    }
#ifdef HAVE_KAKADU
    else if( imtype=="jpx" || imtype=="jp2" ){
      if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: JPEG2000 image requested" << endl;
      *session->image = new KakaduImage( test );
    }
#endif
    else throw string( "Unsupported image type: " + imtype );

    /* Disable module loading for now!
    else{

#ifdef ENABLE_DL

      // Check our map list for the requested type
      if( moduleList.empty() ){
	throw string( "Unsupported image type: " + imtype );
      }
      else{

	map<string, string> :: iterator mod_it  = moduleList.find( imtype );

	if( mod_it == moduleList.end() ){
	  throw string( "Unsupported image type: " + imtype );
	}
	else{
	  // Construct our dynamic loading image decoder 
	  session->image = new DSOImage( test );
	  (*session->image)->Load( (*mod_it).second );

	  if( session->loglevel >= 2 ){
	    *(session->logfile) << "FIF :: Image type: '" << imtype
	                        << "' requested ... using handler "
				<< (*session->image)->getDescription() << endl;
	  }
	}
      }
#else
      throw string( "Unsupported image type: " + imtype );
#endif
    }
    */


    if( session->loglevel >= 3 ){
      *(session->logfile) << "FIF :: Created image" << endl;
    }


    (*session->image)->openImage();
    session->response->setLastModified( (*session->image)->getTimestamp() );

    if( session->loglevel >= 2 ){
      *(session->logfile) << "FIF :: Image dimensions are " << (*session->image)->getImageWidth()
			  << " x " << (*session->image)->getImageHeight() << endl;
      tm *t;
      t = gmtime( &(*session->image)->timestamp );
      char strt[128];
      strftime( strt, 128, "%a, %d %b %Y %H:%M:%S GMT", t );
      *(session->logfile) << "FIF :: Image timestamp: " << strt << endl;
    }

  }
  catch( const string& error ){
    // Unavailable file error code is 1 3
    session->response->setError( "1 3", "FIF" );
    throw error;
  }


  // Check whether we have had an if modified since header. If so, compare to our image timestamp
  if( session->headers.find("HTTP_IF_MODIFIED_SINCE") != session->headers.end() ){

      tm mod_t;
      strptime( (session->headers)["HTTP_IF_MODIFIED_SINCE"].c_str(), "%a, %d %b %Y %H:%M:%S GMT", &mod_t );
      time_t t;
      t = timegm(&mod_t);
      if( (*session->image)->timestamp <= t ){

	if( session->loglevel >= 2 ){
	  *(session->logfile)	<< "FIF :: Unmodified content" << endl;
	  *(session->logfile)	<< "FIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
	}

	throw( 304 );
      }
      else{
	if( session->loglevel >= 2 ){
	  *(session->logfile)	<< "FIF :: Content modified" << endl;
	}
      }
  }

  // Reset our angle values
  session->view->xangle = 0;
  session->view->yangle = 90;


  if( session->loglevel >= 2 ){
    *(session->logfile)	<< "FIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
