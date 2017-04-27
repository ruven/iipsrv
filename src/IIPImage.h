// IIPImage class

/*  IIP fcgi server module

    Copyright (C) 2000-2016 Ruven Pillay.

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


#ifndef _IIPIMAGE_H
#define _IIPIMAGE_H


// Fix missing snprintf in Windows
#if _MSC_VER
#define snprintf _snprintf
#endif


#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdexcept>

#include "RawTile.h"


/// Define our own derived exception class for file errors
class file_error : public std::runtime_error {
 public:
  file_error(std::string s) : std::runtime_error(s) { }
};


// Supported image formats
enum ImageFormat { TIF, JPEG2000, UNSUPPORTED };



/// Main class to handle the pyramidal image source
/** Provides functions to open, get various information from an image source
    and get individual tiles. This class is the base class for specific image
    file formats such as Tiled Pyramidal TIFF images via TPTImage.h and
    JPEG2000 via Kakadu.h
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

  /// Image file name suffix
  std::string suffix;

  /// Private function to determine the image type
  void testImageType() throw( file_error );

  /// If we have a sequence of images, determine which horizontal angles exist
  void measureHorizontalAngles();

  /// If we have a sequence of images, determine which vertical angles exist
  void measureVerticalAngles();

  /// The list of available horizontal angles (for image sequences)
  std::list <int> horizontalAnglesList;

  /// The list of available vertical angles (for image sequences)
  std::list <int> verticalAnglesList;


 protected:

  /// LUT
  std::vector <int> lut;

  /// Number of resolution levels that don't physically exist in file
  unsigned int virtual_levels;

  /// Return the image format e.g. tif
  ImageFormat format;


 public:
  /// The image pixel dimensions
  std::vector <unsigned int> image_widths, image_heights;

  /// The base tile pixel dimensions
  unsigned int tile_width, tile_height;

  /// The colour space of the image
  ColourSpaces colourspace;

  /// The number of available resolutions in this image
  unsigned int numResolutions;

  /// The bits per channel for this image
  unsigned int bpc;

  /// The number of channels for this image
  unsigned int channels;

  /// The sample format type (fixed or floating point)
  SampleType sampleType;

  /// The min and max sample value for each channel
  std::vector <float> min, max;

  /// Quality layers
  unsigned int quality_layers;

  /// Indicate whether we have opened and initialised some parameters for this image
  bool isSet;

  /// If we have an image sequence, the current X and Y position
  int currentX, currentY;

  /// STL map to hold string metadata
  std::map <const std::string, std::string> metadata;

  /// Image modification timestamp
  time_t timestamp;

  /// ICC Color Profile Data that is copied from the source image once when it is first opened
  unsigned long icc_profile_len;
  unsigned char *icc_profile_buf;

 public:

  /// Default Constructor
  IIPImage()
   : isFile( false ),
    virtual_levels( 0 ),
    format( UNSUPPORTED ),
    tile_width( 0 ),
    tile_height( 0 ),
    colourspace( NONE ),
    numResolutions( 0 ),
    bpc( 0 ),
    channels( 0 ),
    sampleType( FIXEDPOINT ),
    quality_layers( 0 ),
    isSet( false ),
    currentX( 0 ),
    currentY( 90 ),
    timestamp( 0 ),
    icc_profile_len( 0 ),
    icc_profile_buf( NULL ) { };


  /// Constructer taking the image path as parameter
  /** @param s image path
   */
  IIPImage( const std::string& s )
   : imagePath( s ),
    isFile( false ),
    virtual_levels( 0 ),
    format( UNSUPPORTED ),
    tile_width( 0 ),
    tile_height( 0 ),
    colourspace( NONE ),
    numResolutions( 0 ),
    bpc( 0 ),
    channels( 0 ),
    sampleType( FIXEDPOINT ),
    quality_layers( 0 ),
    isSet( false ),
    currentX( 0 ),
    currentY( 90 ),
    timestamp( 0 ),
    icc_profile_len( 0 ),
    icc_profile_buf( NULL ) { };


  /// Copy Constructor taking reference to another IIPImage object
  /** @param im IIPImage object
   */
  IIPImage( const IIPImage& image )
   : imagePath( image.imagePath ),
    fileSystemPrefix( image.fileSystemPrefix ),
    fileNamePattern( image.fileNamePattern ),
    isFile( image.isFile ),
    suffix( image.suffix ),
    horizontalAnglesList( image.horizontalAnglesList ),
    verticalAnglesList( image.verticalAnglesList ),
    lut( image.lut ),
    virtual_levels( image.virtual_levels ),
    format( image.format ),
    image_widths( image.image_widths ),
    image_heights( image.image_heights ),
    tile_width( image.tile_width ),
    tile_height( image.tile_height ),
    colourspace( image.colourspace ),
    numResolutions( image.numResolutions ),
    bpc( image.bpc ),
    channels( image.channels ),
    sampleType( image.sampleType ),
    min( image.min ),
    max( image.max ),
    quality_layers( image.quality_layers ),
    isSet( image.isSet ),
    currentX( image.currentX ),
    currentY( image.currentY ),
    metadata( image.metadata ),
    timestamp( image.timestamp ) {
      // create a copy of the ICC profile for the new image
      icc_profile_buf = NULL;
      icc_profile_len = 0;
      if ( image.icc_profile_buf != NULL ) {
        icc_profile_buf = new unsigned char[image.icc_profile_len];
        memcpy(&icc_profile_buf[0], image.icc_profile_buf, image.icc_profile_len);
        icc_profile_len = image.icc_profile_len;
      }
    };

  /// Virtual Destructor
  virtual ~IIPImage() 
  { 
    if ( icc_profile_buf != NULL ) {
      free( icc_profile_buf );
    }
  };

  /// Test the image and initialise some parameters
  void Initialise();

  /// Swap function
  /** @param a Object to copy to
      @param b Object to copy from
   */
  void swap( IIPImage& a, IIPImage& b );

  /// Return a list of available vertical angles
  std::list <int> getVerticalViewsList(){ return verticalAnglesList; };

  /// Return a list of horizontal angles
  std::list <int> getHorizontalViewsList(){ return horizontalAnglesList; };

  /// Return the image path
  const std::string& getImagePath() { return imagePath; };

  /// Return the full file path for a particular horizontal and vertical angle
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
   */
  const std::string getFileName( int x, int y );

  /// Get the image format
  //  const std::string& getImageFormat() { return format; };
  ImageFormat getImageFormat() { return format; };

  /// Get the image timestamp
  /** @param s file path
   */
  void updateTimestamp( const std::string& s ) throw( file_error );

  /// Get a HTTP RFC 1123 formatted timestamp
  const std::string getTimestamp();

  /// Check whether this object has been initialised
  bool set() { return isSet; };

  /// Set a file system prefix for added security
  void setFileSystemPrefix( const std::string& prefix ) { fileSystemPrefix = prefix; };

  /// Set the file name pattern used in image sequences
  void setFileNamePattern( const std::string& pattern ) { fileNamePattern = pattern; };

  /// Return the number of available resolutions in the image
  unsigned int getNumResolutions() { return numResolutions; };

  /// Return the number of bits per pixel for this image
  unsigned int getNumBitsPerPixel() { return bpc; };

  /// Return the number of channels for this image
  unsigned int getNumChannels() { return channels; };

  /// Return the minimum sample value for each channel
  /** @param n channel index
   */
  float getMinValue( int n=0 ) { return min[n]; };

  /// Return the minimum sample value for each channel
  /** @param n channel index
   */
  float getMaxValue( int n=0 ) { return max[n]; };

  /// Return the sample format type
  SampleType getSampleType(){ return sampleType; };

  /// Return the image width in pixels for a given resolution
  /** @param n resolution number (0 is default and full size image)
   */
  unsigned int getImageWidth( int n=0 ) { return image_widths[n]; };

  /// Return the image height in pixels for a given resolution
  /** @param n resolution number (0 is default and full size image)
   */
  unsigned int getImageHeight( int n=0 ) { return image_heights[n]; };

  /// Return the base tile height in pixels for a given resolution
  unsigned int getTileHeight() { return tile_height; };

  /// Return the base tile width in pixels
  unsigned int getTileWidth() { return tile_width; };

  /// Return the colour space for this image
  ColourSpaces getColourSpace() { return colourspace; };

  /// Return image metadata
  /** @param index metadata field name */
  const std::string& getMetadata( const std::string& index ) {
    return metadata[index];
  };

  /// Return whether this image type directly handles region decoding
  virtual bool regionDecoding(){ return false; };

  /// Load the appropriate codec module for this image type
  /** Used only for dynamically loading codec modules. Overloaded by DSOImage class.
      @param module the codec module path
   */
  virtual void Load( const std::string& module ) {;};

  /// Return codec description: Overloaded by child class.
  virtual const std::string getDescription() { return std::string( "IIPImage Base Class" ); };

  /// Open the image: Overloaded by child class.
  virtual void openImage() { throw file_error( "IIPImage openImage called" ); };

  /// Load information about the image eg. number of channels, tile size etc.
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
   */
  virtual void loadImageInfo( int x, int y ) { ; };

  /// Close the image: Overloaded by child class.
  virtual void closeImage() {;};


  /// Return an individual tile for a given angle and resolution
  /** Return a RawTile object: Overloaded by child class.
      @param h horizontal angle
      @param v vertical angle
      @param r resolution
      @param l quality layers
      @param t tile number
   */
  virtual RawTile getTile( int h, int v, unsigned int r, int l, unsigned int t ) { return RawTile(); };


  /// Return a region for a given angle and resolution
  /** Return a RawTile object: Overloaded by child class.
      @param ha horizontal angle
      @param va vertical angle
      @param r resolution
      @param layers number of layers to decode
      @param x offset in x direction
      @param y offset in y direction
      @param w width of region
      @param h height of region
      @param b image buffer
  */
  virtual RawTile getRegion( int ha, int va, unsigned int r, int layers, int x, int y, unsigned int w, unsigned int h ){ return RawTile(); };

  /// Assignment operator
  /** @param im IIPImage object */
  IIPImage& operator = ( IIPImage image ){
    swap( *this, image );
    return *this;
  };

  /// Comparison equality operator
  friend int operator == ( const IIPImage&, const IIPImage& );

  /// Comparison non-equality operator
  friend int operator != ( const IIPImage&, const IIPImage& );

  /// Return ICC Color Profile data
  /** Return void
      @param icc_len address of variable to hold length in bytes
      @param icc_buf address of variable to hold start to profile data
  */
  virtual void getICCProfile(unsigned long *icc_len, unsigned char **icc_buf)
  {
    *icc_len = icc_profile_len;
    *icc_buf = icc_profile_buf;
  };


};


#endif
