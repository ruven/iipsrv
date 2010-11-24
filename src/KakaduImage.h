// Kakadu JPEG2000 Image class Interface

/*  IIP Kakadu JPEG2000 Class


    Development supported by Moravian Library in Brno (Moravska zemska 
    knihovna v Brne, http://www.mzk.cz/) R&D grant MK00009494301 & Old 
    Maps Online (http://www.oldmapsonline.org/) from the Ministry of 
    Culture of the Czech Republic. 


    Copyright (C) 2009-2010 IIPImage.
    Authors: Ruven Pillay & Petr Pridal

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


#ifndef _KAKADUIMAGE_H
#define _KAKADUIMAGE_H


#include "IIPImage.h"
#include <cstdio>

#include <jpx.h>
#include <jp2.h>
#include <kdu_stripe_decompressor.h>
#include <iostream>
#include <fstream>

#define TILESIZE 256


extern std::ofstream logfile;


class kdu_stream_message : public kdu_message {
 private: // Data
  std::ostream *stream;
  std::string message;

 public: // Member classes
  kdu_stream_message(std::ostream *stream)
    { this->stream = stream; }
  void put_text(const char *string)
  { logfile << string; }
  void flush(bool end_of_message=false){
    logfile << message;
    if( end_of_message ) throw 1;
  }
};


//static kdu_stream_message cout_message(&std::cout);
//static kdu_stream_message cerr_message(&std::cerr);

static kdu_stream_message cout_message(&logfile);
static kdu_stream_message cerr_message(&logfile);

static kdu_message_formatter pretty_cout(&cout_message);
static kdu_message_formatter pretty_cerr(&cerr_message);





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

  // Tile or Strip region
  kdu_dims comp_dims;

  // Quality layers
  unsigned int max_layers;

  /// Number of levels that don't physically exist
  unsigned int virtual_levels;


 public:

  /// Constructor
 KakaduImage():IIPImage() { 
    tile_width = TILESIZE; tile_height = TILESIZE;
    numResolutions = 0; virtual_levels = 0;
  };

  /// Constructor
  /** \param path image path
   */
 KakaduImage( const std::string& path ): IIPImage( path ) { 
    tile_width = TILESIZE; tile_height = TILESIZE;
    numResolutions = 0; virtual_levels = 0;
  };

  /// Copy Constructor
  /** \param image IIPImage object
   */
 KakaduImage( const IIPImage& image ): IIPImage( image ) {
    tile_width = TILESIZE; tile_height = TILESIZE;
    numResolutions = 0; virtual_levels = 0;
  };

  /// Destructor
  ~KakaduImage() { closeImage(); };

  /// Overloaded function for opening a TIFF image
  void openImage() throw (std::string);


  /// Overloaded function for loading TIFF image information
  /** \param x horizontal sequence angle
      \param y vertical sequence angle
  */
  void loadImageInfo( int x, int y ) throw (std::string);

  /// Overloaded function for closing a TIFF image
  void closeImage();

  /// Overloaded function for getting a particular tile
  /** \param x horizontal sequence angle
      \param y vertical sequence angle
      \param r resolution
      \param l quality layers
      \param t tile number
  */
    RawTile getTile( int x, int y, unsigned int r, int l, unsigned int t ) throw (std::string);

  /// Overloaded function for returning a region for a given angle and resolution
  /** Return a RawTile object: Overloaded by child class.
      \param ha horizontal angle
      \param va vertical angle
      \param r resolution
      \param x
      \param y
      \param w
      \param h
  */
  void getRegion( int ha, int va, unsigned int r, int layers, int x, int y, int w, int h, unsigned char* b ) throw (std::string);

  void process( int, int,int, int, int, int, void* ) throw (std::string);


  void startStrip( int vipsres, int layers, int xoffset, int yoffset, int tw, int th, void *d );
  int processStrip(unsigned char* buffer);
  void finishStrip();

};


#endif
