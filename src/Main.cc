/*
    IIP FCGI server module - Main loop.

    Copyright (C) 2000-2025 Ruven Pillay

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






#include <ctime>
#include <csignal>
#include <string>
#include <utility>
#include <map>
#include <algorithm>
#ifdef DEBUG
#include <iostream>
#endif

#include "TPTImage.h"
#include "JPEGImage.h"
#include "Tokenizer.h"
#include "IIPResponse.h"
#include "View.h"
#include "Timer.h"
#include "TileManager.h"
#include "Task.h"
#include "Environment.h"
#include "Writer.h"
#include "Logger.h"
#ifdef HAVE_KAKADU
#include "KakaduImage.h"
#endif
#ifdef HAVE_OPENJPEG
#include "OpenJPEGImage.h"
#endif

#ifdef HAVE_MEMCACHED
//#ifdef WIN32
//#include "../windows/MemcachedWindows.h"
//#else
#include "Memcached.h"
//#endif
#endif

#ifdef ENABLE_DL
#include "DSOImage.h"
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

// If necessary, define missing setenv and unsetenv functions
#ifndef HAVE_SETENV
static void setenv(char *n, char *v, int x) {
  char buf[256];
  snprintf(buf,sizeof(buf),"%s=%s",n,v);
  putenv(buf);
}
static void unsetenv(char *env_name) {
  extern char **environ;
  char **cc;
  int l;
  l=strlen(env_name);
  for (cc=environ;*cc!=NULL;cc++) {
    if (strncmp(env_name,*cc,l)==0 && ((*cc)[l]=='='||(*cc)[l]=='\0')) break;
  } for (; *cc != NULL; cc++) *cc=cc[1];
}
#endif


// Define our default socket backlog
#define DEFAULT_BACKLOG 2048


using namespace std;



/* We need to define some variables globally so that the signal handler
   can have access to them
*/
int loglevel;
Logger logfile;
unsigned long IIPcount;



// Create pointers to our cache structures for use in our signal handler function
imageCacheMapType* ic = NULL;
Cache* tc = NULL;


void IIPReloadCache( int signal )
{
  if( ic ) ic->clear();
  if( tc ) tc->clear();

  if( loglevel >= 1 ){
    // No strsignal on Windows
#ifdef WIN32
    int sigstr = signal;
#else
    const char *sigstr = strsignal( signal );
#endif
    logfile << "Caught " << sigstr << " signal. Emptying internal caches" << endl << endl;
  }
}



/* Handle a termination signal - print out some stats and exit
 */
void IIPSignalHandler( int signal )
{
  if( loglevel >= 1 ){

    time_t current_time = time( NULL );
    char *date = ctime( &current_time );

    // Remove trailing newline
    date[strcspn(date, "\n")] = '\0';

    // No strsignal on Windows
#ifdef WIN32
    int sigstr = signal;
#else
    const char *sigstr = strsignal( signal );
#endif

    logfile << endl << "Caught " << sigstr << " signal. "
	    << "Terminating after " << IIPcount << " accesses" << endl
	    << date << endl
	    << "<----------------------------------->" << endl << endl;
    logfile.close();
  }

  exit( 0 );
}





int main( int argc, char *argv[] )
{

  IIPcount = 0;
  int i;


  // Define ourselves a version
  string version = string( VERSION );



  /*************************************************
    Initialise some variables from our environment
  *************************************************/


  //  Check for a verbosity env variable and open an appendable logfile
  //  if we want logging ie loglevel >= 0
  loglevel = Environment::getVerbosity();

  if( loglevel >= 1 ){

    // Check for the requested log file path
    string lf = Environment::getLogFile();
    logfile.open( lf );

    // If we cannot open this, set the loglevel to 0
    if( !logfile ){
      loglevel = 0;
    }

    // Put a header marker and credit in the file
    else{

      // Get current time
      time_t current_time = time( NULL );
      const char *date = ctime( &current_time );

      logfile << "<----------------------------------->" << endl
	      << date << endl
	      << "IIPImage Server. Version " << version << endl
	      << "*** Ruven Pillay <ruven@users.sourceforge.net> ***" << endl << endl
	      << "Verbosity level set to " << loglevel << endl;
    }

  }


#ifdef DEBUG
  if( loglevel >= 1 ) logfile << "DEBUG mode: listening to input on command line and sending output to 'iipsrv.debug'" << endl << endl;
#else

  // Set up our FCGI connnection

  FCGX_Request request;
  int listen_socket = 0;
  bool standalone = false;


  // Initialize FCGI library
  if( FCGX_Init() ) return( 1 );


  // Check if we're running directly from the command line
  if( argv[1] && (string(argv[1]) == "--bind") ){
    string socket = argv[2];
    if( !socket.length() ){
      if( loglevel >= 1 ) logfile << "No socket specified" << endl << endl;
      exit(1);
    }
    int backlog = DEFAULT_BACKLOG;
    if( argv[3] && (string(argv[3]) == "--backlog") ){
      string bklg = argv[4];
      if( bklg.length() ) backlog = atoi( bklg.c_str() );
    }
    listen_socket = FCGX_OpenSocket( socket.c_str(), backlog );
    if( listen_socket < 0 ){
      if( loglevel >= 1 ) logfile << "Unable to open socket '" << socket << "'" << endl << endl;
      exit(1);
    }
    standalone = true;
    if( loglevel >= 1 ) logfile << "Running in standalone mode on socket: " << socket << " with backlog: " << backlog << endl << endl;
  }


  // Initialize FCGI request
  if( FCGX_InitRequest( &request, listen_socket, 0 ) ) return( 1 );


  // Check whether we really are in FCGI mode - only if we are not in standalone mode
  if( FCGX_IsCGI() ){
    if( !standalone ){
      if( loglevel >= 1 ) logfile << "CGI-only mode detected" << endl << endl;
      return( 1 );
    }
  }
  else{
    if( loglevel >= 1 ) logfile << "Running in FCGI mode" << endl << endl;
  }

#endif


  // Set our maximum image cache size
  float max_image_cache_size = Environment::getMaxImageCacheSize();


  // Get our maximum metadata cache size
  FIF::max_metadata_cache_size = Environment::getMaxMetadataCacheSize();
  imageCacheMapType imageCache;
  ic = &imageCache;


  // Get our image pattern variable
  FIF::filename_pattern = Environment::getFileNamePattern();


  // Get the filesystem prefix if any
  FIF::filesystem_prefix = Environment::getFileSystemPrefix();;


  // Get the filesystem suffix if any
  FIF::filesystem_suffix = Environment::getFileSystemSuffix();


  // Get our default quality variable
  int jpeg_quality = Environment::getJPEGQuality();


  // Get our default PNG compression level
  int png_quality = Environment::getPNGQuality();


  // Get our default WebP compression level
  int webp_quality = Environment::getWebPQuality();


  // Get our default AVIF compression level
  int avif_quality = Environment::getAVIFQuality();


  // Get our requested AVIF codec
  unsigned int avif_codec = Environment::getAVIFCodec();


  // Get our default TIFF compression scheme
  int tiff_compression = Environment::getTIFFCompression();
  int tiff_quality = Environment::getTIFFQuality();


  // Get our max CVT size
  int max_CVT = Environment::getMaxCVT();


  // Get the default number of quality layers to decode
  int max_layers = Environment::getMaxLayers();


  // Set up our watermark object
  Watermark watermark( Environment::getWatermark(),
		       Environment::getWatermarkOpacity(),
		       Environment::getWatermarkProbability() );


  // Get the CORS setting
  string cors = Environment::getCORS();


  // Get any Base URL setting
  string base_url = Environment::getBaseURL();


  // Get requested HTTP Cache-Control setting
  string cache_control = Environment::getCacheControl();


  // Get URI mapping if we are not using query strings
  string uri_map_string = Environment::getURIMap();
  map<string,string> uri_map;


  // Get the allow upscaling setting
  bool allow_upscaling = Environment::getAllowUpscaling();


  // Get the max ICC profile size we allow to be embedded
  int max_icc = Environment::getMaxICC();


  // Get codec passthrough setting
  IIPImage::codec_passthrough = Environment::getCodecPassthrough();


  // Set our IIIF version
  IIIF::version = Environment::getIIIFVersion();


  // Set our IIIF multi-page delimiter
  IIIF::delimiter = Environment::getIIIFDelimiter();


  // Add extra IIIF info JSON fields
  IIIF::extra_info = Environment::getIIIFExtraInfo();


  // Set global rights/copyright
  string copyright = Environment::getCopyright();


  // Create our image processing engine
  Transform processor;


  // Setup codec logging only once for TIFF and Kakadu
  // Note that OpenJPEG requires codec to be initialized before setting logging functions, so cannot do statically here
  if( loglevel > 2 ) IIPImage::logging = true;
  TPTImage::setupLogging();

#ifdef HAVE_KAKADU
  // Get the Kakadu readmode
  unsigned int kdu_readmode = Environment::getKduReadMode();
  // Setup codec logging
  KakaduImage::setupLogging();
#endif



  // Print out some information
  if( loglevel >= 1 ){
    logfile << "Setting maximum image tile data cache size to " << max_image_cache_size << "MB" << endl;

    logfile << "Setting maximum image metadata cache size to ";
    if( FIF::max_metadata_cache_size == -1 ) logfile << "-1 (unlimited) images" << endl;
    else logfile << FIF::max_metadata_cache_size << " images" << endl;

    logfile << "Setting filesystem prefix to '" << FIF::filesystem_prefix << "'" << endl;
    logfile << "Setting filesystem suffix to '" << FIF::filesystem_suffix << "'" << endl;
    logfile << "Setting default TIFF output compression/quality to "
	    << TIFFCompressor::getCompressionName(tiff_compression) << "/" << tiff_quality << endl;
    logfile << "Setting default JPEG output quality to " << jpeg_quality << endl;
#ifdef HAVE_PNG
    logfile << "Setting default PNG output compression level to " << png_quality << endl;
#endif
#ifdef HAVE_WEBP
    logfile << "Setting default WebP output compression level to ";
    if( webp_quality == -1 ) logfile << "lossless" << endl;
    else logfile << webp_quality << endl;
#endif
#ifdef HAVE_AVIF
    logfile << "Setting default AVIF compression level to ";
    if( avif_quality == -1 ) logfile << "lossless" << endl;
    else logfile << avif_quality << endl;
    logfile << "Setting AVIF codec to " << AVIFCompressor::getCodecName( avif_codec ) << endl;
#endif
    logfile << "Setting maximum CVT size to " << max_CVT << endl;
    logfile << "Setting HTTP Cache-Control header to '" << cache_control << "'" << endl;
    logfile << "Setting 3D file sequence name pattern to '" << FIF::filename_pattern << "'" << endl;
    logfile << "Setting default IIIF Image API version to " << IIIF::version << endl;
    if( IIIF::delimiter.size() ){
      logfile << "Setting default IIIF multi-page delimiter to '" << IIIF::delimiter << "'" << endl;
    }
    if( !cors.empty() ) logfile << "Setting Cross Origin Resource Sharing to '" << cors << "'" << endl;
    if( !base_url.empty() ) logfile << "Setting base URL to '" << base_url << "'" << endl;
    if( max_layers != 0 ){
      logfile << "Setting max quality layers (for supported file formats) to ";
      if( max_layers < 0 ) logfile << "all layers" << endl;
      else logfile << max_layers << endl;
    }
    logfile << "Setting Allow Upscaling to " << (allow_upscaling? "true" : "false") << endl;
    logfile << "Setting maximum ICC profile size to ";
    if( max_icc < 0 ) logfile << "unlimited" << endl;
    else logfile << max_icc << " bytes" << endl;
    logfile << "Setting codec passthrough to " << (IIPImage::codec_passthrough? "true" : "false") << endl;
    if( !copyright.empty() ) logfile << "Setting default rights/copyright statement to '" << copyright << "'" << endl;
    logfile << "Setting up TIFF support via " << TPTImage::getCodecVersion() << endl;
    logfile << "Setting up JPEG support via " << JPEGImage::getCodecVersion() << endl;
#ifdef HAVE_KAKADU
    logfile << "Setting up JPEG2000 support via Kakadu SDK " << KakaduImage::getCodecVersion() << endl;
    logfile << "Setting Kakadu read-mode to " << ((kdu_readmode==2) ? "resilient" : (kdu_readmode==1) ? "fussy" : "fast") << endl;
#elif defined(HAVE_OPENJPEG)
    logfile << "Setting up JPEG2000 support via OpenJPEG " << OpenJPEGImage::getCodecVersion() << endl;
#endif
    logfile << "Setting image processing engine to " << processor.getDescription() << endl;
#ifdef _OPENMP
    int num_threads = 0;
#pragma omp parallel
    {
      num_threads = omp_get_num_threads();
    }
    if( num_threads > 1 ) logfile << "OpenMP enabled for parallelized image processing with " << num_threads << " threads" << endl;
#endif
  }


  // Setup our URI mapping for non-CGI requests
  if( !uri_map_string.empty() ){

    // Check map is well-formed: maps must be of the form "prefix=>protocol"
    size_t pos;
    if( (pos = uri_map_string.find("=>")) != string::npos ){

      // Extract protocol
      string prefix = uri_map_string.substr( 0, pos );
      string protocol = uri_map_string.substr( pos+2 );
      bool supported_protocol = false;

      // Make sure the command is one of our supported protocols: "IIP", "IIIF", "Zoomify", "DeepZoom"
      string prtcl = protocol;
      transform( prtcl.begin(), prtcl.end(), prtcl.begin(), ::tolower );
      if( prtcl == "iip" || prtcl == "iiif" || prtcl == "zoomify" || prtcl == "deepzoom" ){
	supported_protocol = true;
      }

      if( loglevel > 0 ){
	logfile << "Setting URI mapping to " << uri_map_string << ". "
		<< ((supported_protocol)?"S":"Uns") << "upported protocol: " << protocol << endl;
      }

      // IIP protocol requires "FIF" as first argument
      if( prtcl == "iip" ) prtcl = "fif";

      // Initialize our map
      if( supported_protocol ) uri_map[prefix] = prtcl;
    }
    else if( loglevel > 0 ) logfile << "Malformed URI map: " << uri_map_string << endl;

  }
  

  // Try to load our watermark
  if( watermark.getImage().length() > 0 ){
    watermark.init();
    if( loglevel >= 1 ){
      if( watermark.isSet() ){
	logfile << "Loaded watermark image '" << watermark.getImage()
		<< "': setting probability to " << watermark.getProbability()
		<< " and opacity to " << watermark.getOpacity() << endl;
      }
      else{
	logfile << "Unable to load watermark image '" << watermark.getImage() << "'" << endl;
      }
    }
  }


#ifdef HAVE_MEMCACHED

  // Get our list of memcached servers if we have any and the timeout
  string memcached_servers = Environment::getMemcachedServers();
  unsigned int memcached_timeout = Environment::getMemcachedTimeout();

  // Create our memcached object
  Memcache memcached( memcached_servers, memcached_timeout );
  if( loglevel >= 1 ){
    if( memcached.connected() ){
      logfile << "Memcached support enabled. Connected to servers: '" << memcached_servers
	      << "' with timeout " << memcached_timeout << endl;
    }
    else logfile << "Unable to connect to Memcached servers: '" << memcached.error() << "'" << endl;
  }

#endif



  // Add a new line
  if( loglevel >= 1 ) logfile << endl;


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
    Set up a signal handler for USR1, TERM, HUP and INT signals
    - to simplify things, they can all just shutdown the
      server. We can rely on mod_fastcgi to restart us.
    - SIGUSR1 and SIGHUP don't exist on Windows, though. 
  ***********************************************************/

#ifndef WIN32
  signal( SIGUSR1, IIPSignalHandler );
  signal( SIGHUP, IIPReloadCache );
#endif

  signal( SIGTERM, IIPSignalHandler );
  signal( SIGINT, IIPSignalHandler );



  if( loglevel >= 1 ){
    logfile << endl << "Initialisation Complete." << endl
	    << "<----------------------------------->"
	    << endl << endl;
  }


  // Set up our request timers and seed our random number generator with the millisecond count from it
  Timer request_timer;
  srand( request_timer.getTime() );

  // Create our tile cache
  Cache tileCache( max_image_cache_size );
  tc = &tileCache;

  // Declare our task object and request string
  Task* task = NULL;
  string request_string;


  /********************
    Main Request Loop
  ********************/

#ifdef DEBUG

  // When in debug mode, listen for requests on standard in and output to a file
  while( getline( cin, request_string ) ){

    FILE *f = fopen( "iipsrv.debug", "w" );
    if( f == NULL ) exit( 1 );
    FileWriter writer( f );

#else

  // In FCGI mode, listen for FCGI requests
  while( FCGX_Accept_r( &request ) >= 0 ){

    FCGIWriter writer( request.out );
    request_string.clear();

#endif


    // Time each request
    if( loglevel >= 2 ) request_timer.start();


    // Declare our image pointer here outside of the try scope
    //  so that we can close the image on exceptions
    IIPImage *image = NULL;
    JPEGCompressor jpeg( jpeg_quality );
    TIFFCompressor tiff( tiff_compression, tiff_quality );
#ifdef HAVE_PNG
    PNGCompressor png( png_quality );
#endif
#ifdef HAVE_WEBP
    WebPCompressor webp( webp_quality );
#endif
#ifdef HAVE_AVIF
    AVIFCompressor avif( avif_quality );
    avif.setCodec( avif_codec );
#endif

    // View object for use with the CVT command etc
    View view;
    if( max_CVT != 0 ) view.setMaxSize( max_CVT );
    if( max_layers != 0 ) view.setMaxLayers( max_layers );
    view.setAllowUpscaling( allow_upscaling );
    view.setMaxICC( max_icc );


    // Create an IIPResponse object - we use this for the OBJ requests.
    // As the commands return images etc, they handle their own responses.
    IIPResponse response;
    response.setCORS( cors );
    response.setCacheControl( cache_control );


    try{

      // Set up our session data object
      Session session;
      session.image = &image;
      session.response = &response;
      session.view = &view;
      session.jpeg = &jpeg;
      session.tiff = &tiff;
#ifdef HAVE_PNG
      session.png = &png;
#endif
#ifdef HAVE_WEBP
      session.webp = &webp;
#endif
#ifdef HAVE_AVIF
      session.avif = &avif;
#endif
      session.loglevel = loglevel;
      session.logfile = &logfile;
      session.imageCache = &imageCache;
      session.tileCache = &tileCache;
      session.out = &writer;
      session.watermark = &watermark;
      session.headers.clear();
      session.processor = &processor;
#ifdef HAVE_KAKADU
      session.codecOptions["KAKADU_READMODE"] = kdu_readmode;
#endif


#ifndef DEBUG

      char* header = NULL;

      // If we have a URI prefix mapping, first test for a match between the map prefix string
      //  and the full REQUEST_URI variable
      if( !uri_map.empty() ){

	string prefix = uri_map.begin()->first;
	string command = uri_map.begin()->second;

	header = FCGX_GetParam( "REQUEST_URI", request.envp );
	const string request_uri = (header!=NULL) ? header : "";

	// Try to find the prefix at the beginning of request URI
	// Note that the first character will always be "/"
	size_t len = prefix.length();
	if( (len==0) || (request_uri.find(prefix)==1) ){
	  // This is indeed a mapped request, so map our prefix with the appropriate protocol
	  unsigned int start = (len>0) ? len+2 : 1; // Add 2 to remove both leading and trailing slashes
	  // Strip out any query string if we are in prefix mode
	  size_t q = request_uri.find_first_of('?');
	  unsigned int end = (q==string::npos) ? request_uri.length() : q;
	  request_string = command + "=" + request_uri.substr( start, end-start );
	  if( loglevel >= 2 ) logfile << "Request URI mapped to " << request_string << endl;
	}
      }


      // If the request string hasn't been set through a URI map, get it from the QUERY_STRING variable
      if( request_string.empty() ){

	// Get the query into a string
	header = FCGX_GetParam( "QUERY_STRING", request.envp );
	request_string = (header!=NULL)? header : "";

	header = FCGX_GetParam( "REQUEST_METHOD", request.envp );
	session.headers["REQUEST_METHOD"] = header;

	// Handle OPTIONS request
	if( session.headers["REQUEST_METHOD"] == "OPTIONS" ){
	  if( loglevel >=2 ) logfile << "HTTP OPTIONS request" << endl;
	  throw( 204 );
	}

	// Check for requests sent using POST, PUT or other HTTP methods
	if( request_string.empty() ){
	  int contentLength = 0;
	  if( ( header = FCGX_GetParam("CONTENT_LENGTH",request.envp) ) ) contentLength = atoi( header );
	  if( loglevel >=2 ) logfile << "HTTP " << session.headers["REQUEST_METHOD"] << " request with contentLength " << contentLength << endl;
	  if( contentLength > 0 ){
	    char *contentBuffer = new char[contentLength];
	    FCGX_GetStr( contentBuffer, contentLength, request.in );
	    request_string = string( contentBuffer, contentLength );
	    delete[] contentBuffer;
	  }
	  else request_string = "";
	}
      }

      // Check that we actually have a request string. If not, just show server home page
      if( request_string.empty() ){
	response.setStatus( "200 OK" );
	throw string( "QUERY_STRING not set" );
      }

      if( loglevel >=2 ){
	logfile << "Full Request is " << request_string << endl;
      }


      // Get several important HTTP headers
      if( (header = FCGX_GetParam("SERVER_PROTOCOL", request.envp)) ){
        session.headers["SERVER_PROTOCOL"] = string(header);
      }
      if( (header = FCGX_GetParam("HTTP_HOST", request.envp)) ){
        session.headers["HTTP_HOST"] = string(header);
      }
      if( (header = FCGX_GetParam("REQUEST_URI", request.envp)) ){
        session.headers["REQUEST_URI"] = string(header);
      }
      if( (header = FCGX_GetParam("HTTPS", request.envp)) ) {
        session.headers["HTTPS"] = string(header);
      }
      if( (header = FCGX_GetParam("HTTP_ACCEPT", request.envp)) ){
	session.headers["HTTP_ACCEPT"] = string(header);
      }
      if( (header = FCGX_GetParam("HTTP_X_IIIF_ID", request.envp)) ){
        session.headers["HTTP_X_IIIF_ID"] = string(header);
      }

      // Check for IF_MODIFIED_SINCE
      if( (header = FCGX_GetParam("HTTP_IF_MODIFIED_SINCE", request.envp)) ){
	session.headers["HTTP_IF_MODIFIED_SINCE"] = string(header);
	if( loglevel >= 2 ){
	  logfile << "HTTP Header: If-Modified-Since: " << header << endl;
	}
      }
#endif


      // Store some key session information not necessarily found in HTTP headers
      session.headers["QUERY_STRING"] = request_string;
      if( !base_url.empty() ) session.headers["BASE_URL"] = base_url;
      if( !copyright.empty() ) session.headers["COPYRIGHT"] = copyright;


#ifdef HAVE_MEMCACHED
#ifndef DEBUG
      // Check whether this exists in memcached, but only if we haven't had an if_modified_since
      // request, which should always be faster to send
      if( !header || session.headers["HTTP_IF_MODIFIED_SINCE"].empty() ){
	char* memcached_response = NULL;
	if( (memcached_response = memcached.retrieve( request_string )) ){
	  writer.putStr( memcached_response, memcached.length() );
	  writer.flush();
	  free( memcached_response );
	  throw( 100 );
	}
      }
#endif
#endif


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


      ////////////////////////////////////////////////////////
      ////////// Insert the result into Memcached  ///////////
      ////////// - Note that we never store errors ///////////
      //////////   or 304 replies                  ///////////
      ////////////////////////////////////////////////////////

#ifdef HAVE_MEMCACHED
#ifndef DEBUG
      if( response.cachable() && memcached.connected() ){
	Timer memcached_timer;
	memcached_timer.start();
	memcached.store( session.headers["QUERY_STRING"], writer.buffer, writer.sz );
	if( loglevel >= 3 ){
	  logfile << "Memcached :: stored " << writer.sz << " bytes in "
		  << memcached_timer.getTime() << " microseconds" << endl;
	}
      }
#endif
#endif


      //////////////////////////////////////////////////////
      //////////////// End of try block ////////////////////
      //////////////////////////////////////////////////////
    }

    /* Use this for sending various HTTP status codes
     */
    catch( const int& code ){

      string status;

      switch( code ){

        case 304:
	  status = "Status: 304 Not Modified\r\nServer: iipsrv/" + version + "\r\n\r\n";
	  writer.putS( status.c_str() );
	  writer.flush();
          if( loglevel >= 2 ){
	    logfile << "Sending HTTP 304 Not Modified" << endl;
	  }
	  break;

        case 204:
	  // Handle HTTP OPTIONS requests
	  status = "Status: 204 No Content\r\nServer: iipsrv/" + version + "\r\nX-Powered-By: IIPImage\r\n" +
	    "Content-Length: 0\r\n" + response.getCORS() + "\r\n\r\n";
	  writer.putS( status.c_str() );
	  writer.flush();
	  if( loglevel >= 2 ){
	    logfile << "Returning HTTP 204 No Content" << endl;
	  }
	  break;

        case 100:
	  if( loglevel >= 2 ){
	    logfile << "Memcached hit" << endl;
	  }
	  break;

        default:
          if( loglevel >= 1 ){
	    logfile << "Unsupported HTTP status code: " << code << endl << endl;
	  }
       }
    }

    /* Catch any errors
     */
    catch( const string& error ){

      if( loglevel >= 1 ){
	logfile << endl << error << endl << endl;
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
	// Display our advertising banner ;-)
	if( writer.putS( response.getAdvert().c_str() ) == -1 ){
	  if( loglevel >= 1 ) logfile << "Error sending IIPImage banner" << endl;
	}
      }

    }

    // Image file errors
    catch( const file_error& error ){
      string status = "Status: 404 Not Found\r\nServer: iipsrv/" + version +
	"\r\nContent-Type: text/plain; charset=utf-8" +
	(response.getCORS().length() ? "\r\n" + response.getCORS() : "") +
	"\r\n\r\n" + error.what();
      writer.putS( status.c_str() );
      writer.flush();
      if( loglevel >= 2 ){
	logfile << error.what() << endl;
	logfile << "Sending HTTP 404 Not Found" << endl;
      }
    }

    // Parameter errors
    catch( const invalid_argument& error ){
      string status = "Status: 400 Bad Request\r\nServer: iipsrv/" + version +
	"\r\nContent-Type: text/plain; charset=utf-8" +
	(response.getCORS().length() ? "\r\n" + response.getCORS() : "") +
	"\r\n\r\n" + error.what();
      writer.putS( status.c_str() );
      writer.flush();
      if( loglevel >= 2 ){
	logfile << error.what() << endl;
	logfile << "Sending HTTP 400 Bad Request" << endl;
      }
    }

    // Memory allocation errors through std::bad_alloc
    catch( const bad_alloc& error ){
      string message = "Unable to allocate memory";
      string status = "Status: 500 Internal Server Error\r\nServer: iipsrv/" + version +
	"\r\nContent-Type: text/plain; charset=utf-8" +
	(response.getCORS().length() ? "\r\n" + response.getCORS() : "") +
	"\r\n\r\n" + message;
      writer.putS( status.c_str() );
      writer.flush();
      if( loglevel >= 1 ){
	logfile << "Error: " << message << endl;
	logfile << "Sending HTTP 500 Internal Server Error" << endl;
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
      writer.putS( response.getAdvert().c_str() );

    }


    /* Do some cleaning up etc. here after all the potential exceptions
       have been handled
     */
    if( task ){
      delete task;
      task = NULL;
    }
    delete image;
    image = NULL;
    IIPcount ++;


#ifdef DEBUG
    fclose( f );
#endif


    // How long did this request take?
    if( loglevel >= 2 ){
      logfile << "Total Request Time: " << request_timer.getTime() << " microseconds" << endl
	      << "Image closed and deleted" << endl
	      << "Server count: " << IIPcount << endl << endl;
    }



    ///////// End of FCGI_ACCEPT while loop or while loop in debug mode //////////
  }


  // Avoid dangling global pointers
  ic = NULL;
  tc = NULL;


  // Close our FCGI connection
#ifndef DEBUG
  FCGX_Finish_r( &request );
#endif


  if( loglevel >= 1 ){
    logfile << endl << "Terminating after " << IIPcount << " iterations" << endl;
    logfile.close();
  }

  return( 0 );

}
