/*
    IIP FCGI server module - Main loop.

    Copyright (C) 2000-2009 Ruven Pillay

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





#include <fcgiapp.h>

#include <ctime>
#include <csignal>
#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <map>
#include <sys/time.h>


#include "TPTImage.h"
#include "JPEGCompressor.h"
#include "Tokenizer.h"
#include "IIPResponse.h"
#include "View.h"
#include "Timer.h"
#include "TileManager.h"
#include "Task.h"
#include "Environment.h"
#include "Writer.h"


#ifdef ENABLE_DL
#include "DSOImage.h"
#endif





//#define DEBUG 1


using namespace std;



/* We need to define some variables globally so that the signal handler
   can have access to them
*/
int loglevel;
ofstream logfile;
unsigned long IIPcount;




/* Handle a signal - print out some stats and exit
 */
void IIPSignalHandler( int signal )
{
  if( loglevel >= 1 ){

#ifdef HAVE_TIME_H	
	time_t current_time = time( NULL );
	char *date = ctime( &current_time );
#else
	char *date = "Today";
#endif

    logfile << endl << "Caught signal " << signal << ". "
	    << "Terminating after " << IIPcount << " accesses" << endl
	    << date
	    << "<----------------------------------->" << endl << endl;
    logfile.close();
  }

  exit( 1 );
}





int main( int argc, char *argv[] )
{

  IIPcount = 0;
  int i;


  // Define ourselves a version
#ifdef VERSION
  string version = string( VERSION );
#else
  string version = string( "0.9.9.9" );
#endif




  /*************************************************
    Initialise some variables from our environment
  *************************************************/


  //  Check for a verbosity env variable and open an appendable logfile
  //  if we want logging ie loglevel >= 0

  loglevel = Environment::getVerbosity();

  if( loglevel >= 1 ){

    // Check for the requested log file path
    string lf = Environment::getLogFile();

    logfile.open( lf.c_str(), ios::app );
    // If we cannot open this, set the loglevel to 0
    if( !logfile ){
      loglevel = 0;
    }

    // Put a header marker and credit in the file
    else{

      // Get current time if possible
#ifdef HAVE_TIME_H	
      time_t current_time = time( NULL );
      char *date = ctime( &current_time );
#else
      char *date = "Today";
#endif

      logfile << "<----------------------------------->" << endl
	      << date << endl
	      << "IIPImage Server. Version " << version << endl
	      << "*** Ruven Pillay <ruven@users.sourceforge.net> ***" << endl << endl
	      << "Verbosity level set to " << loglevel << endl;
    }

  }



  // Set up some FCGI items and make sure we are in FCGI mode

#ifndef DEBUG

  FCGX_Request request;
  int listen_socket = 0;

  if( argv[1] && (string(argv[1]) == "--standalone") ){
    string socket = argv[2];
    if( !socket.length() ){
      logfile << "No socket specified" << endl << endl;
      exit(1);
    }
    listen_socket = FCGX_OpenSocket( socket.c_str(), 10 );
    if( listen_socket < 0 ){
      logfile << "Unable to open socket '" << socket << "'" << endl << endl;
      exit(1);
    }
  }

  if( FCGX_InitRequest( &request, listen_socket, 0 ) ) return(1);

  if( FCGX_IsCGI() ){
    if( loglevel >= 1 ) logfile << "CGI-only mode detected. Terminating" << endl << endl;
    return( 1 );
  }
  else{
    if( loglevel >= 1 ) logfile << "Running in FCGI mode" << endl << endl;
  }

#endif


  // Set our maximum image cache size
  float max_image_cache_size = Environment::getMaxImageCacheSize();
  imageCacheMapType imageCache;


  // Get our image pattern variable
  string filename_pattern = Environment::getFileNamePattern();


  //  Get our default quality variable
  int jpeg_quality = Environment::getJPEGQuality();


  //  Get our max CVT size
  int max_CVT = Environment::getMaxCVT();


  // Get the default number of quality layers to decode
  int layers = Environment::getLayers();


  if( loglevel >= 1 ){
    logfile << "Setting maximum image cache size to " << max_image_cache_size << "MB" << endl;
    logfile << "Setting 3D file sequence name pattern to " << filename_pattern << endl;
    logfile << "Setting default JPEG quality to " << jpeg_quality << endl;
    logfile << "Setting maximum CVT size to " << max_CVT << endl;
    logfile << "Setting default decoded quality layers (for supported file formats) to " << layers << endl;
  }




  /***********************************************************
    Check for loadable modules - only if enabled by configure
  ***********************************************************/

#ifdef ENABLE_DL

  map <string, string> moduleList;
  string modulePath;
  envpara = getenv( "DECODER_MODULES" );

  if( envpara ){

    modulePath = string( envpara );

    // Try to open the module

    Tokenizer izer( modulePath, "," );
  
    while( izer.hasMoreTokens() ){
      
      try{
	string token = izer.nextToken();
	DSOImage module;
	module.Load( token );
	string type = module.getImageType();
	if( loglevel >= 1 ){
	  logfile << "Loading external module: " << module.getDescription() << endl;
	}
	moduleList[ type ] = token;
      }
      catch( const string& error ){
	if( loglevel >= 1 ) logfile << error << endl;
      }

    }
    
    // Tell us what's happened
    if( loglevel >= 1 ) logfile << moduleList.size() << " external modules loaded" << endl;

  }

#endif





  /***********************************************************
    Set up a signal handler for USR1, TERM and SIGHUP signals
    - to simplify things, they can all just shutdown the
      server. We can rely on mod_fastcgi to restart us.
    - SIGUSR1 and SIGHUP don't exist on Windows, though. 
  ***********************************************************/

#ifndef WIN32
  signal( SIGUSR1, IIPSignalHandler );
  signal( SIGHUP, IIPSignalHandler );
#endif

  signal( SIGTERM, IIPSignalHandler );




  if( loglevel >= 1 ){
    logfile << endl << "Initialisation Complete." << endl
	    << "<----------------------------------->"
	    << endl << endl;
  }


  // Set up some timers and create our tile cache
  Timer request_timer;
  Cache tileCache( max_image_cache_size );
  Task* task = NULL;



  /****************
    Main FCGI loop
  ****************/

#ifdef DEBUG
  int status = true;
  while( status ){

    FILE *f = fopen( "test.jpg", "w" );
    FileWriter writer( f );
    status = false;

#else

  while( FCGX_Accept_r( &request ) >= 0 ){

    FCGIWriter writer( request.out );

#endif


    // Time each request
    if( loglevel >= 2 ) request_timer.start();


    // Declare our image pointer here outside of the try scope
    //  so that we can close the image on exceptions
    IIPImage *image = NULL;
    JPEGCompressor jpeg( jpeg_quality );


    // View object for use with the CVT command etc
    View view;
    if( max_CVT != -1 ){
      view.setMaxSize( max_CVT );
      if( loglevel >= 2 ) logfile << "CVT maximum viewport size set to " << max_CVT << endl;
    }
    view.setLayers( layers );


    // Create an IIPResponse object - we use this for the OBJ requests.
    // As the commands return images etc, they handle their own responses.
    IIPResponse response;


    try{
      
      // Get the query into a string
#ifdef DEBUG
      string request_string = argv[1];
#else
      string request_string = FCGX_GetParam( "QUERY_STRING", request.envp );
#endif

      // Check that we actually have a request string
      if( request_string.length() == 0 ) {
	throw string( "QUERY_STRING not set" );
      }

      if( loglevel >=2 ){
	logfile << "Full Request is " << request_string << endl;
      }


      // Set up our session data object
      Session session;
      session.image = &image;
      session.response = &response;
      session.view = &view;
      session.jpeg = &jpeg;
      session.loglevel = loglevel;
      session.logfile = &logfile;
      session.imageCache = &imageCache;
      session.tileCache = &tileCache;
      session.out = &writer;


      // Parse up the command list

      list < pair<string,string> > requests;
      list < pair<string,string> > :: const_iterator commands;

      Tokenizer izer( request_string, "&" );
      while( izer.hasMoreTokens() ){
	pair <string,string> p;
	string token = izer.nextToken();
	int n = token.find_first_of( "=" );
	p.first = token.substr( 0, n );
	p.second = token.substr( n+1, token.length() );
	if( p.first.length() && p.second.length() ) requests.push_back( p );
      }


      i = 0;
      for( commands = requests.begin(); commands != requests.end(); commands++ ){

	string command = (*commands).first;
	string argument = (*commands).second;

	if( loglevel >= 2 ){
	  logfile << "[" << i+1 << "/" << requests.size() << "]: Command / Argument is " << command << " : " << argument << endl;
	  i++;
	}

	task = Task::factory( command );
	if( task ) task->run( &session, argument );

	if( !task ){
	  if( loglevel >= 1 ) logfile << "Unsupported command: " << command << endl;
	  // Unsupported command error code is 2 2
	  response.setError( "2 2", command );
	}


	// Delete our task
	if( task ){
	  delete task;
	  task = NULL;
	}

      }



      ////////////////////////////////////////////////////////
      ////////// Send out our Errors if necessary ////////////
      ////////////////////////////////////////////////////////

      /* Make sure something has actually been sent to the client
	 If no response has been sent by now, we must have a malformed command
       */
      if( (!response.imageSent()) && (!response.isSet()) ){
	// Malformed command syntax error code is 2 1
	response.setError( "2 1", request_string );
      }


      /* Once we have finished parsing all our OBJ and COMMAND requests
	 send out our response.
       */
      if( response.isSet() ){
	if( loglevel >= 4 ){
	  logfile << "---" << endl <<
	    response.formatResponse() <<
	    endl << "---" << endl;
	}
	if( writer.putS( response.formatResponse().c_str() ) == -1 ){
	  if( loglevel >= 1 ) logfile << "Error sending IIPResponse" << endl;
	}
      }



      //////////////////////////////////////////////////////
      //////////////// End of try block ////////////////////
      //////////////////////////////////////////////////////
    }

    catch( const string& error ){

      if( loglevel >= 1 ){
	logfile << error << endl << endl;
      }

      if( response.errorIsSet() ){
	if( loglevel >= 4 ){
	  logfile << "---" << endl <<
	    response.formatResponse() <<
	    endl << "---" << endl;
	}
	if( writer.putS( response.formatResponse().c_str() ) == -1 ){
	  if( loglevel >= 1 ) logfile << "Error sending IIPResponse" << endl;
	}
      }
      else{
	/* Display our advertising banner ;-)
	 */
	writer.putS( response.getAdvert( version ).c_str() );
      }

    }

    /* Default catch
     */
    catch( ... ){

      if( loglevel >= 1 ){
	logfile << "Error: Default Catch: " << endl << endl;
      }

      /* Display our advertising banner ;-)
       */
      writer.putS( response.getAdvert( version ).c_str() );

    }


    /* Do some cleaning up etc. here after all the potential exceptions
       have been handled
     */
    if( task ) delete task;
    delete image;
    image = NULL;
    IIPcount ++;

#ifdef DEBUG
    fclose( f );
#endif

    // How long did this request take?
    if( loglevel >= 2 ){
      logfile << "Total Request Time: " << request_timer.getTime() << " microseconds" << endl;
    }


    if( loglevel >= 2 ){
      logfile << "image closed and deleted" << endl
	      << "Server count is " << IIPcount << endl << endl;
      
    }



    ///////// End of FCGI_ACCEPT while loop or for loop in debug mode //////////
  }



  if( loglevel >= 1 ){
    logfile << endl << "Terminating after " << IIPcount << " iterations" << endl;
    logfile.close();
  }

  return( 0 );

}
