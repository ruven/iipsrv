// IIPImage class

/*  IIP fcgi server module

    Copyright (C) 2000-2009 Ruven Pillay.

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

  /// Prefix to add to paths
  std::string fileSystemPrefix;

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



  //protected: 
  public:

  /// Return the image type e.g. tif
  std::string type;

  /// The image pixel dimensions
  std::vector<unsigned int> image_widths, image_heights;

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

  /// Image modification timestamp
  time_t timestamp;


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
  std::list <int> getVerticalViewsList(){ return verticalAnglesList; };

  /// Return a list of horizontal angles
  std::list <int> getHorizontalViewsList(){ return horizontalAnglesList; };

  /// Return the image path
  const std::string& getImagePath() { return imagePath; };

  /// Return the full file path for a particular horizontal and vertical angle
  /** \param x horizontal sequence angle
      \param y vertical sequence angle
   */
  const std::string getFileName( int x, int y );

  /// Get the image type
  const std::string& getImageType() { return type; };

  /// Get the image timestamp
  void updateTimestamp( const std::string& );

  /// Get a HTTP RFC 1123 formatted timestamp
  const std::string getTimestamp();

  /// Check whether this object has been initialised
  bool set() { return isSet; };

  /// Set a file system prefix for added security
  void setFileSystemPrefix( const std::string& prefix ) { fileSystemPrefix = prefix; };

  /// Set the file name pattern used in image sequences
  void setFileNamePattern( const std::string& pattern ) { fileNamePattern = pattern; };

  /// Return the number of available resolutions in the image
  int getNumResolutions() { return numResolutions; };

  /// Return the number of bits per pixel for this image
  unsigned int getNumBitsPerPixel() { return bpp; };

  /// Return the number of channels for this image
  unsigned int getNumChannels() { return channels; };

  /// Return the image width in pixels for a given resolution
  /** \param n resolution number (0 is default and full size image)
   */
  unsigned int getImageWidth( int n=0) { return image_widths[n]; };

  /// Return the image height in pixels for a given resolution
  /** \param n resolution number (0 is default and full size image)
   */
  unsigned int getImageHeight( int n=0 ) { return image_heights[n]; };

  /// Return the base tile height in pixels for a given resolution

  unsigned int getTileHeight() { return tile_height; };

  /// Return the base tile width in pixels
  unsigned int getTileWidth() { return tile_width; };

  /// Return the colour space for this image
  ColourSpaces getColourSpace() { return colourspace; };

  /// Return image metadata
  /** \param index metadata field name */
  std::string getMetadata( const std::string& index ) {
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


  /// Return an individual tile for a given angle and resolution
  /** Return a RawTile object: Overloaded by child class.
      \param h horizontal angle
      \param v vertical angle
      \param r resolution
      \param l quality layers
      \param t tile number
   */
  virtual RawTile getTile( int h, int v, unsigned int r, int l, unsigned int t ) { return RawTile(); };


  /// Return a region for a given angle and resolution
  /** Return a RawTile object: Overloaded by child class.
      \param ha horizontal angle
      \param va vertical angle
      \param r resolution
      \param layers number of layers to decode
      \param x offset in x direction
      \param y offset in y direction
      \param w width of region
      \param h height of region
      \param b image buffer
  */
    virtual void getRegion( int ha, int va, unsigned int r, int layers, int x, int y, int w, int h, unsigned char* b ){ return; };

  /// Assignment operator
  const IIPImage& operator = ( const IIPImage& );

  /// Comparison equality operator
  friend int operator == ( const IIPImage&, const IIPImage& );

  /// Comparison non-equality operator
  friend int operator != ( const IIPImage&, const IIPImage& );


};



#endif
