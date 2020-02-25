/*
    IIP DeepZoom Request Command Handler Class Member Function

    Development supported by Moravian Library in Brno (Moravska zemska
    knihovna v Brne, http://www.mzk.cz/) R&D grant MK00009494301 & Old
    Maps Online (http://www.oldmapsonline.org/) from the Ministry of
    Culture of the Czech Republic.


    Copyright (C) 2009-2020 Ruven Pillay.

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

#include <cmath>

#include "Task.h"
#include "Transforms.h"



using namespace std;


// Windows does not provide a log2 function!
#if (!defined HAVE_LOG2 && !defined _MSC_VER) || (defined _MSC_VER && _MSC_VER<1900)
double log2(double max){
  return log((double)max)/log((double)2);
}
#endif


void DeepZoom::run( Session* session, const std::string& argument ){

  if( session->loglevel >= 3 ) (*session->logfile) << "DeepZoom handler reached" << endl;

  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // A DeepZoom request consists of 2 types of request. The first for the .dzi xml file
  // containing image metadata and the second of the form _files/r/x_y.jpg for the tiles
  // themselves where r is the resolution number and x and y are the tile coordinates
  // starting from the bottom left.

  string prefix, suffix;
  suffix = argument.substr( argument.find_last_of( "." )+1, argument.length() );

  // We need to extract the image path, which is not always the same
  if( suffix == "dzi" )
    prefix = argument.substr( 0, argument.length()-4 );
  else
    prefix = argument.substr( 0, argument.rfind( "_files/" ) );



  // As we don't have an independent FIF request, we need to run it now
  FIF fif;
  fif.run( session, prefix );


  // Get the full image size and the total number of resolutions available
  unsigned int width = (*session->image)->getImageWidth();
  unsigned int height = (*session->image)->getImageHeight();


  unsigned int tw = (*session->image)->getTileWidth();
  unsigned int numResolutions = (*session->image)->getNumResolutions();


  // DeepZoom does not accept arbitrary numbers of resolutions. The number of levels
  // is calculated by rounding up the log_2 of the larger of image height and image width;
  unsigned int dzi_res;
  unsigned int max = width;
  if( height > width ) max = height;
  dzi_res = (int) ceil( log2(max) );

  if( session->loglevel >= 4 ){
    *(session->logfile) << "DeepZoom :: required resolutions : " << dzi_res << ", real: " << numResolutions << endl;
  }


  // DeepZoom clients have 2 phases, the initialization phase where they request
  // an XML file containing image data and the tile requests themselves.
  // These 2 phases are handled separately
  if( suffix == "dzi" ){

    if( session->loglevel >= 2 )
      *(session->logfile) << "DeepZoom :: DZI header request" << endl;

    if( session->loglevel >= 4 ){
      *(session->logfile) << "DeepZoom :: Total resolutions: " << numResolutions << ", image width: " << width
			  << ", image height: " << height << endl;
    }


    // Format our output
    stringstream header;
    header << session->response->createHTTPHeader( "xml", (*session->image)->getTimestamp() )
	   << "<Image xmlns=\"http://schemas.microsoft.com/deepzoom/2008\" "
	   << "TileSize=\"" << tw << "\" Overlap=\"0\" Format=\"jpg\">"
	   << "<Size Width=\"" << width << "\" Height=\"" << height << "\"/>"
	   << "</Image>";

    session->out->printf( (const char*) header.str().c_str() );
    session->response->setImageSent();

    return;
  }


  // Get the tile coordinates. DeepZoom requests are of the form $image_files/r/x_y.jpg
  // where r is the resolution number and x and y are the tile coordinates

  int resolution, x, y;
  unsigned int n, n1, n2;

  // Extract resolution
  n1 = argument.find_last_of("/");
  n2 = argument.substr(0,n1).find_last_of("/")+1;
  resolution = atoi( argument.substr(n2,n1-n2).c_str() );

  // Extract tile x,y coordinates
  n = argument.find_last_of(".")-n1-1;
  suffix = argument.substr( n1+1, n );
  n = suffix.find_first_of("_");
  x = atoi( suffix.substr(0,n).c_str() );
  y = atoi( suffix.substr(n+1,suffix.length()).c_str() );


  // Take into account the extra zoom levels required by the DeepZoom spec
  resolution = resolution - (dzi_res-numResolutions) - 1;
  if( resolution < 0 ) resolution = 0;
  if( (unsigned int)resolution > numResolutions-1 ) resolution = numResolutions-1;

  if( session->loglevel >= 2 ){
    *(session->logfile) << "DeepZoom :: Tile request for resolution: "
			<< resolution << " at x: " << x << ", y: " << y << endl;
  }


  // Get the width and height for the requested resolution
  width = (*session->image)->getImageWidth(numResolutions-resolution-1);
  height = (*session->image)->getImageHeight(numResolutions-resolution-1);


  // Get the width of the tiles and calculate the number
  // of tiles in each direction
  unsigned int rem_x = width % tw;
  unsigned int ntlx = (width / tw) + (rem_x == 0 ? 0 : 1);


  // Calculate the tile index for this resolution from our x, y
  unsigned int tile = y*ntlx + x;


  // Simply pass this on to our JTL send command
  JTL jtl;
  jtl.send( session, resolution, tile );


  // Total DeepZoom response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "DeepZoom :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }


}
