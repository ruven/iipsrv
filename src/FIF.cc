/*
    IIP FIF Command Handler Class Member Function

    Copyright (C) 2006-2023 Ruven Pillay.

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


#include <algorithm>
#include "Task.h"
#include "URL.h"
#include "Environment.h"
#include "TPTImage.h"
#include "JPEGImage.h"

#ifdef HAVE_KAKADU
#include "KakaduImage.h"
#endif

#ifdef HAVE_OPENJPEG
#include "OpenJPEGImage.h"
#endif


using namespace std;


// Initialize our static members
long FIF::max_metadata_cache_size = 0;
string FIF::filesystem_prefix;
string FIF::filesystem_suffix;
string FIF::filename_pattern;


void FIF::run( Session* session, const string& src ){

  if( session->loglevel >= 3 ) *(session->logfile) << "FIF handler reached" << endl;

  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // Decode any URL-encoded characters from our path
  URL url( src );
  string argument = url.decode();


  // Filter out any ../ to prevent users by-passing any file system prefix
  unsigned int n;
  while( (n=argument.find("../")) < argument.length() ) argument.erase(n,3);

  if( session->loglevel >=1 ){
    if( url.warning().length() > 0 ) *(session->logfile) << "FIF :: " << url.warning() << endl;
    if( session->loglevel >= 5 ){
      *(session->logfile) << "FIF :: URL decoding/filtering: " << src << " => " << argument << endl;
    }
  }


  // Create our IIPImage object
  IIPImage test;


  // Timestamp of cached image
  time_t timestamp = 0;


  // Put the image setup into a try block as object creation can throw an exception
  try{

    // Check whether we are using a metadata cache
    if( FIF::max_metadata_cache_size == 0 ){
      test = IIPImage( argument );
      test.setFileNamePattern( FIF::filename_pattern );
      test.setFileSystemPrefix( FIF::filesystem_prefix );
      test.setFileSystemSuffix( FIF::filesystem_suffix );
      test.Initialise();
    }
    else{

      // Cache Hit
      if( session->imageCache->find(argument) != session->imageCache->end() ){
	test = (*session->imageCache)[ argument ];
	timestamp = test.timestamp;       // Record timestamp if we have a cached image
	if( session->loglevel >= 2 ){
	  *(session->logfile) << "FIF :: Image metadata cache hit" << endl;
	}
      }

      // Cache Miss
      else{
	if( session->loglevel >= 2 ){
	  *(session->logfile) << "FIF :: Image metadata cache ";
	  if( session->imageCache->empty() ) *(session->logfile) << "initialization" << endl;
	  else *(session->logfile) << "miss" << endl;
	}
	test = IIPImage( argument );
	test.setFileNamePattern( FIF::filename_pattern );
	test.setFileSystemPrefix( FIF::filesystem_prefix );
	test.setFileSystemSuffix( FIF::filesystem_suffix );
	test.Initialise();

	// Delete items if our metadata cache becomes too large - unless we have set cache size to -1 (unlimited)
	if( FIF::max_metadata_cache_size > 0 ){
	  while( session->imageCache->size() >= (unsigned long) FIF::max_metadata_cache_size ){
	    session->imageCache->erase( session->imageCache->begin() );
	  }
	}

      }
    }



    /*****************************************************
      Test for supported image formats: TIFF or JPEG2000
    ******************************************************/

    ImageEncoding format = test.getImageFormat();

    if( format == ImageEncoding::TIFF ){
      if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: TIFF image detected" << endl;
      *session->image = new TPTImage( test );
    }
    else if( format == ImageEncoding::JPEG ){
      if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: JPEG image detected" << endl;
      *session->image = new JPEGImage( test );
    }
#if defined(HAVE_KAKADU) || defined(HAVE_OPENJPEG)
    else if( format == ImageEncoding::JPEG2000 ){
      if( session->loglevel >= 2 )
        *(session->logfile) << "FIF :: JPEG2000 image detected" << endl;
#if defined(HAVE_KAKADU)
      *session->image = new KakaduImage( test );
      if( session->codecOptions["KAKADU_READMODE"] ){
	((KakaduImage*)*session->image)->kdu_readmode = (KakaduImage::KDU_READMODE) session->codecOptions["KAKADU_READMODE"];
      }
#elif defined(HAVE_OPENJPEG)
      *session->image = new OpenJPEGImage( test );
#endif
    }
#endif
    else throw string( "Unsupported image type: " + argument );


    // Open image and update timestamp
    Timer function_timer;
    if( session->loglevel >= 3 ) function_timer.start();
    (*session->image)->openImage();
    if( session->loglevel >= 3 ){
      *(session->logfile) << "FIF :: Image opened in " << function_timer.getTime() << " microseconds" << endl;
    }


    // Check timestamp consistency. If cached timestamp is different, update metadata
    if( timestamp>0 && (timestamp != (*session->image)->timestamp) ){
      timestamp = -1;    // Indicate that we have a reloaded image
      if( session->loglevel >= 2 ){
	*(session->logfile) << "FIF :: Image timestamp changed: reloading metadata" << endl;
      }
      (*session->image)->loadImageInfo( (*session->image)->currentX, (*session->image)->currentY );
    }


    // Set copyright from global startup parameter if none exists within image itself
    map<const string, const string> :: const_iterator mit = (*session->image)->metadata.find( "rights" );
    if( mit == (*session->image)->metadata.end() ){
      string rights = (session->headers["COPYRIGHT"]);
      if( !rights.empty() ) (*session->image)->metadata.insert( {"rights",rights} );
    }


    // Add this image to our cache, overwriting previous version if it exists
    (*session->imageCache)[argument] = *(*session->image);

    if( session->loglevel >= 3 ){
      *(session->logfile) << "FIF :: Created image" << endl;
    }


    // Set the timestamp for the reply
    session->response->setLastModified( (*session->image)->getTimestamp() );

    if( session->loglevel >= 2 ){
      tm *t = gmtime( &(*session->image)->timestamp );
      char strt[64];
      strftime( strt, 64, "%a, %d %b %Y %H:%M:%S GMT", t );

      if( FIF::max_metadata_cache_size != 0 ){
	*(session->logfile) << "FIF :: Image metadata cache size: " << session->imageCache->size() << endl;
      }
      *(session->logfile) << "FIF :: Image dimensions are " << (*session->image)->getImageWidth()
			  << " x " << (*session->image)->getImageHeight() << endl
			  << "FIF :: Image contains " << (*session->image)->channels
			  << " channel" << (((*session->image)->channels>1)?"s":"") << " with "
			  << (*session->image)->bpc << " bit" << (((*session->image)->bpc>1)?"s":"") << " per channel" << endl
			  << "FIF :: Image timestamp: " << strt << endl;
      if( (*session->image)->isStack() ){
	std::list <Stack> stack = (*session->image)->getStack();
	*(session->logfile) << "FIF :: Image is a stack containing " << stack.size() << " elements" << endl;
      }
    }

  }
  catch( const file_error& error ){
    // Unavailable file error code is 1 3
    session->response->setError( "1 3", "FIF" );
    throw;
  }


  // Check whether we have had an if_modified_since header. If so, compare to our image timestamp
  if( session->headers.find("HTTP_IF_MODIFIED_SINCE") != session->headers.end() ){

    tm mod_t;
    time_t t;

    strptime( (session->headers)["HTTP_IF_MODIFIED_SINCE"].c_str(), "%a, %d %b %Y %H:%M:%S %Z", &mod_t );

    t = timegm( &mod_t );
    if( (session->loglevel >= 1) && (t == -1) ) *(session->logfile) << "FIF :: Error creating timestamp" << endl;

    if( (timestamp != -1) && ((*session->image)->timestamp == t) ){
      if( session->loglevel >= 2 ){
	*(session->logfile) << "FIF :: Unmodified content" << endl;
	*(session->logfile) << "FIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
      }
      throw( 304 );
    }
    else{
      if( session->loglevel >= 2 ){
	*(session->logfile) << "FIF :: Content modified since requested time" << endl;
      }
    }
  }


  if( session->loglevel >= 2 ){
    *(session->logfile)	<< "FIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
