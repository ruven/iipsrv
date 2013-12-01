/*
    IIP Profile Command Handler Class Member Function

    Copyright (C) 2013 Ruven Pillay.

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

#include "Task.h"
#include <cmath>
#include <sstream>

using namespace std;


/// Return the profile for a line in JSON format
void PFL::run( Session* session, const std::string& argument ){

  /* The argument should be of the form <resolution>:<x1>,<y1>-<x2>,<y2>
     1) resolution
     2) start pixel index in x direction
     3) start pixel index in y direction
     4) end pixel index in x direction
     5) end pixel index in y direction
  */

  if( session->loglevel >= 3 ) (*session->logfile) << "PFL handler reached" << endl;

  unsigned int resolution, x1, y1, x2, y2, width, height;


  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // Parse the argument list
  string arg = argument;
  int delimitter = arg.find( ":" );
  resolution = atoi( arg.substr(0,delimitter).c_str() );

  arg = arg.substr( delimitter + 1, arg.length() );
  delimitter = arg.find( "," );
  x1 = atoi( arg.substr(0,delimitter).c_str() );

  arg = arg.substr( delimitter + 1, arg.length() );
  delimitter = arg.find( "-" );
  y1 = atoi( arg.substr(0,delimitter).c_str() );

  if( delimitter == -1 ){
    (*session->logfile) << "PFL :: Single point requested" << endl;
    x2 = x1;
    y2 = y1;
  }
  else{
    arg = arg.substr( delimitter + 1, arg.length() );
    delimitter = arg.find( "," );
    x2 = atoi( arg.substr(0,delimitter).c_str() );

    arg = arg.substr( delimitter + 1, arg.length() );
    y2 = atoi( arg.substr(0,arg.length()).c_str() );
  }


  if( session->loglevel >= 5 ){ 
    (*session->logfile) << "PFL :: Resolution: " << resolution
			<< ", Position: " << x1 << "," << y1 << " - " 
			<< x2 << "," << y2 << endl;
  }


  // Make sure we don't request impossible resolutions
  if( resolution<0 || resolution>=(*session->image)->getNumResolutions() ){
    ostringstream error;
    error << "PFL :: Invalid resolution number: " << resolution; 
    throw error.str();
  }


  // Determine whether we have a horizontal or vertical profile or just a single point
  if( x2 > x1 ){
    width = x2-x1;
    height = 1;
  }
  else if( y2 > y1 ){
    width = 1;
    height = y2-y1;
  }
  else{
    width = 1;
    height = 1;
  }
  unsigned long length = width * height;


  // Create our tilemanager object
  TileManager tilemanager( session->tileCache, *session->image, session->watermark, session->jpeg, session->logfile, session->loglevel );


  // Use our horizontal views function to get a list of available spectral images
  list <int> views = (*session->image)->getHorizontalViewsList();
  list <int> :: const_iterator i;
  unsigned int n = views.size();


  // Put the results into a string stream
  ostringstream profile;
  profile.precision(6);

  // Insert our opening braces
  profile << "{\n\t\"profile\": ";
  if( n > 1 ) profile << "{\n";

  unsigned int k = 0;

  // Loop through our spectral bands
  for( i = views.begin(); i != views.end(); i++ ){

    int wavelength = *i;

    // Add our opening brackets and prefix with the wavelenth if we have multi-spectral data
    if( n > 1 ) profile << "\t\t" << wavelength << ": ";
    profile << "[";

    // Get the region of data for this wavelength and line profile
    RawTile rawtile = tilemanager.getRegion( resolution, wavelength, session->view->yangle, session->view->getLayers(), x1, y1, width, height );


    // Loop through our pixels
    for( unsigned int j=0; j<length; j++ ){

      float intensity;
      void *ptr;

      // Handle depending on bit depth
      if( rawtile.bpc == 8 ){
	ptr = (unsigned char*) (rawtile.data);
	intensity = (float)((unsigned char*)ptr)[j];
      }
      else if( rawtile.bpc == 16 ){
	ptr = (unsigned short*) (rawtile.data);
	intensity = (float)((unsigned short*)ptr)[j];
      }
      else if( rawtile.bpc == 32 ){
	if( rawtile.sampleType == FIXEDPOINT ){
	  ptr = (unsigned int*) rawtile.data;
	  intensity = (float)((unsigned int*)ptr)[j];
	}
	else{
	  ptr = (float*) rawtile.data;
	  intensity = (float)((float*)ptr)[j];
	}
      }

      if( rawtile.sampleType == FLOATINGPOINT ) profile << fixed;
      profile << intensity;
      if( j < length-1 ) profile << ",";

    }

    // Don't add trailing commas to the final sequence
    if( k++ < n-1 ) profile << "],\n";
    else profile << "]\n";

  }

  // Add our closing braces
  if( n > 1 ) profile << "\t}\n";
  profile << "}";


  // Send out our JSON header
#ifndef DEBUG
  char str[1024];
  snprintf( str, 1024,
	    "Server: iipsrv/%s\r\n"
	    "Content-Type: application/json\r\n"
	    "Cache-Control: max-age=%d\r\n"
	    "Last-Modified: %s\r\n"
	    "\r\n",
	    VERSION, MAX_AGE, (*session->image)->getTimestamp().c_str() );

  session->out->printf( (const char*) str );
  session->out->flush();
#endif

  // Send the data itself
  session->out->printf( profile.str().c_str() );
  session->out->flush();

  if( session->out->flush() == -1 ) {
    if( session->loglevel >= 1 ){
      *(session->logfile) << "PFL :: Error flushing JSON" << endl;
    }
  }

  // Inform our response object that we have sent something to the client
  session->response->setImageSent();

  // Total profile response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "PFL :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
