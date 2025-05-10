/*
    IIP OJB Command Handler Class Member Functions

    Copyright (C) 2006-2025 Ruven Pillay.

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
#include <algorithm>
#include <sstream>


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


  // Argument is usually always 'iip,1.0', but just compare the prefix to allow other versions in future
  if( argument.compare( 0, 4, "iip," ) == 0 ) iip();
  else if( argument == "basic-info" ){
    iip_server();
    max_size();
    resolution_number();
    colorspace( "*,*" );
  }
  else if( argument == "iip-server" ) iip_server();
  // IIP optional commands
  else if( argument == "iip-opt-comm" ) session->response->addResponse( "IIP-opt-comm:CVT CNT QLT JTL JTLS WID HEI RGN MINMAX SHD CMP INV CTW" );
  // IIP optional objects
  else if( argument == "iip-opt-obj" ) session->response->addResponse( "IIP-opt-obj:Horizontal-views Vertical-views Tile-size Bits-per-channel Min-Max-sample-values Resolutions" );
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
  // Minimum and maximum provided by TIFF tags
  else if( argument == "min-max-sample-values" ) min_max_values();
  // List of available resolutions
  else if( argument == "resolutions" ) resolutions();
  // Get physical resolution (DPI)
  else if( argument == "dpi" ) dpi();
  else if( argument == "stack" ) stack();

  // Colorspace
  /* The request can have a suffix, which we don't need, so do a
     like scan
  */
  else if( argument.find( "colorspace" ) != string::npos ){
    colorspace( "*,*" );
  }

  // Image Metadata
  else if( argument == "summary-info" ){
    metadata( "rights" );
    metadata( "description" );
    metadata( "creator" );
    metadata( "date" );
    metadata( "software" );
  }

  else if( argument == "rights" || argument == "title" ||
	   argument == "description" || argument == "creator" ||
	   argument == "keywords" || argument == "comment" ||
	   argument == "last-author" || argument == "rev-number" ||
	   argument == "edit-time" || argument == "last-printed" ||
	   argument == "date" || argument == "last-save-dtm" ||
	   argument == "software"  || argument == "make" ||
	   argument == "model" || argument == "xmp" ||
	   argument == "scale" ){

    metadata( argument );
  }

  // Send all available metadata
  else if( argument == "metadata" ){

    stringstream json;
    json << "{ ";

    map <const string, const string> :: const_iterator i;
    for( i = (*session->image)->metadata.begin(); i != (*session->image)->metadata.end(); i++ ){
      if( i->first == "icc" || i->first == "xmp" ) continue;
      if( (i->second).length() ) json << endl << "\t\"" << i->first << "\": \"" << i->second << "\",";
    }

    // Remove trailling comma - if no items were added, this safely removes extra white space after opening {
    json.seekp( -1, std::ios_base::end );
    json << endl << "}";

    session->response->setMimeType( "application/json" );
    session->response->addResponse( json.str() );
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
  session->response->setProtocol( "IIP:" + string(VERSION) );
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

  // For 90 and 270 rotation swap width and height
  if( (int)((session->view)->getRotation()) % 180 == 90 ){
    unsigned int tmp = x;
    x = y;
    y = tmp;
  }

  if( session->loglevel >= 2 ){
    *(session->logfile) << "OBJ :: Max-size is " << x << " " << y << endl;
  }
  session->response->addResponse( "Max-size", x, y );
}


void OBJ::resolution_number(){

  checkImage();
  int no_res = (*session->image)->getNumResolutions();
  if( session->loglevel >= 5 ){
    *(session->logfile) << "OBJ :: Resolution-number handler returning " << no_res << endl;
  }
  session->response->addResponse( "Resolution-number", no_res );

}


void OBJ::dpi(){

  checkImage();
  float dpix = (*session->image)->getHorizontalDPI();
  float dpiy = (*session->image)->getVerticalDPI();

  if( dpix && dpiy ){
    char tmp[64];
    snprintf( tmp, 64, "DPI:%f %f", dpix, dpiy );
    if( session->loglevel >= 5 ){
      *(session->logfile) << "OBJ :: DPI handler returning " << tmp << endl;
    }
    session->response->addResponse( tmp );
  }
}


void OBJ::tile_size(){
  checkImage();

  int x = (*session->image)->getTileWidth();
  int y = (*session->image)->getTileHeight();
  if( session->loglevel >= 5 ){
    *(session->logfile) << "OBJ :: Tile-size is " << x << " " << y << endl;
  }
  session->response->addResponse( "Tile-size", x, y );
}


void OBJ::bits_per_channel(){

  checkImage();
  int bpc = (*session->image)->getNumBitsPerPixel();
  if( session->loglevel >= 5 ){
    *(session->logfile) << "OBJ :: Bits-per-channel handler returning " << bpc << endl;
  }
  session->response->addResponse( "Bits-per-channel", bpc );

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


void OBJ::min_max_values(){

  checkImage();
  unsigned int n = (*session->image)->getNumChannels();
  string tmp = "Min-Max-sample-values:";
  char val[24];
  float minimum, maximum;
  for( unsigned int i=0; i<n ; i++ ){
    minimum = (*session->image)->getMinValue(i);
    maximum = (*session->image)->getMaxValue(i);
    snprintf( val, 24, " %.9g ", minimum );
    tmp += val;
    snprintf( val, 24, " %.9g ", maximum );
    tmp += val;
  }
  // Chop off the final space
  tmp.resize( tmp.length() - 1 );
  session->response->addResponse( tmp );
  if( session->loglevel >= 5 ){
    *(session->logfile) << "OBJ :: Min-Max-sample-values handler returning " << tmp << endl;
  }

}


void OBJ::resolutions(){

  checkImage();
  char val[32];
  int num_res = (*session->image)->getNumResolutions();

  string tmp = "Resolutions:";
  for( int i=num_res-1; i>=0; i-- ){
    snprintf( val, 32, "%d %d", (*session->image)->image_widths[i], (*session->image)->image_heights[i] );
    tmp += val;
    if( i>0 ) tmp += ",";
  }
  session->response->addResponse( tmp );
  if( session->loglevel >= 5 ){
    *(session->logfile) << "OBJ :: Resolutions handler returning " << tmp << endl;
  }
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
  if( (*session->image)->getColorSpace() == ColorSpace::CIELAB ){
    colourspace = 4;
    calibrated = 1;
  }
  else if( (*session->image)->getColorSpace() == ColorSpace::GREYSCALE ){
    colourspace = 1;
    planes = "1 0";
  }
  else colourspace = 3;

  int no_res = (*session->image)->getNumResolutions();
  char tmp[41];
  snprintf( tmp, 41, "Colorspace,0-%d,0:%d 0 %d %s", no_res-1,
	    calibrated, colourspace, planes );

  if( session->loglevel >= 5 ){
    *(session->logfile) << "OBJ :: Colourspace handler returning " << tmp << endl;
  }

  session->response->addResponse( tmp );
}


void OBJ::metadata( string field ){

  checkImage();

  string metadata = (*session->image)->metadata[field];

  if( session->loglevel >= 5 ){
    *(session->logfile) << "OBJ :: " << field << " handler returning '" << metadata << "'" << endl;
  }

  if( metadata.length() ){
    // Set appropriate mime type
    string mimeType = "text/plain";
    if( field == "xmp" ) mimeType = "application/xml";
    session->response->setMimeType( mimeType );
    session->response->addResponse( metadata );
  }
}


// Return image stack metadata in JSON format
void OBJ::stack(){

  checkImage();

  if( (*session->image)->isStack() ){

    list<Stack> items = (*session->image)->getStack();
    list<Stack> :: const_iterator i;
    int n = 0;

    stringstream json;
    json.precision(9);
    json << "[ ";

    for( i=items.begin(); i != items.end(); i++ ){
      json << endl << "\t{" << endl
	   << "\t\t\"id\": " << n++ << "," << endl
	   << "\t\t\"name\": \"" << (*i).name << "\"," << endl
	   << "\t\t\"scale\": " << (*i).scale << endl << "\t},";
    }

    // Remove trailling comma - if no items were added, this safely removes extra white space after opening {
    json.seekp( -1, std::ios_base::end );
    json << endl << "]";

    session->response->setMimeType( "application/json" );
    session->response->addResponse( json.str() );
  }
  else{
    if( session->loglevel >= 3 ){
      *(session->logfile) << "OBJ :: stack handler: not an image stack" << endl;
    }
  }
}
