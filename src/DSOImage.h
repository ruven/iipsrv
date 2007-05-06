
/*  IIP fcgi server module

    Copyright (C) 2000-2003 Ruven Pillay.

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


#ifdef ENABLE_DL

#ifndef _DSOIMAGE_H
#define _DSOIMAGE_H


#include <string>
#include "IIPImage.h"


/// Class to handle dynamically loaded image codecs: Inherits from IIPImage


class DSOImage : public IIPImage{

 private:

  /// Path of the module
  std::string modulePath;

  /// Module description
  std::string description;

  /// Handle to the module
  void *libHandle;

  /// Overloaded function to load the module
  void loadLibrary() throw (std::string);

  /// Overloaded function to unload the module
  void unloadLibrary() throw (std::string);

  /// Get error messages from the module
  std::string getError();


 public:

  /// Constructor
  DSOImage() : IIPImage() { 
    libHandle = NULL;
    tile_width = 0; tile_height = 0;
    numResolutions = 0;
  };

  /// Constructor
  /** \param s image path
   */
  DSOImage( const std::string& s ) : IIPImage( s ) { 
    libHandle = NULL;
    tile_width = 0; tile_height = 0;
    numResolutions = 0;
  };

  /// Copy Constructor
  /** \param image IIPImage object
   */
  DSOImage( const IIPImage& image ) : IIPImage( image ) { 
    libHandle = NULL;
    tile_width = 0; tile_height = 0;
    numResolutions = 0;
  };

  /// Destructor
  ~DSOImage();


  /// Return description of the module
  const std::string& getDescription() { return description; };

  /// Load the module
  /** \param p module path
   */
  void Load( const std::string& p ) throw (std::string);

  /// Overloaded function to open and read the image
  void openImage() throw (std::string);

  /// Overloaded function to close the image
  void closeImage() throw (std::string);


  /// Overloaded function to get a specific tile
  /** \param x horizontal sequence angle
      \param y vertical sequence angle
      \param r resolution
      \param t tile number
   */
  RawTile getTile( int x, int y, int r, int t ) throw (std::string);


};


#endif

#endif
