/*
    View Member Functions

    Copyright (C) 2004-2026 Ruven Pillay.

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
#include <limits>
using namespace std;


// Round to an unsigned int only when the rounded result is representable.
static bool roundToInt( double value, unsigned int& result ){
  if( !std::isfinite(value) ) return false;

  double rounded = std::round( value );
  if( rounded < 0.0 ||
      rounded > (double) numeric_limits<unsigned int>::max() ) return false;

  result = (unsigned int) rounded;
  return true;
}



/// Calculate the optimal resolution and the size of this resolution for the requested view,
/// taking into account any maximum size settings
unsigned int View::getResolution( const std::vector<unsigned int>& widths, const std::vector<unsigned int>& heights ){

  int level;

  // Get requested size - takes into account max size
  vector<unsigned int> requested_size = View::getRequestSize();

  // Start from the smallest resolution size (last item in size array)
  for( level = (int) max_resolutions - 1; level >= 0; level-- ){

    // Scaling factor (assume powers of 2)
    unsigned int factor = 1 << level;

    // For some reason, this needs to be done in 2 steps with fw, fh first, otherwise rounding errors occur :-/
    float fw = (float) width * view_width;
    // For full width or height requests use the real level size.
    // Also add an epsilon to avoid floating-point rounding errors when using floor()
    unsigned int scaled_width = (view_width == 1.0) ? widths[level] : floor( (fw/factor) + numeric_limits<float>::epsilon() );
    float fh = (float) height * view_height;
    unsigned int scaled_height = (view_height == 1.0) ? heights[level] : floor( (fh/factor) + numeric_limits<float>::epsilon() );

    // Note that we add 1px for the first checks to take into account differences between levels that have rounded up or down
    if( scaled_width <= (widths[level]+1) && scaled_height <= (heights[level]+1) &&
	// Skip the check only when a single dimension has been requested; a request with neither dimension
	// set must still fit the request
	( requested_width == 0 && requested_height > 0 ? true : scaled_width >= requested_size[0] ) &&
	( requested_height == 0 && requested_width > 0 ? true : scaled_height >= requested_size[1] ) ){
      break; // Lowest resolution where conditions are met
    }
  }
  // Clamp to 0 if no matches
  if( level < 0 ) level = 0;

  // Size of image at this level
  res_width = widths[level];
  res_height = heights[level];

  // Store resolution as IIP resolution (0=smallest)
  resolution = max_resolutions - level - 1;

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

    while( resolution > 0 && ( dimension > max_size ) ){
      dimension = (int) (dimension/2.0);
      res_width = widths[max_resolutions - resolution];
      res_height = heights[max_resolutions - resolution];
      resolution--;
    }
  }

  return resolution;

}


float View::getScale(){

  // No resolution has been set yet
  if( res_width == 0 || res_height == 0 ) return 1.0;

  unsigned int rw;
  unsigned int rh;
  if( requested_width == 0 && requested_height > 0 ){
    if( !roundToInt( (double)res_width * requested_height / res_height, rw ) ) rw = width;
  }
  else rw = requested_width;

  if( requested_height == 0 && requested_width > 0 ){
    if( !roundToInt( (double)res_height * requested_width / res_width, rh ) ) rh = height;
  }
  else rh = requested_height;


  // Scale is the resampling ratio between the requested size and the size of
  // the region at the selected resolution level (level size adjusted for any
  // viewport)
  float scale = static_cast<float>(rw) / (res_width * view_width);

  if( static_cast<float>(rh) / (res_height * view_height) < scale ){
    scale = static_cast<float>(rh) / (res_height * view_height);
  }


  // Sanity check
  if( scale <= 0 || scale > 1.0 ) scale = 1.0;
  return scale;

}


void View::setViewLeft( double x ) {
  if( !std::isfinite(x) || x < 0.0 ) view_left = 0.0;
  else if( x > 1.0 ) view_left = 1.0;
  else view_left = x;
}


void View::setViewTop( double y ) {
  if( !std::isfinite(y) || y < 0.0 ) view_top = 0.0;
  else if( y > 1.0 ) view_top = 1.0;
  else view_top = y;
}


void View::setViewWidth( double w ) {
  // Sanity check
  if( w > 1.0 ) view_width = 1.0;
  else if( !std::isfinite(w) || w <= 0.0 ) view_width = (double) min_size / std::max( 1u, width );
  else view_width = w;
}


void View::setViewHeight( double h ) {
  // Sanity check
  if( h > 1.0 ) view_height = 1.0;
  else if( !std::isfinite(h) || h <= 0.0 ) view_height = (double) min_size / std::max( 1u, height );
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
  unsigned int l;
  if( !roundToInt( (double)width * view_left / (1 << (max_resolutions-resolution-1)), l ) ) l = 0;
  // As we use round(), l can be 1px larger than the stored resolution width
  // (encoders may floor or ceil when building pyramid levels) - keep the clamp.
  // Leave room for min_size as well: getViewWidth() uses this value as the
  // rectangle origin, so l == res_width would force a zero width that
  // min_size then pads to one pixel past the image edge.
  unsigned int max_left = (res_width > min_size) ? res_width - min_size : 0;
  if( l > max_left ) l = max_left;
  return l;
}


unsigned int View::getViewTop(){
  // Scale up our view to a real pixel value
  // Note that we calculate from our full resolution image to avoid errors from the rounding at each resolution size
  unsigned int t;
  if( !roundToInt( (double)height * view_top / (1 << (max_resolutions-resolution-1)), t ) ) t = 0;
  // As above, round() can overshoot the stored resolution height by a pixel.
  unsigned int max_top = (res_height > min_size) ? res_height - min_size : 0;
  if( t > max_top ) t = max_top;
  return t;
}


unsigned int View::getViewWidth(){

  // Scale up our viewport, then make sure our size is not too large or too small
  float scale = (float) width / (1<<(max_resolutions-resolution-1));   // Calculate exact scale from largest resolution
  unsigned int w;
  if( !roundToInt( (double)view_width * scale, w ) ) w = res_width;

  // Use the exact same origin as getViewLeft(). Recomputing it here with a
  // different expression can differ by a pixel and lets the rectangle escape
  // the image even when each getter looks in bounds on its own.
  unsigned int left = getViewLeft();
  unsigned int available = (res_width > left) ? res_width - left : 0;
  if( w > available ) w = available;
  if( w < min_size && available >= min_size ) w = min_size;

  return w;
}


unsigned int View::getViewHeight(){

  // Scale up our viewport, then make sure our size is not too large or too small
  float scale = (float) height / (1<<(max_resolutions-resolution-1));  // Calculate exact scale from largest resolution
  unsigned int h;
  if( !roundToInt( (double)view_height * scale, h ) ) h = res_height;

  // Use the exact same origin as getViewTop() (see getViewWidth()).
  unsigned int top = getViewTop();
  unsigned int available = (res_height > top) ? res_height - top : 0;
  if( h > available ) h = available;
  if( h < min_size && available >= min_size ) h = min_size;

  return (unsigned int) h;
}


vector<unsigned int> View::getRequestSize(){

  unsigned int w = requested_width;
  unsigned int h = requested_height;
  // Non-finite or unrepresentable results fall back to the full image extent;
  // the max_size clamp at the end of this function still applies.

  // Calculate aspect ratio using double precision
  double ratio = (view_height * height > 0.0 && view_width * width > 0.0)
               ? (view_width * width) / (view_height * height)
               : 1.0;

  if( requested_width == 0 && requested_height != 0 ){
    if( !roundToInt( (double)requested_height * ratio, w ) ) w = width;
    if( w == 0 ) w = 1;
  }
  else if( requested_height == 0 && requested_width != 0 ){
    if( !roundToInt( (double)requested_width / ratio, h ) ) h = height;
    if( h == 0 ) h = 1;
  }
  else if( requested_width == 0 && requested_height == 0 ){
    // No explicit size: use the viewport size at full resolution,
    // falling back to the full image extent when no region is set
    if( viewPortSet() ){
      if( !roundToInt( (double)view_width * width, w ) ) w = width;
      if( !roundToInt( (double)view_height * height, h ) ) h = height;
      if( w == 0 ) w = 1;
      if( h == 0 ) h = 1;
    }
    else{
      w = width;
      h = height;
    }
  }
  // If both width and height are set, restrict image to this bounding box
  else if( requested_width != 0 && requested_height != 0 && maintain_aspect ){
    double xscale = (view_width * width > 0.0) ? (double)requested_width / (view_width * width) : 1.0;
    double yscale = (view_height * height > 0.0) ? (double)requested_height / (view_height * height) : 1.0;
    // Fit to the axis requiring the most scaling (smallest factor)
    if( xscale > yscale ){
      if( !roundToInt( (double)requested_height * ratio, w ) ) w = width;
      if( w == 0 ) w = 1;
    }
    else{
      if( !roundToInt( (double)requested_width / ratio, h ) ) h = height;
      if( h == 0 ) h = 1;
    }
  }


  // Limit our requested size to the maximum output size
  if( max_size > 0 && (w > (unsigned int) max_size || h > (unsigned int) max_size) ){
    // Explicit two-dimensional requests with maintain_aspect disabled are
    // capped independently on each axis
    if( requested_width != 0 && requested_height != 0 && !maintain_aspect ){
      if( w > (unsigned int) max_size ) w = max_size;
      if( h > (unsigned int) max_size ) h = max_size;
    }
    else if( w > h ){
      w = max_size;
      if( !roundToInt( (double) w / ratio, h ) || h > (unsigned int) max_size ) h = max_size;
    }
    else if( h > w ){
      h = max_size;
      if( !roundToInt( (double) h * ratio, w ) || w > (unsigned int) max_size ) w = max_size;
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
