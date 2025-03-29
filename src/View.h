/*
    Image View and Transform Parameters

    Copyright (C) 2003-2025 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/


#ifndef _VIEW_H
#define _VIEW_H


#include <cstddef>
#include <vector>

#include "Transforms.h"




/// Class to intelligently handle Image Transforms

class View{


 private:

  // Resolution independent x,y,w,h region viewport in range 0 -> 1.0
  float view_left, view_top, view_width, view_height; /// viewport

  int resolution;                             ///< Requested resolution where 0 is smallest available
  unsigned int max_resolutions;               ///< Total available resolutions
  unsigned int width, height;                 ///< Image width and height at full resolution
  unsigned int res_width, res_height;         ///< Width and height at requested resolution
  unsigned int min_size;                      ///< Minimum viewport dimension
  int max_size;                               ///< Maximum viewport dimension
  unsigned int requested_width;               ///< Width requested by WID command
  unsigned int requested_height;              ///< Height requested by HEI command
  float rotation;                             ///< Rotation requested by ROT command


  /// Internal function to calculate the optimal resolution associated with a width
  ///  or height request. This also takes into account maximum & minimum size limits.
  /** @param m maximum size
      @param r requested size
   */
  void calculateResolution( unsigned int m, unsigned int r );


 public:

  int xangle;                                 ///< Horizontal View
  int yangle;                                 ///< Vertical View
  bool shaded;                                ///< Whether to use shading view
  int shade[3];                               ///< Shading incident light angles (x,y,z)
  bool cmapped;                               ///< Whether to modify colormap
  enum cmap_type cmap;                        ///< colormap
  bool inverted;                              ///< Whether to invert colormap
  int max_layers;                             ///< Maximum number of quality layers allowed
  int layers;                                 ///< Number of quality layers
  ColorSpace colorspace;                      ///< Requested colorspace
  std::vector< std::vector<float> > ctw;      ///< Colour twist matrix
  int flip;                                   ///< Flip (1=horizontal, 2=vertical)
  bool maintain_aspect;                       ///< Indicate whether aspect ratio should be maintained
  bool allow_upscaling;                       ///< Indicate whether images may be served larger than the source file
  int max_icc;                                ///< Maximum ICC profile size we allow to be embedded
  ImageEncoding output_format;                ///< Requested output format
  float contrast;                             ///< Contrast adjustment requested by CNT command
  float gamma;                                ///< Gamma adjustment requested by GAM command
  std::vector<float> convolution;             ///< Convolution matrix
  bool equalization;                          ///< Whether to perform histogram equalization
  bool minmax;                                ///< Whether to perform contrast stretching using user-defined min/max


  /// Constructor
  View() {
    view_left = 0.0; view_top = 0.0; view_width = 1.0; view_height = 1.0;
    resolution = 0; max_resolutions = 0;
    width = 0; height = 0;
    res_width = 0; res_height = 0;
    min_size = 1; max_size = 0;
    requested_width = 0; requested_height = 0;
    contrast = 1.0; gamma = 1.0;
    xangle = 0; yangle = 90;
    shaded = false; shade[0] = 0; shade[1] = 0; shade[2] = 0;
    cmapped = false; cmap = HOT; inverted = false;
    max_layers = 0; layers = 0;
    rotation = 0.0; flip = 0;
    maintain_aspect = true;
    allow_upscaling = true;
    colorspace = ColorSpace::NONE;
    max_icc = -1;
    output_format = ImageEncoding::JPEG;
    equalization = false;
    minmax = false;
  };


  /// Set the maximum view port dimension
  /** @param m maximum viewport dimension */
  void setMaxSize( int m ){ max_size = m; };


  /// Get the maximum allowed output size
  /** @return maximum output dimension */
  int getMaxSize(){ return max_size; };


  /// Get the minimum allowed output size
  /** @return minimum output dimension */
  unsigned int getMinSize(){ return min_size; };


  /// Set the allow_upscaling flag
  /** @param upscale allow upscaling of source image */
  void setAllowUpscaling( bool upscale ){ allow_upscaling = upscale; };


  /// Get the allow_upscaling flag
  /** @return true or false */
  bool allowUpscaling(){ return allow_upscaling; };


  /// Set the maximum ICC profile size we allow to be embedded
  /** @param max maximum icc profile size
   */
  void setMaxICC( int max ){ max_icc = max; };


  /// Get the maximum ICC profile size we allow to be embedded - disable if certain processing has been carried out
  /** @return max ICC profile size
   */
  int maxICC(){
    // Disable if colour-mapping, twist, hill-shading or greyscale conversion applied
    if( cmapped || shaded || ctw.size() || colorspace==ColorSpace::GREYSCALE ) return 0;
    return max_icc;
  }


  /// Set the maximum view port dimension
  /** @param r number of availale resolutions */
  void setMaxResolutions( unsigned int r ){ max_resolutions = r; resolution=r-1; };


  /// Set the size of the requested width
  /** @param w requested image width */
  void setRequestWidth( unsigned int w ){
    requested_width = w;
  };


  /// Get requested image size
  /** @return output size as a vector */
  std::vector<unsigned int> getRequestSize();


  /// Set the size of the requested height
  /** @param h requested image height */
  void setRequestHeight( unsigned int h ){
    requested_height = h;
  };


  /// Return the resolution level needed for the requested view
  /** @return requested resolution level */
  unsigned int getResolution();


  /// Return the scaling required in case our requested width or height is in between available resolutions
  /** @return scaling factor */
  float getScale();


  /// Set the left co-ordinate of the viewport
  /** @param x left resolution independent co-ordinate */
  void setViewLeft( float x );


  /// Set the top co-ordinate of the viewport
  /** @param y top resolution independent co-ordinate */
  void setViewTop( float y );


  /// Set the width co-ordinate of the viewport
  /** @param w width resolution independent co-ordinate */
  void setViewWidth( float w );


  /// Set the height co-ordinate of the viewport
  /** @param h height resolution independent co-ordinate */
  void setViewHeight( float h );


  /// Return the view dimensions scaled to the full resolution of the image
  /** @return size view dimensions on the full resolution canvas packed into a vector */
  std::vector<float> getViewSize(){
    std::vector<float> size = { width*view_width, height*view_height };
    return size;
  };


  /// Set the source image pixel size
  /** @param w pixel width
      @param h pixel height
   */
  void setImageSize( unsigned int w, unsigned int h ){ width = w; height = h; };


  /// Limit the maximum number of quality layers we are allowed to decode
  /** @param l Max number of layers to decode */
  void setMaxLayers( int l ){ max_layers = l; };

  /// Set the number of quality layers to decode, limiting to our max value
  /** @param l Number of layers to decode */
  void setLayers( int l ){ layers = l; };

  /// Return the number of layers to decode
  int getLayers();

  /// Return the image width at our requested resolution
  /** @return image width */
  unsigned int getImageWidth(){ return width; };

  /// Return the image height at our requested resolution
  /** @return image height */
  unsigned int getImageHeight(){ return height; };

  /// Return the left pixel of the viewport
  /** @return position of left of viewport in pixels */
  unsigned int getViewLeft() ;

  /// Return the top pixel of the viewport
  /** @return position of top of viewport in pixels */
  unsigned int getViewTop();

  /// Return the pixel width of the viewport
  /** @return width of viewport in pixels */
  unsigned int getViewWidth();

  /// Return the pixel height of the viewport
  /** @return height of viewport in pixels */
  unsigned int getViewHeight();

  /// Indicate whether the viewport has been set
  /** @return boolean indicating whether viewport specified */
  bool viewPortSet();

  /// Set rotation
  /** @param r angle of rotation in degrees */
  void setRotation( float r ){ rotation = r; };

  /// Get rotation
  /** @return requested rotation angle in degrees */
  float getRotation(){ return rotation; };

  /// Whether view requires floating point processing
  bool floatProcessing(){
    if( contrast != 1.0 || gamma != 1.0 || cmapped || shaded || inverted || minmax || ctw.size() || convolution.size() ){
      return true;
    }
    else return false;
  }

  /// Whether we require a histogram
  bool requireHistogram(){
    if( equalization || colorspace==ColorSpace::BINARY || contrast==-1 ) return true;
    else return false;
  }

};


#endif
