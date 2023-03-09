// Kakadu JPEG2000 Image class Interface

/*  IIP Kakadu JPEG2000 Class


    Development supported by Moravian Library in Brno (Moravska zemska
    knihovna v Brne, http://www.mzk.cz/) R&D grant MK00009494301 & Old
    Maps Online (http://www.oldmapsonline.org/) from the Ministry of
    Culture of the Czech Republic.


    Copyright (C) 2009-2022 IIPImage.
    Author: Ruven Pillay

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


#ifndef _KAKADUIMAGE_H
#define _KAKADUIMAGE_H


#include "IIPImage.h"

#include <jpx.h>
#include <jp2.h>
#include <kdu_stripe_decompressor.h>
#include <fstream>

#define TILESIZE 256

// Kakadu 7.5 uses namespaces
#if KDU_MAJOR_VERSION > 7 || (KDU_MAJOR_VERSION == 7 && KDU_MINOR_VERSION >= 5)
using namespace kdu_supp; // Also includes the `kdu_core' namespace
#endif



/// Image class for Kakadu JPEG2000 Images: Inherits from IIPImage. Uses the Kakadu library.
class KakaduImage : public IIPImage {

 private:

  /// Kakadu codestream object
  kdu_codestream codestream;

  /// Codestream source
  kdu_compressed_source *input;

  /// JPX format object
  jpx_source jpx_input;

  /// JP2 file format object
  jp2_family_src src;

  /// JPX codestream source
  jpx_codestream_source jpx_stream;

  /// Kakadu decompressor object
  kdu_stripe_decompressor decompressor;

  /// Tile or Strip region
  kdu_dims comp_dims;

  /// Main processing function
  /** @param r resolution
      @param l number of quality levels to decode
      @param x x coordinate
      @param y y coordinate
      @param w width of region
      @param h height of region
      @param d buffer to fill
   */
  void process( unsigned int r, int l, int x, int y, unsigned int w, unsigned int h, void* d );

  /// Convenience function to delete allocated buffers
  /** @param b pointer to buffer
   */
  void delete_buffer( void* b );


 public:

  /// Constructor
  KakaduImage(): IIPImage(){
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE); input = NULL;
  };

  /// Constructor
  /** @param path image path
   */
  KakaduImage( const std::string& path ): IIPImage( path ){
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE); input = NULL;
  };

  /// Copy Constructor
  /** @param image Kakadu object
   */
  KakaduImage( const KakaduImage& image ): IIPImage( image ) {};

  /// Constructor from IIPImage object
  /** @param image IIPImage object
   */
  KakaduImage( const IIPImage& image ): IIPImage( image ){
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE); input = NULL;
  };

  /// Assignment Operator
  /** @param image object
   */
  KakaduImage& operator = ( KakaduImage image ) {
    if( this != &image ){
      closeImage();
      IIPImage::operator=(image);
    }
    return *this;
  }


  /// Destructor
  ~KakaduImage() { closeImage(); };


  /// Overloaded static function for seting up logging for codec library
  static void setupLogging();


  /// Overloaded function for opening a TIFF image
  void openImage();

  /// Overloaded function for loading TIFF image information
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
   */
  void loadImageInfo( int x, int y );

  /// Overloaded function for closing a JPEG2000 image
  void closeImage();

  /// Return whether this image type directly handles region decoding
  bool regionDecoding(){ return true; };

  /// Overloaded function for getting a particular tile
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
      @param r resolution
      @param l number of quality layers to decode
      @param t tile number
   */
  RawTile getTile( int x, int y, unsigned int r, int l, unsigned int t );

  /// Overloaded function for returning a region for a given angle and resolution
  /** Return a RawTile object: Overloaded by child class.
      @param ha horizontal angle
      @param va vertical angle
      @param r resolution
      @param l number of quality layers to decode
      @param x x coordinate
      @param y y coordinate
      @param w width of region
      @param h height of region
      @return RawTile image
   */
  RawTile getRegion( int ha, int va, unsigned int r, int l, int x, int y, unsigned int w, unsigned int h );

  /// Read-mode types
  enum KDU_READMODE { KDU_FAST,     ///< Default fast mode
		      KDU_FUSSY,    ///< Fussy mode
		      KDU_RESILIENT ///< Reslient mode for damaged JP2 streams
  };

  /// Read-mode
  KDU_READMODE kdu_readmode;


};


#endif
