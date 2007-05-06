#include "Task.h"
#include "Environment.h"

#include "TPTImage.h"
#include "Cache.h"


using namespace std;



void FIF::run( Session* session, std::string argument ){

  if( session->loglevel >= 3 ) *(session->logfile) << "FIF handler reached" << endl;

  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // The argument is a path, which may contain spaces or other characters
  // that will be MIME encoded into a suitable format for the URL.
  // So, first decode this path (following code modified from CommonC++ library)

  const char* source = argument.c_str();

  char destination[ 256 ];  // Hopefully we won't get any paths longer than 256 chars!
  char *dest = destination;
  char* ret = dest;
  char hex[3];

  while( *source ){
    switch( *source ){

    case '+':
      *(dest++) = ' ';
      break;

    case '%':
      // NOTE: wrong input can finish with "...%" giving
      // buffer overflow, cut string here
      if(source[1]){
	hex[0] = source[1];
	++source;
	if(source[1]){
	  hex[1] = source[1];
	  ++source;
	}
	else
	  hex[1] = 0;
      }
      else hex[0] = hex[1] = 0;

      hex[2] = 0;
      *(dest++) = (char) strtol(hex, NULL, 16);
      break;

    default:
      *(dest++) = *source;

    }
    ++source;
  }

  *dest = 0;
  argument = string( ret );


  IIPImage test;

  // Get our image pattern variable
  string filename_pattern = Environment::getFileNamePattern();


  // Put the image opening into a try block so that we can set
  // a meaningful error
  try{

    // TODO: Try to use a reference to this list, so that we can
    //  keep track of the current sequence between runs

    if( session->imageCache->empty() ){

      test = IIPImage( argument );
      test.setFileNamePattern( filename_pattern );
      test.Initialise();

      (*session->imageCache)[argument] = test;
      if( session->loglevel >= 1 ) *(session->logfile) << "Image cache initialisation" << endl;
    }

    else{

      if( session->imageCache->find(argument) != session->imageCache->end() ){
	test = (*session->imageCache)[ argument ];
	if( session->loglevel >= 2 ){
	  *(session->logfile) << "Image cache hit. Number of elements: " << session->imageCache->size() << endl;
	}
      }
      else{
	test = IIPImage( argument );
	test.setFileNamePattern( filename_pattern );
	test.Initialise();
	if( session->loglevel >= 2 ) *(session->logfile) << "Image cache miss" << endl;
	if( session->imageCache->size() >= 100 ) session->imageCache->erase( session->imageCache->end() );
	(*session->imageCache)[argument] = test;
      }
    }



    /***************************************************************
	      Test for different image types - only TIFF is native for now
    ***************************************************************/

    string imtype = test.getImageType();

    if( imtype == "tif" || imtype == "ptif" ){
      if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: TIFF image requested" << endl;
      *session->image = new TPTImage( test );
    }
    else throw string( "Unsupported image type: " + imtype );

    /*
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
	    *(session->logfile) << imtype << " image requested ... using handler "
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
      *(session->logfile) << "FIF :: created image" << endl;
    }


    (*session->image)->openImage();

    if( session->loglevel >= 2 ){
      *(session->logfile) << "image width is " << (*session->image)->getImageWidth()
			  << " and height is " << (*session->image)->getImageHeight() << endl;
    }


  }
  catch( const string& error ){
    // Unavailable file error code is 1 3
    session->response->setError( "1 3", "FIF" );
    throw error;
  }


  // Reset our angle values
  session->view->xangle = 0;
  session->view->yangle = 90;

	  
  if( session->loglevel >= 3 ){
    *(session->logfile)	<< "FIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
