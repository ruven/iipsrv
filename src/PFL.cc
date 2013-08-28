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


/// Return the spectral reflectance for a particular point in XML format
void PFL::run( Session* session, const std::string& argument ){

  /* The argument should be of the form <resolution>:<x1>,<y1>-<x2>,<y2>
     1) resolution
     2) start pixel index in x direction
     3) start pixel index in y direction
     4) end pixel index in x direction
     5) end pixel index in y direction
  */

  if( session->loglevel >= 3 ) (*session->logfile) << "PFL handler reached" << endl;

  unsigned int resolution, x1, y1, x2, y2;


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

  arg = arg.substr( delimitter + 1, arg.length() );
  delimitter = arg.find( "," );
  x2 = atoi( arg.substr(0,delimitter).c_str() );

  arg = arg.substr( delimitter + 1, arg.length() );
  y2 = atoi( arg.substr(0,arg.length()).c_str() );

  if( session->loglevel >= 5 ){ 
    (*session->logfile) << "PFL :: Resolution: " << resolution
			<< ", Position: " << x1 << "," << y1 << " - " 
			<< x2 << "," << y2 << endl;
  }
  

  // Create our tilemanager object
  TileManager tilemanager( session->tileCache, *session->image, session->watermark, session->jpeg, session->logfile, session->loglevel );


#ifndef DEBUG
  char str[1024];
  snprintf( str, 1024,
	    "Server: iipsrv/%s\r\n"
	    "Content-Type: application/xml\r\n"
	    "Cache-Control: max-age=%d\r\n"
	    "Last-Modified: %s\r\n"
	    "\r\n",
	    VERSION, MAX_AGE, (*session->image)->getTimestamp().c_str() );

  session->out->printf( (const char*) str );
  session->out->flush();
#endif

  session->out->printf( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
  session->out->printf( "<profile>\n" );
  session->out->flush();

  unsigned width = x2-x1;

  // Get the region of data for this line profile
  RawTile rawtile = tilemanager.getRegion( resolution, session->view->xangle, session->view->yangle, session->view->getLayers(), x1, y1, width, 1 );

  // Put the results into a string stream
  std::ostringstream profile;
  profile.precision(4);

  for( unsigned int i=0; i<width; i++ ){

    float intensity;
    void *ptr;

    // Handle depending on bit depth
    if( rawtile.bpc == 8 ){
      ptr = (unsigned char*) (rawtile.data);
      intensity = static_cast<float>((float)((unsigned char*)ptr)[i]);
    }
    else if( rawtile.bpc == 16 ){
      ptr = (unsigned short*) (rawtile.data);
      intensity = static_cast<float>((float)((unsigned short*)ptr)[i]);
    }
    else if( rawtile.bpc == 32 ){
      if( rawtile.sampleType == FIXEDPOINT ) {
        ptr = (unsigned int*) rawtile.data;
        intensity = static_cast<float>((float)((unsigned int*)ptr)[i]);
      }
      else {
        ptr = (float*) rawtile.data;
        intensity = static_cast<float>((float)((float*)ptr)[i]);
      }
    }

    if( rawtile.sampleType == FLOATINGPOINT ) profile << fixed;
    profile << intensity;
    if( i < width-1 ) profile << ",";

  }

  session->out->printf( profile.str().c_str() );
  session->out->flush();

  if( session->loglevel >= 5 ) (*session->logfile) << "PFL :: " << profile.str().c_str() << endl;


  session->out->printf( "</profile>" );

  session->out->printf( "\r\n" );

  if( session->out->flush() == -1 ) {
    if( session->loglevel >= 1 ){
      *(session->logfile) << "PFL :: Error flushing jpeg tile" << endl;
    }
  }


  // Inform our response object that we have sent something to the client
  session->response->setImageSent();

  // Total profile response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "PFL :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
