/*
    IIPImage Server - Watermark Class

    Enables dynamic watermarking of images with user-defined opacity and
    random positioning within the image.

    Development supported by Moravian Library in Brno (Moravska zemska
    knihovna v Brne, http://www.mzk.cz/) R&D grant MK00009494301 & Old
    Maps Online (http://www.oldmapsonline.org/) from the Ministry of
    Culture of the Czech Republic.


    Copyright (C) 2010-2025 Ruven Pillay.

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



#ifndef _WATERMARK_H
#define _WATERMARK_H

#include <string>



/// Watermark class

class Watermark {

 private:

  /// Width of watermark image
  unsigned int _width;

  /// Height of watermark image
  unsigned int _height;

  /// Number of colour channels in image
  unsigned int _channels;

  /// Number of bits per channel
  unsigned int _bpc;

  /// Watermark file
  std::string _image;

  /// Watermark opacity
  float _opacity;

  /// Watermark probability
  float _probability;

  /// Whether we have a valid watermark
  bool _isSet;

  /// Our watermark buffer
  unsigned char* _watermark;


 public:

  /// Constructor
  Watermark() :
    _opacity( 0.0 ),
    _probability( 0.0 ),
    _isSet( false ),
    _watermark( NULL ){};

  /// Constructor
  /** @param file image file path
      @param opacity opacity applied to watermark
      @param probability probability (in range 0.0 -> 1.0) that watermark will be applied to a particular tile
   */
  Watermark( const std::string& file, float opacity, float probability ) :
    _width( 0 ),
    _height( 0 ),
    _channels( 0 ),
    _bpc( 0 ),
    _image( file ),
    _opacity( opacity ),
    _probability( probability ),
    _isSet( false ),
    _watermark( NULL ) {};

  /// Destructor
  ~Watermark(){
    if( _watermark ) delete[] _watermark;
  };

  /// Apply the watermark to a data buffer
  /** @param data buffer of image data
      @param width tile width
      @param height tile height
      @param channels number of channels
      @param bpc bits per channel (8 or 16)
      @param block size in pixels of square block size used for watermark tiling (optional).
             By default image size is used and only a single watermark applied
   */
  void apply( void* data, unsigned int width, unsigned int height, unsigned int channels, unsigned int bpc, unsigned int block=0 );

  /// Return watermark image path
  std::string getImage(){ return _image; };

  /// Return watermark opacity
  float getOpacity(){ return _opacity; };

  /// Return watermark probability
  float getProbability(){ return _probability; };

  /// Initialize our watermark image
  void init();

  /// Determine whether a watermark has been specified
  bool isSet(){
    if( _isSet ) return true;
    else return false;
  }

};



#endif
