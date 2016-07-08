/*
    IIP FIF Command Handler Class Member Function

    Copyright (C) 2006-2015 Ruven Pillay.

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

#ifdef HAVE_KAKADU
#include "KakaduImage.h"
#endif

#ifdef HAVE_OPENJPEG
#include "OpenJPEGImage.h"
#endif

#define MAXIMAGECACHE 1000  // Max number of items in image cache



using namespace std;



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

  // Get our image pattern variable
  string filesystem_prefix = Environment::getFileSystemPrefix();

  // Get our image pattern variable
  string filename_pattern = Environment::getFileNamePattern();

  // Timestamp of cached image
  time_t timestamp = 0;


  // Put the image setup into a try block as object creation can throw an exception
  try{

    // Check whether cache is empty
    if( session->imageCache->empty() ){
      if( session->loglevel >= 1 ) *(session->logfile) << "FIF :: Image cache initialization" << endl;
      test = IIPImage( argument );
      test.setFileNamePattern( filename_pattern );
      test.setFileSystemPrefix( filesystem_prefix );
      test.Initialise();
    }
    // If not, look up our object
    else{
      // Cache Hit
      if( session->imageCache->find(argument) != session->imageCache->end() ){
	test = (*session->imageCache)[ argument ];
	timestamp = test.timestamp;       // Record timestamp if we have a cached image
	if( session->loglevel >= 2 ){
	  *(session->logfile) << "FIF :: Image cache hit. Number of elements: " << session->imageCache->size() << endl;
	}
      }
      // Cache Miss
      else{
	if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: Image cache miss" << endl;
	test = IIPImage( argument );
	test.setFileNamePattern( filename_pattern );
	test.setFileSystemPrefix( filesystem_prefix );
	test.Initialise();
	// Delete items if our list of images is too long.
	if( session->imageCache->size() >= MAXIMAGECACHE ) session->imageCache->erase( session->imageCache->begin() );
      }
    }



    /***************************************************************
      Test for different image types - only TIFF is native for now
    ***************************************************************/

    ImageFormat format = test.getImageFormat();

    if( format == TIF ){
      if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: TIFF image detected" << endl;
      *session->image = new TPTImage( test );
    }
#if defined(HAVE_KAKADU) || defined(HAVE_OPENJPEG)
    else if( format == JPEG2000 ){
      if( session->loglevel >= 2 )
        *(session->logfile) << "FIF :: JPEG2000 image detected" << endl;
#if defined(HAVE_OPENJPEG)
      if( session->useOpenJPEG )
        *session->image = new OpenJPEGImage( test );
#endif
#if defined(HAVE_KAKADU)
#if defined(HAVE_OPENJPEG)
      else
#endif
        *session->image = new KakaduImage( test );
#endif
    }
#endif
    else throw string( "Unsupported image type: " + argument );

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


    // Open image and update timestamp
    (*session->image)->openImage();

    // Check timestamp consistency. If cached timestamp is older, update metadata
    if( timestamp>0 && (timestamp < (*session->image)->timestamp) ){
      if( session->loglevel >= 2 ){
	*(session->logfile) << "FIF :: Image timestamp changed: reloading metadata" << endl;
      }
      (*session->image)->loadImageInfo( (*session->image)->currentX, (*session->image)->currentY );
    }

    // Add this image to our cache, overwriting previous version if it exists
    (*session->imageCache)[argument] = *(*session->image);

    if( session->loglevel >= 3 ){
      *(session->logfile) << "FIF :: Created image" << endl;
    }


    // Set the timestamp for the reply
    session->response->setLastModified( (*session->image)->getTimestamp() );

    if( session->loglevel >= 2 ){
      *(session->logfile) << "FIF :: Image dimensions are " << (*session->image)->getImageWidth()
			  << " x " << (*session->image)->getImageHeight() << endl
			  << "FIF :: Image contains " << (*session->image)->channels
			  << " channel" << (((*session->image)->channels>1)?"s":"") << " with "
			  << (*session->image)->bpc << " bit" << (((*session->image)->bpc>1)?"s":"") << " per channel" << endl;
      tm *t = gmtime( &(*session->image)->timestamp );
      char strt[64];
      strftime( strt, 64, "%a, %d %b %Y %H:%M:%S GMT", t );
      *(session->logfile) << "FIF :: Image timestamp: " << strt << endl;
    }

  }
  catch( const file_error& error ){
    // Unavailable file error code is 1 3
    session->response->setError( "1 3", "FIF" );
    throw error;
  }


  // Check whether we have had an if_modified_since header. If so, compare to our image timestamp
  if( session->headers.find("HTTP_IF_MODIFIED_SINCE") != session->headers.end() ){

    tm mod_t;
    time_t t;

    strptime( (session->headers)["HTTP_IF_MODIFIED_SINCE"].c_str(), "%a, %d %b %Y %H:%M:%S %Z", &mod_t );

    // Use POSIX cross-platform mktime() function to generate a timestamp.
    // This needs UTC, but to avoid a slow TZ environment reset for each request, we set this once globally in Main.cc
    t = mktime(&mod_t);
    if( (session->loglevel >= 1) && (t == -1) ) *(session->logfile) << "FIF :: Error creating timestamp" << endl;

    if( (*session->image)->timestamp <= t ){
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

  // Reset our angle values
  session->view->xangle = 0;
  session->view->yangle = 90;


  if( session->loglevel >= 2 ){
    *(session->logfile)	<< "FIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
