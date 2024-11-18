/*
    View Member Functions

    Copyright (C) 2004-2024 Ruven Pillay.

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

#include "View.h"
#include <cmath>
using namespace std;


/// Calculate optimal resolution for a given requested pixel dimension
void View::calculateResolution( unsigned int dimension,
				unsigned int requested_size ){

  // Reset our resolution level to the smallest available
  resolution = 0;

  // Start from the highest resolution
  int j = max_resolutions - 1;
  unsigned int d = dimension;

  // Make sure we have a minimum size
  unsigned int rs = (requested_size<min_size) ? min_size : requested_size;

  // Find the resolution level closest but higher than the requested size
  while( true ){
    d = (unsigned int) floor(d/2.0);
    if( d < rs ) break;
    j--;
  }

  // Limit j to the maximum resolution
  if( j < 0 ) j = 0;
  if( j > (int)(max_resolutions-1) ) j = max_resolutions - 1;

  // Only update value of resolution if our calculated resolution is greater than that has already been set
  if( j > resolution ) resolution = j;

}


/// Calculate the optimal resolution and the size of this resolution for the requested view,
/// taking into account any maximum size settings
unsigned int View::getResolution(){

  unsigned int i;

  // Note that we use floor() as that is how our resolutions are calculated
  vector<unsigned int> requested_size = View::getRequestSize();
  View::calculateResolution( width, floor((float)requested_size[0]/(float)view_width) );
  View::calculateResolution( height, floor((float)requested_size[1]/(float)view_height) );

  res_width = width;
  res_height = height;

  // Calculate the width and height of this resolution
  for( i=1; i < (max_resolutions - resolution); i++ ){
    res_width = (int) floor(res_width/2.0);
    res_height = (int) floor(res_height/2.0);
  }

  // Check if we need to limit to a smaller resolution due to our max size limit
  float scale = getScale();

  if( (max_size > 0) &&
      ( (res_width*view_width*scale > (unsigned int) max_size) ||
	(res_height*view_height*scale > (unsigned int) max_size) ) ){
    int dimension;
    if( (res_width*view_width/max_size) > (res_height*view_height/max_size) ){
      dimension = (int) (res_width*view_width*scale);
    }
    else{
      dimension = (int) (res_height*view_height*scale);
    }

    while( resolution > 0 && ( dimension > (unsigned int) max_size ) ){
      dimension = (int) (dimension/2.0);
      res_width = (int) floor(res_width/2.0);
      res_height = (int) floor(res_height/2.0);
      resolution--;
    }
  }

  return resolution;

}


float View::getScale(){

  unsigned int rw;
  unsigned int rh;
  if( requested_width == 0 && requested_height > 0 ){
    rw = static_cast<unsigned int>( round(res_width * requested_height / res_height) );
  }
  else rw = requested_width;

  if( requested_height == 0 && requested_width > 0 ){
    rh = static_cast<unsigned int>( round( res_height * requested_width / res_width ) );
  }
  else rh = requested_height;


  float scale = static_cast<float>(rw) / static_cast<float>(width);

  if( static_cast<float>(rh) / static_cast<float>(res_height) < scale ){
    scale = static_cast<float>(rh) / static_cast<float>(res_height);
  }


  // Sanity check
  if( scale <= 0 || scale > 1.0 ) scale = 1.0;
  return scale;

}


void View::setViewLeft( float x ) {
  if( x > 1.0 ) view_left = 1.0;
  else if( x < 0.0 ) view_left = 0.0;
  else view_left = x;
}


void View::setViewTop( float y ) {
  if( y > 1.0 ) view_top = 1.0;
  else if( y < 0.0 ) view_top = 0.0;
  else view_top = y;
}


void View::setViewWidth( float w ) {
  // Sanity check
  if( w > 1.0 ) view_width = 1.0;
  else if( w < 0.0 ) view_width = 0.0;
  else view_width = w;
}


void View::setViewHeight( float h ) {
  // Sanity check
  if( h > 1.0 ) view_height = 1.0;
  else if( h < 0.0 ) view_height = 0.0;
  else view_height = h;
}


bool View::viewPortSet() {
  if( (view_width < 1.0) || (view_height < 1.0) ||
      (view_left > 0.0) || (view_top > 0.0) ){
    return true;
  }
  else return false;
}


unsigned int View::getViewLeft(){
  // Scale up our view to a real pixel value.
  // Note that we calculate from our full resolution image to avoid errors from the rounding at each resolution size
  unsigned int l = round( width*view_left/(1 << (max_resolutions-resolution-1)) );
  if( l > res_width ) l = res_width;   // As we use round(), possible to have sizes > than existing resolution size
  return l;
}


unsigned int View::getViewTop(){
  // Scale up our view to a real pixel value
  // Note that we calculate from our full resolution image to avoid errors from the rounding at each resolution size
  unsigned int t = round( height*view_top/(1<<(max_resolutions-resolution-1)) );
  if( t > res_height ) t = res_height;   // As we use round(), possible to have sizes > than existing resolution size
  return t;
}


unsigned int View::getViewWidth(){

  // Scale up our viewport, then make sure our size is not too large or too small
  float scale = (float) width / (1<<(max_resolutions-resolution-1));   // Calculate exact scale from largest resolution
  unsigned int w = (unsigned int) round( view_width * scale );
  unsigned int left = (unsigned int) round( view_left * scale );

  if( left > res_width ) left = res_width;   // As we use round(), possible to have sizes > than existing resolution size
  if( (w + left) > res_width ) w = res_width - left;                   // Need to use width of current resolution
  if( w < min_size ) w = min_size;

  return w;
}


unsigned int View::getViewHeight(){

  // Scale up our viewport, then make sure our size is not too large or too small
  float scale = (float) height / (1<<(max_resolutions-resolution-1));  // Calculate exact scale from largest resolution
  unsigned int h = (unsigned int) round( view_height * scale );
  unsigned int top = (unsigned int) round( view_top * scale );

  if( top > res_height ) top = res_height;   // As we use round(), possible to have sizes > than existing resolution size
  if( (h + top) > res_height ) h = res_height - top;                   // Need to use height of current resolution
  if( h < min_size ) h = min_size;

  return (unsigned int) h;
}


vector<unsigned int> View::getRequestSize(){

  unsigned int w = requested_width;
  unsigned int h = requested_height;

  // Calculate aspect ratio
  float ratio = (view_width * width) / (view_height * height);

  if( requested_width == 0 && requested_height != 0 ){
    w = (unsigned int) round( (float)requested_height * ratio );
  }
  else if( requested_height == 0 && requested_width != 0 ){
    h = (unsigned int) round( (float)requested_width / ratio );
  }
  else if( requested_width == 0 && requested_height == 0 ){
    w = width;
    h = height;
  }
  // If both width and height are set, restrict image to this bounding box
  else if( requested_width != 0 && requested_height != 0 && maintain_aspect ){
    float xscale = requested_width / (view_width*width);
    float yscale = requested_height / (view_height*height);
    // Fit to the axis requiring the most scaling (smallest factor)
    if( xscale > yscale ) w = (unsigned int) (unsigned int) round( (float)requested_height * ratio );
    else h = (unsigned int) round( (float)requested_width / ratio );
  }


  // Limit our requested size to the maximum output size
  if( max_size > 0 && (w > (unsigned int) max_size || h > (unsigned int) max_size) ){
    if( w > h ){
      w = max_size;
      h = round( (float) w / ratio );
    }
    else if( h > w ){
      h = max_size;
      w = round( (float) h * ratio );
    }
    else{
      w = max_size;
      h = max_size;
    }
  }

  // Create and return our result
  std::vector<unsigned int> size = { w, h };
  return size;
}


/// Return the number of layers to decode
int View::getLayers(){
  // If max_layers is set, limit to this value, otherwise return layers
  if( max_layers > 0 ){
    return ((layers>0)&&(layers<max_layers)) ? layers : max_layers;
  }
  if( max_layers<0 && layers==0 ){ return -1; };

  return layers;
}
