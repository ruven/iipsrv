/*
    IIP OJB Command Handler Class Member Functions

    Copyright (C) 2006-2009 Ruven Pillay.

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
#include <iostream>
#include <algorithm>


using namespace std;



void OBJ::run( Session* s, const std::string& a )
{

  argument = a;
  // Convert to lower case the argument supplied to the OBJ command
  transform( argument.begin(), argument.end(), argument.begin(), ::tolower );

  session = s;

  // Log this
  if( session->loglevel >= 3 ) *(session->logfile) << "OBJ :: " << argument << " to be handled" << endl;

  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  if( argument == "iip,1.0" ) iip();
  else if( argument == "basic-info" ){
    iip_server();
    max_size();
    resolution_number();
    colorspace( "*,*" );
  }
  else if( argument == "iip-server" ) iip_server();
  // IIP optional commands
  else if( argument == "iip-opt-comm" ) session->response->addResponse( "IIP-opt-comm:CVT CNT QLT JTL JTLS WID HEI RGN SHD" );
  // IIP optional objects
  else if( argument == "iip-opt-obj" ) session->response->addResponse( "IIP-opt-obj:Horizontal-views Vertical-views Tile-size Bits-per-channel" );
  // Resolution-number
  else if( argument == "resolution-number" ) resolution_number();
  // Max-size
  else if( argument == "max-size" ) max_size();
  // Tile-size
  else if( argument == "tile-size" ) tile_size();
  // Bits per pixel
  else if( argument == "bits-per-channel" ) bits_per_channel();
  // Vertical-views
  else if( argument == "vertical-views" ) vertical_views();
  // Horizontal-views
  else if( argument == "horizontal-views" ) horizontal_views();

  // Colorspace
  /* The request can have a suffix, which we don't need, so do a
     like scan
  */
  else if( argument.find( "colorspace" ) != string::npos ){
    colorspace( "*,*" );
  }

  // Image Metadata
  else if( argument == "summary-info" ){

    metadata( "copyright" );
    metadata( "subject" );
    metadata( "author" );
    metadata( "create-dtm" );
    metadata( "app-name" );
  }

  else if( argument == "copyright" || argument == "title" || 
	   argument == "subject" || argument == "author" ||
	   argument == "keywords" || argument == "comment" ||
	   argument == "last-author" || argument == "rev-number" ||
	   argument == "edit-time" || argument == "last-printed" ||
	   argument == "create-dtm" || argument == "last-save-dtm" ||
	   argument == "app-name" ){

    metadata( argument );
  }


  // None of the above!
  else{

    if( session->loglevel >= 1 ){
      *(session->logfile) << "OBJ :: Unsupported argument: " << argument << " received" << endl;
    }

    // Unsupported object error code is 3 2
    session->response->setError( "3 2", argument );
  }


  if( session->loglevel >= 2 ){
    *(session->logfile) << "OBJ :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }


}



void OBJ::iip(){
  session->response->setProtocol( "IIP:1.0" );
}


void OBJ::iip_server(){
  // The binary capability code is 1000001 == 65 in integer
  // ie can do CVT jpeg and JTL, but no transforms
  session->response->addResponse( "IIP-server:3.65" );
}


void OBJ::max_size(){
  checkImage();
  int x = (*session->image)->getImageWidth();
  int y = (*session->image)->getImageHeight();
  if( session->loglevel >= 2 ){
    *(session->logfile) << "OBJ :: Max-size is " << x << " " << y << endl;
  }
  session->response->addResponse( "Max-size", x, y );
}


void OBJ::resolution_number(){

  checkImage();
  int no_res = (*session->image)->getNumResolutions();
  if( session->loglevel >= 2 ){
    *(session->logfile) << "OBJ :: Resolution-number handler returning " << no_res << endl;
  }
  session->response->addResponse( "Resolution-number", no_res );

}


void OBJ::tile_size(){
  checkImage();

  int x = (*session->image)->getTileWidth();
  int y = (*session->image)->getTileHeight();
  if( session->loglevel >= 2 ){
    *(session->logfile) << "OBJ :: Tile-size is " << x << " " << y << endl;
  }
  session->response->addResponse( "Tile-size", x, y );
}


void OBJ::bits_per_channel(){

  checkImage();
  int bpp = (*session->image)->getNumBitsPerPixel();
  if( session->loglevel >= 2 ){
    *(session->logfile) << "OBJ :: Bits-per-channel handler returning " << bpp << endl;
  }
  session->response->addResponse( "Bits-per-channel", bpp );

}


void OBJ::vertical_views(){
  checkImage();
  list <int> views = (*session->image)->getVerticalViewsList();
  list <int> :: const_iterator i;
  string tmp = "Vertical-views:";
  char val[8];
  for( i = views.begin(); i != views.end(); i++ ){
    snprintf( val, 8, "%d ", *i );
    tmp += val;
  }
  // Chop off the final space
  tmp.resize( tmp.length() - 1 );
  session->response->addResponse( tmp );
}


void OBJ::horizontal_views(){
  checkImage();
  list <int> views = (*session->image)->getHorizontalViewsList();
  list <int> :: const_iterator i;
  string tmp = "Horizontal-views:";
  char val[8];
  for( i = views.begin(); i != views.end(); i++ ){
    snprintf( val, 8, "%d ", *i );
    tmp += val;
  }
  // Chop off the final space
  tmp.resize( tmp.length() - 1 );
  session->response->addResponse( tmp );
}


void OBJ::colorspace( std::string arg ){

  checkImage();

  /* Assign the colourspace tag: 1 for greyscale, 3 for RGB and
     a colourspace of 4 to LAB images
     WARNING: LAB support is an extension and is not in the
     IIP protocol standard (as of version 1.05)
  */
  const char *planes = "3 0 1 2";
  int calibrated = 0;
  int colourspace;
  if( (*session->image)->getColourSpace() == CIELAB ){
    colourspace = 4;
    calibrated = 1;
  }
  else if( (*session->image)->getColourSpace() == GREYSCALE ){
    colourspace = 1;
    planes = "1 0";
  }
  else colourspace = 3;

  int no_res = (*session->image)->getNumResolutions();
  char tmp[32];
  snprintf( tmp, 32, "Colorspace,0-%d,0:%d 0 %d %s", no_res-1,
	    calibrated, colourspace, planes );

  if( session->loglevel >= 2 ){
    *(session->logfile) << "OBJ :: Colourspace handler returning " << tmp << endl;
  }

  session->response->addResponse( tmp );
}


void OBJ::metadata( string field ){

  checkImage();

  string metadata = (*session->image)->getMetadata( field );
  if( session->loglevel >= 3 ){
    *(session->logfile) << "OBJ :: " << field << " handler returning" << metadata << endl;
  }

  if( metadata.length() ){
    session->response->addResponse( field, metadata.c_str() );
  }


}


