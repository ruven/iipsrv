/*
    Image View Parameters

    Copyright (C) 2003-2009 Ruven Pillay.

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


#ifndef _VIEW_H
#define _VIEW_H


/// Class to intelligently handle Image Transforms

class View{


 private:

  // Resolution independent x,y,w,h viewport
  float view_left, view_top, view_width, view_height;

  int resolution;                             /// Requested resolution
  unsigned int max_resolutions;               /// Total available resolutions
  unsigned int left, top, width, height;      /// Requested width and height
  unsigned int min_size;                      /// Minimum viewport dimension
  unsigned int max_size;                      /// Maximum viewport dimension
  unsigned int requested_width;               /// Width requested by WID command
  unsigned int requested_height;              /// Height requested by HEI command
  float contrast;                             /// Contrast adjustment requested by CNT command

  /// Internal function to calculate the resolution associated with a width
  ///  or height request. This also takes into account maximum size limits.
  /** \param m maximum size
      \param r requested size
   */
  void calculateResolution( unsigned int m, unsigned int r );


 public:

  int xangle;                                  /// Horizontal View
  int yangle;                                  /// Vertical View
  bool shaded;                                 /// Whether to use shading view
  int shade[3];                                /// Shading incident light angles (x,y,z)
  int layers;			               /// Number of quality layers

  /// Constructor
  View() {
    resolution = 0; max_resolutions = 0; min_size = 8; max_size = 0;
    width = 0; height = 0;
    view_left = 0.0; view_top = 0.0; view_width = 1.0; view_height = 1.0;
    requested_width = 0; requested_height = 0;
    contrast = 1.0;
    xangle = 0; yangle = 90;
    shaded = false; shade[0] = 0; shade[1] = 0; shade[2] = 0;
    layers = 1;
  };


  /// Set the contrast adjustment
  /** \param c contrast (where 1.0 is no adjustment) */
  void setContrast( float c ){ contrast = c; };


  /// Set the maximum view port dimension
  /** \param m maximum viewport dimension */
  void setMaxSize( unsigned int m ){ max_size = m; };


  /// Set the maximum view port dimension
  /** \param r number of availale resolutions */
  void setMaxResolutions( unsigned int r ){ max_resolutions = r; };


  /// Set the size of the requested width
  /** \param w requested image width */
  void setRequestWidth( unsigned int w ){ requested_width = w; };


  /// Set the size of the requested height
  /** \param h requested image height */
  void setRequestHeight( unsigned int h ){ requested_height = h; };


  /// Return the requested resolution
  unsigned int getResolution();


  /// Set the left co-ordinate of the viewport
  /** \param x left resolution independent co-ordinate */
  void setViewLeft( float x );


  /// Set the top co-ordinate of the viewport
  /** \param y top resolution independent co-ordinate */
  void setViewTop( float y );


  /// Set the width co-ordinate of the viewport
  /** \param w width resolution independent co-ordinate */
  void setViewWidth( float w );


  /// Set the height co-ordinate of the viewport
  /** \param h height resolution independent co-ordinate */
  void setViewHeight( float h );


  /// Set the source image pixel size
    /** \param w pixel width
	\param h pixel height
    */
  void setImageSize( unsigned int w, unsigned int h ){ width = w; height = h; };


  /// Set the number of quality layers to decode
  /** \param l Number of layers to decode */
  void setLayers( int l ){ layers = l; };


  /// Return the contrast adjustment
  float getContrast(){ return contrast; };

  /// Return the image width at our requested resolution
  unsigned int getImageWidth(){ return width; };

  /// Return the image height at our requested resolution
  unsigned int getImageHeight(){ return height; };

  /// Return the left pixel of the viewport
  unsigned int getViewLeft() ;

  /// Return the top pixel of the viewport
  unsigned int getViewTop();

  /// Return the pixel width of the viewport
  unsigned int getViewWidth();

  /// Return the pixel height of the viewport
  unsigned int getViewHeight();

  /// Return the number of layers to decode
  unsigned int getLayers(){ return layers; };

  /// Indicate whether the viewport has been set
  bool viewPortSet();


};


#endif
