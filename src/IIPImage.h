// IIPImage class

/*  IIP fcgi server module

    Copyright (C) 2000-2006 Ruven Pillay.

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


#ifndef _IIPIMAGE_H
#define _IIPIMAGE_H



#include <string>
#include <list>
#include <vector>
#include <map>

#include "RawTile.h"



/// Main class to handle the pyramidal image source
/** Provides functions to open, get various information from an image source
    and get individual tiles. This class is the base class for specific image
    file formats, for example, Tiled Pyramidal TIFF images: TPTImage.h
 */

class IIPImage {

 private:

  /// Image path supplied
  std::string imagePath; 

  /// Pattern for sequences
  std::string fileNamePattern;

  /// Indicates whether our image is a single file or part or a sequence
  bool isFile;

  /// Private function to determine the image type
  void testImageType();

  /// If we have a sequence of images, determine which horizontal angles exist
  void measureHorizontalAngles();

  /// If we have a sequence of images, determine which vertical angles exist
  void measureVerticalAngles();

  /// The list of available horizontal angles (for image sequences)
  std::list <int> horizontalAnglesList;

  /// The list of available vertical angles (for image sequences)
  std::list <int> verticalAnglesList;



 protected: 

  /// Return the image type e.g. tif
  std::string type;

  /// The image pixel dimensions
  unsigned int image_width, image_height;

  /// The base tile pixel dimensions
  unsigned int tile_width, tile_height;

  /// The colour space of the image
  ColourSpaces colourspace;

  /// The number of available resolutions in this image
  int numResolutions;

  /// The bits per pixel for this image
  unsigned int bpp;

  /// The number of channels for this image
  unsigned int channels;

  /// Indicate whether we have opened and initialised some paramters for this image
  bool isSet;

  /// If we have an image sequence, the current X and Y position
  int currentX, currentY;

  /// STL map to hold string metadata
  std::map <const std::string, std::string> metadata;



 public:

  /// Default Constructor
  IIPImage();

  /// Constructer taking the image path as paramter
  IIPImage( const std::string& );

  /// Copy Constructor taking reference to another IIPImage object
  IIPImage( const IIPImage& );

  /// Virtual Destructor
  virtual ~IIPImage() { ; };

  /// Test the image and initialise some parameters
  void Initialise();

  /// Return a list of available vertical angles
  std::list <int> getVerticalViewsList();

  /// Return a list of horizontal angles
  std::list <int> getHorizontalViewsList();

  /// Return the image path
  const std::string& getImagePath() { return imagePath; };

  /// Return the full file path for a particular horizontal and vertical angle
  /** \param x horizontal sequence angle
      \param y vertical sequence angle
   */
  std::string getFileName( int x, int y );

  /// Get the image type
  std::string getImageType() { return type; };

  /// Check whether this object has been initialised
  bool set() { return isSet; };

  /// Set the file name pattern used in image sequences
  void setFileNamePattern( std::string pattern ) { fileNamePattern = pattern; };

  /// Return the number of available resolutions in the image
  int getNumResolutions() { return numResolutions; };

  /// Return the number of bits per pixel for this image
  unsigned int getNumBitsPerPixel() { return bpp; };

  /// Return the number of channels for this image
  unsigned int getNumChannels() { return channels; };

  /// Return the image width in pixels
  unsigned int getImageWidth() { return image_width; };

  /// Return the image height in pixels
  unsigned int getImageHeight() { return image_height; };

  /// Return the base tile height in pixels
  unsigned int getTileHeight() { return tile_height; };

  /// Return the base tile width in pixels
  unsigned int getTileWidth() { return tile_width; };

  /// Return the colour space for this image
  ColourSpaces getColourSpace() { return colourspace; };

  /// Return image metadata
  /** \param index metadata field name */
  std::string getMetadata( const std::string index ) {
    return metadata[index];
  };



  /// Load the appropriate codec module for this image type
  /** Used only for dynamically loading codec modules. Overloaded by DSOImage class.
      \param module the codec module path
   */
  virtual void Load( const std::string& module ) {;};

  /// Return codec description: Overloaded by child class.
  virtual const std::string getDescription() { return std::string( "IIPImage Base Class" ); };

  /// Open the image: Overloaded by child class.
  virtual void openImage() { throw std::string( "IIPImage openImage called" ); };

  /// Load information about the image eg. number of channels, tile size etc.
  /** \param x horizontal sequence angle
      \param y vertical sequence angle
   */
  virtual void loadImageInfo( int x, int y ) { ; };

  /// Close the image: Overloaded by child class.
  virtual void closeImage() {;};


  /// Return a tile for a given angle and resolution
  /** Return a RawTile object: Overloaded by child class.
      \param h horizontal angle
      \param v vertical angle
      \param r resolution
      \param t tile number
   */
  virtual RawTile getTile( int h, int v, unsigned int r, unsigned int t ) { return RawTile(); };


  /// Assignment operator
  const IIPImage& operator = ( const IIPImage& );

  /// Comparison equality operator
  friend int operator == ( const IIPImage&, const IIPImage& );

  /// Comparison non-equality operator
  friend int operator != ( const IIPImage&, const IIPImage& );


};



#endif
