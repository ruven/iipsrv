/*
    IIP SPECTRA Command Handler Class Member Function

    Copyright (C) 2009-2025 Ruven Pillay.

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
void SPECTRA::run( Session* session, const std::string& argument ){

  /* The argument should consist of 2 comma separated values:
     1) resolution
     2) tile number
     3) pixel index in x direction
     4) pixel index in y direction
  */

  if( session->loglevel >= 3 ) (*session->logfile) << "SPECTRA handler reached" << endl;


  // Make sure we have set our image
  this->session = session;
  checkImage();


  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // Parse the argument list
  string arg = argument;
  int delimitter = arg.find( "," );
  int resolution = atoi( arg.substr(0,delimitter).c_str() );

  arg = arg.substr( delimitter + 1, arg.length() );
  delimitter = arg.find( "," );
  int tile = atoi( arg.substr(0,delimitter).c_str() );

  arg = arg.substr( delimitter + 1, arg.length() );
  delimitter = arg.find( "," );
  int x = atoi( arg.substr(0,delimitter).c_str() );

  arg = arg.substr( delimitter + 1, arg.length() );
  delimitter = arg.find( "," );
  int y = atoi( arg.substr(0,arg.length()).c_str() );

  if( session->loglevel >= 5 ){ 
    (*session->logfile) << "SPECTRA :: resolution: " << resolution
			<< ", tile: " << tile
			<< ", x: " << x
			<< ", y: " << y << endl;
  }

  // Make sure our x,y coordinates are within the tile dimensions
  if( x < 0 || x >= (int)(*session->image)->getTileWidth(resolution) ||
      y < 0 || y >= (int)(*session->image)->getTileHeight(resolution) ){
    throw invalid_argument( "SPECTRA :: Error: x,y coordinates outside of tile boundaries" );
  }
  

  TileManager tilemanager( session->tileCache, *session->image, session->jpeg, session->logfile, session->loglevel );

  // Use our horizontal views function to get a list of available spectral images
  list <int> views = (*session->image)->getHorizontalViewsList();
  list <int> :: const_iterator i;

  // Check whether we have an image stack
  bool haveStack = false;
  std::list <Stack> stack = (*session->image)->getStack();
  list<Stack> :: const_iterator j = stack.begin();
  if( stack.size() > 0 ) haveStack = true;

  // Our list of spectral reflectance values for the requested point
  list <float> spectrum;


#ifndef DEBUG
  // Output our HTTP header
  stringstream header;
  header << session->response->createHTTPHeader( "xml", (*session->image)->getTimestamp() );
  session->out->putStr( header.str().c_str(), (int) header.tellp() );
  session->out->flush();
#endif

  session->out->putS( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
  session->out->putS( "<spectra>\n" );
  session->out->flush();

  for( i = views.begin(); i != views.end(); i++ ){

    int n = *i;

    RawTile rawtile = tilemanager.getTile( resolution, tile, n, session->view->yangle, session->view->getLayers(), ImageEncoding::RAW );

    // Make sure our x,y coordinates are within the tile dimensions
    if( x >= (int)rawtile.width || y >= (int)rawtile.height ){
      if( session->loglevel >= 1 ){
	(*session->logfile) << "SPECTRA :: Error: x,y coordinates outside of tile boundaries" << endl;
      }
      break;
    }


    unsigned int tw = (*session->image)->getTileWidth(resolution);
    unsigned int index = y*tw + x;

    void *ptr;
    float reflectance = 0.0;
    string name;

    if( session->loglevel >= 5 ) (*session->logfile) << "SPECTRA :: " << rawtile.bpc << " bits per channel data" << endl;

    // Handle depending on bit depth
    if( rawtile.bpc == 8 ){
      ptr = (unsigned char*) (rawtile.data);
      reflectance = static_cast<float>((float)((unsigned char*)ptr)[index]) / 255.0;
    }
    else if( rawtile.bpc == 16 ){
      ptr = (unsigned short*) (rawtile.data);
      reflectance = static_cast<float>((float)((unsigned short*)ptr)[index]) / 65535.0;
    }
    else if( rawtile.bpc == 32 ){
      if( rawtile.sampleType == SampleType::FIXEDPOINT ) {
        ptr = (unsigned int*) rawtile.data;
        reflectance = static_cast<float>((float)((unsigned int*)ptr)[index]);
      }
      else {
        ptr = (float*) rawtile.data;
        reflectance = static_cast<float>((float)((float*)ptr)[index]);
      }
    }

    spectrum.push_front( reflectance );

    // Get details from our stack if we have one
    if( haveStack ){
      if( j != stack.end() ){
	if( (*j).name.size() > 0 ) name = (*j).name;
	j++;   // Advance our stack iterator
      }
    }
    if( name.empty() ){
      // Format our integer value
      char tmp[16];
      snprintf( tmp, 16, "%d", *i );
      name = string( tmp );
    }


    char tmp[1024];
    snprintf( tmp, 1024, "\t<point>\n\t\t<wavelength>%s</wavelength>\n\t\t<reflectance>%f</reflectance>\n\t</point>\n", name.c_str(), reflectance );
    session->out->putS( tmp );
    session->out->flush();

    if( session->loglevel >= 3 ) (*session->logfile) << "SPECTRA :: Band: " << n << ", reflectance: " << reflectance << endl;
  }


  session->out->putS( "</spectra>" );

  if( session->out->flush() == -1 ) {
    if( session->loglevel >= 1 ){
      *(session->logfile) << "SPECTRA :: Error flushing XML" << endl;
    }
  }


  // Inform our response object that we have sent something to the client
  session->response->setImageSent();

  // Total SPECTRA response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "SPECTRA :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
