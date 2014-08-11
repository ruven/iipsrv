/*
    View Member Functions

    Copyright (C) 2004-2014 Ruven Pillay.

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


void View::calculateResolution( unsigned int dimension,
				unsigned int requested_size ){
  unsigned int i = 1;
  unsigned int j = 1;

  // Calculate the resolution number for this request.
  while( (unsigned int)(dimension/i) >= requested_size ){
    i *= 2;
    j++;
  }

  // Limit j to the maximum resolution
  if( j > max_resolutions+1 ) j = max_resolutions + 1;

  // Only set this if our requested resolution is greater than that
  // that has already been set.
  if( (int)max_resolutions - (int)j + 1 < resolution ) resolution = (int)max_resolutions - (int)j + 1;

  // Make sure our value is possible
  if( resolution > (signed int)(max_resolutions - 1) ) resolution = max_resolutions - 1;
  if( resolution < 0 ) resolution = 0;

}


unsigned int View::getResolution(){

  unsigned int i;

  resolution = max_resolutions - 1;

  if( requested_width ) View::calculateResolution( width, round((float)requested_width/(float)view_width) );
  if( requested_height ) View::calculateResolution( height, round((float)requested_height/(float)view_height) );

  // Caluclate our new width and height based on the calculated resolution
  for( i=0; i < (max_resolutions - resolution - 1); i++ ){
    width = (int) width / 2;
    height = (int) height / 2;
  }

  // Check if we need to use a smaller resolution due to our max size limit
  float scale = getScale();

  if( (width*view_width*scale > max_size) || (height*view_height*scale > max_size) ){
    int dimension;
    if( (width*view_width/max_size) > (height*view_width/max_size) ){
      dimension = (int) (width*view_width*scale);
    }
    else{
      dimension = (int) (height*view_height*scale);
    }

    i = 1;
    while( (dimension / i) > max_size ){
      dimension /= 2;
      width = (int) (width / 2);
      height = (int) (height / 2 );
      resolution--;
    }
  }

  return resolution;

}


float View::getScale(){

  unsigned int rw;
  unsigned int rh;
  if( requested_width == 0 && requested_height > 0 ){
    rw = static_cast<unsigned int>( round(width * requested_height / height) );
  }
  else rw = requested_width;

  if( requested_height == 0 && requested_width > 0 ){
    rh = static_cast<unsigned int>( round( height * requested_width / width ) );
  }
  else rh = requested_height;

  float scale = static_cast<float>(rw) / static_cast<float>(width);

  if( static_cast<float>(rh) / static_cast<float>(height) < scale ) scale = static_cast<float>(rh) / static_cast<float>(height);

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
  if( w > 1.0 ) view_width = 1.0;
  else if( w < 0.0 ) view_width = 0.0;
  else view_width = w;
}


void View::setViewHeight( float h ) {
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
  // Scale up our view to a real pixel value
  unsigned int l = (unsigned int) round(width * view_left);
  return l;
}


unsigned int View::getViewTop(){
  // Scale up our view to a real pixel value
  unsigned int t = (unsigned int) round(height * view_top );
  return t;
}


unsigned int View::getViewWidth(){
  // Scale up our viewport, then make sure our size is not too large or too small
  unsigned int w = (unsigned int) round(width * view_width);
  unsigned int left = (unsigned int) round(width * view_left);
  if( (w + left) > width ) w = width - left;
  if( w < min_size ) w = min_size;
  return w;
}


unsigned int View::getViewHeight(){
  // Scale up our viewport, then make sure our size is not too large or too small
  unsigned int h = (unsigned int) round(height * view_height);
  unsigned int top = (unsigned int) round(height * view_top);
  if( (h + top) > height ) h = height - top;
  if( h < min_size ) h = min_size;
  return h;
}


unsigned int View::getRequestWidth(){

  // If our requested width has not been set, but height has, return a width proportional to
  // this requested height
  unsigned int w = requested_width;
  if( requested_width == 0 ){
    if( requested_height != 0 ){
      w = (unsigned int) round( (float)(getViewWidth()*requested_height) / (float)getViewHeight() );
    }

    // If no width or height has been set, use our full size
    else if( requested_height==0 ) w = width;
  }

  // Limit our requested width to the maximum export size
  if( w > max_size ) w = max_size;

  // If we have a region request, scale our request accordingly
//   if( view_width < 1.0 ){
//     float vw = (view_width + view_left > 1.0) ? 1.0 - view_left : view_width;
//     return (unsigned int) round( requested_width * vw );
//   }

  return w;
}


unsigned int View::getRequestHeight(){

  // If our requested height has not been set, but the width has, return a height proportional to
  // this requested width
  unsigned int h = requested_height;
  if( requested_height == 0 ){
    if( requested_width != 0 ){
      h = (unsigned int) round( (float)(getViewHeight()*requested_width) / (float)getViewWidth() );
    }

    // If no height has been set, use our full size
    else if( requested_width==0 ) h = height;
  }

  // Limit our requested height to the maximum export size
  if( h > max_size ) requested_height = max_size;

  // If we have a region request, scale our request accordingly
//   if( view_height < 1.0 ){
//     float vh = (view_top + view_height > 1.0) ? 1.0 - view_top : view_height;
//     return (unsigned int) round( requested_height * vh );
//   }

  return h;
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
