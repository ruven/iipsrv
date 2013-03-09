/*
    IIP SPECTRA Command Handler Class Member Function

    Copyright (C) 2009-2013 Ruven Pillay.

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

using namespace std;


/// Return the spectral reflectance for a particular point in XML format
void SPECTRA::run( Session* session, const std::string& argument ){

  /* The argument should consist of 2 comma separated values:
     1) resolution
     2) tile number
     3) pixel index in x direction
     4) pixel index in y direction
  */

  if( session->loglevel >= 3 ) (*session->logfile) << "SPECTRA handler reached" << endl;
  session = session;

  int resolution, tile, x, y;


  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // Parse the argument list
  string arg = argument;
  int delimitter = arg.find( "," );
  resolution = atoi( arg.substr(0,delimitter).c_str() );

  arg = arg.substr( delimitter + 1, arg.length() );
  delimitter = arg.find( "," );
  tile = atoi( arg.substr(0,delimitter).c_str() );

  arg = arg.substr( delimitter + 1, arg.length() );
  delimitter = arg.find( "," );
  x = atoi( arg.substr(0,delimitter).c_str() );

  arg = arg.substr( delimitter + 1, arg.length() );
  delimitter = arg.find( "," );
  y = atoi( arg.substr(0,arg.length()).c_str() );

  if( session->loglevel >= 5 ){ 
    (*session->logfile) << "SPECTRA :: resolution:" << resolution
			<< ",tile: " << tile
			<< ",x:" << x
			<< ",y:" << y << endl;
  }
  

  TileManager tilemanager( session->tileCache, *session->image, session->watermark, session->jpeg, session->logfile, session->loglevel );

  // Use our horizontal views function to get a list of available spectral images
  list <int> views = (*session->image)->getHorizontalViewsList();
  list <int> :: const_iterator i;

  // Our list of spectral reflectance values for the requested point
  list <float> spectrum;


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
  session->out->printf( "<spectra>\n" );
  session->out->flush();

  for( i = views.begin(); i != views.end(); i++ ){

    int n = *i;

    RawTile rawtile = tilemanager.getTile( resolution, tile, n, session->view->yangle, session->view->getLayers(), UNCOMPRESSED );

    unsigned int tw = (*session->image)->getTileWidth();
    unsigned int index = y*tw + x;

    unsigned short *usptr;
    unsigned char *ucptr;
    float reflectance;

    if( session->loglevel >= 5 ) (*session->logfile) << "SPECTRA :: " << rawtile.bpc << " bits per channel data" << endl;

    // Handle depending on bit depth and normalize to 0.0->1.0
    if( rawtile.bpc == 16 ){
      usptr = (unsigned short*) (rawtile.data);
      reflectance = static_cast<float>((float)usptr[index]) / 65536.0;
    }
    else{
      ucptr = (unsigned char*) rawtile.data;
      reflectance = static_cast<float>((float)ucptr[index]) / 256.0;
    }

    spectrum.push_front( reflectance );

    string metadata = (*session->image)->getMetadata( "subject" );

    char tmp[1024];
    snprintf( tmp, 1024, "\t<point>\n\t\t<wavelength>%d</wavelength>\n\t\t<reflectance>%f</reflectance>\n\t</point>\n", n, reflectance );
    session->out->printf( tmp );
    session->out->flush();

    if( session->loglevel >= 3 ) (*session->logfile) << "SPECTRA :: " << n << " with reflectance " << reflectance << endl;
  }


  session->out->printf( "</spectra>" );

  session->out->printf( "\r\n" );

  if( session->out->flush() == -1 ) {
    if( session->loglevel >= 1 ){
      *(session->logfile) << "SPECTRA :: Error flushing jpeg tile" << endl;
    }
  }


  // Inform our response object that we have sent something to the client
  session->response->setImageSent();

  // Total SPECTRA response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "SPECTRA :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
