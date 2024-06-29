// IIPImage class

/*  IIP fcgi server module

    Copyright (C) 2000-2024 Ruven Pillay.

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


// Fix missing snprintf and vsnprintf in Windows
#if defined _MSC_VER && _MSC_VER<1900
#define snprintf _snprintf
#define vsnprintf _vsnprintf
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
  /** @param s error message */
  file_error(const std::string& s) : std::runtime_error(s) { }
};



// Structure for storing basic information on image stacks
// - for now just stores stack name and scaling factor
struct Stack {
  std::string name;
  float scale;
  Stack() : scale(1) {};
};



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

  /// Suffix to add to paths
  std::string fileSystemSuffix;

  /// Pattern for sequences
  std::string fileNamePattern;

  /// Indicates whether our image is a single file or part or a sequence
  bool isFile;

  /// Image file name suffix
  std::string suffix;

  /// Private function to determine the image type
  void testImageType();

  /// If we have a sequence of images, determine which horizontal angles exist
  void measureHorizontalAngles();

  /// If we have a sequence of images, determine which vertical angles exist
  void measureVerticalAngles();


 protected:

  /// Multi-resolution pyramid type - used currently only to distinguish TIFF types
  enum PyramidType { NORMAL, SUBIFD };

  /// The list of available horizontal angles (for image sequences)
  std::list <int> horizontalAnglesList;

  /// The list of available vertical angles (for image sequences)
  std::list <int> verticalAnglesList;

  /// LUT
  std::vector <int> lut;

  /// Number of resolution levels that don't physically exist in file
  unsigned int virtual_levels;

  /// Return the image format e.g. tif
  ImageEncoding format;

  /// Define how pyramid is structured
  PyramidType pyramid;

  /// Whether we have an image stack consisting of multiple images within a single file
  std::list <Stack> stack;

  /// List of IFD offsets for each resolution
  std::vector <uint32_t> resolution_ids;


 public:

  /// The image pixel dimensions
  std::vector <unsigned int> image_widths, image_heights;

  /// The tile dimensions for each resolution
  std::vector <unsigned int> tile_widths, tile_heights;

  /// The color space of the image
  ColorSpace colorspace;

  /// Native physical resolution in both X and Y
  float dpi_x, dpi_y;

  /// Units for native physical resolution
  /** 0=unknown, 1=pixels/inch, 2=pixels/cm */
  int dpi_units;

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

  /// Image histogram
  std::vector<unsigned int> histogram;

  /// STL map to hold string metadata
  std::map <const std::string, const std::string> metadata;

  /// Image modification timestamp
  time_t timestamp;

  /// Our logging stream - declared statically
  static bool logging;

  /// Whether codec pass-through mode is enabled
  static bool codec_passthrough;


 public:

  /// Default Constructor
  IIPImage() :
    isFile( false ),
    virtual_levels( 0 ),
    format( ImageEncoding::UNSUPPORTED ),
    pyramid( NORMAL ),
    colorspace( ColorSpace::NONE ),
    dpi_x( 0 ),
    dpi_y( 0 ),
    dpi_units( 0 ),
    numResolutions( 0 ),
    bpc( 0 ),
    channels( 0 ),
    sampleType( SampleType::FIXEDPOINT ),
    quality_layers( 0 ),
    isSet( false ),
    currentX( 0 ),
    currentY( 90 ),
    timestamp( 0 ) {};

  /// Constructer taking the image path as parameter
  /** @param s image path
   */
  IIPImage( const std::string& s ) :
    imagePath( s ),
    isFile( false ),
    virtual_levels( 0 ),
    format( ImageEncoding::UNSUPPORTED ),
    pyramid( NORMAL ),
    colorspace( ColorSpace::NONE ),
    dpi_x( 0 ),
    dpi_y( 0 ),
    dpi_units( 0 ),
    numResolutions( 0 ),
    bpc( 0 ),
    channels( 0 ),
    sampleType( SampleType::FIXEDPOINT ),
    quality_layers( 0 ),
    isSet( false ),
    currentX( 0 ),
    currentY( 90 ),
    timestamp( 0 ) {};

  /// Copy Constructor taking reference to another IIPImage object
  /** @param image IIPImage object
   */
  IIPImage( const IIPImage& image ) :
    imagePath( image.imagePath ),
    fileSystemPrefix( image.fileSystemPrefix ),
    fileSystemSuffix( image.fileSystemSuffix ),
    fileNamePattern( image.fileNamePattern ),
    isFile( image.isFile ),
    suffix( image.suffix ),
    horizontalAnglesList( image.horizontalAnglesList ),
    verticalAnglesList( image.verticalAnglesList ),
    lut( image.lut ),
    virtual_levels( image.virtual_levels ),
    format( image.format ),
    pyramid( image.pyramid ),
    stack( image.stack ),
    resolution_ids( image.resolution_ids ),
    image_widths( image.image_widths ),
    image_heights( image.image_heights ),
    tile_widths( image.tile_widths ),
    tile_heights( image.tile_heights ),
    colorspace( image.colorspace ),
    dpi_x( image.dpi_x ),
    dpi_y( image.dpi_y ),
    dpi_units( image.dpi_units ),
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
    histogram( image.histogram ),
    metadata( image.metadata ),
    timestamp( image.timestamp ) {};

  /// Virtual Destructor
  virtual ~IIPImage() {};

  /// Test the image and initialise some parameters
  void Initialise();

  /// Swap function
  /** @param a Object to copy to
      @param b Object to copy from
   */
  void swap( IIPImage& a, IIPImage& b );

  /// Return a list of available vertical angles
  std::list <int> getVerticalViewsList() const { return verticalAnglesList; };

  /// Return a list of horizontal angles
  std::list <int> getHorizontalViewsList() const { return horizontalAnglesList; };

  /// Return the image path
  const std::string& getImagePath() const { return imagePath; };

  /// Return the full file path for a particular horizontal and vertical angle
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
   */
  const std::string getFileName( int x, int y );

  /// Get the image format
  ImageEncoding getImageFormat() const { return format; };

  /// Get the image timestamp
  /** @param s file path
   */
  void updateTimestamp( const std::string& s );

  /// Get a HTTP RFC 1123 formatted timestamp
  const std::string getTimestamp();

  /// Check whether this object has been initialised
  bool set() const { return isSet; };

  /// Set a file system prefix for added security
  void setFileSystemPrefix( const std::string& prefix ) { fileSystemPrefix = prefix; };

  /// Set a file system suffix
  void setFileSystemSuffix( const std::string& s ) { fileSystemSuffix = s; };

  /// Set the file name pattern used in image sequences
  void setFileNamePattern( const std::string& pattern ) { fileNamePattern = pattern; };

  /// Return the number of available resolutions in the image
  unsigned int getNumResolutions() const { return numResolutions; };

  /// Return index of the resolution within the image file
  /** @param res IIP protocol resolution level where 0 is smallest image
   */
  int getNativeResolution( const int res ) const { return numResolutions - res - 1; };

  /// Return the number of bits per pixel for this image
  unsigned int getNumBitsPerPixel() const { return bpc; };

  /// Return the number of channels for this image
  unsigned int getNumChannels() const { return channels; };

  /// Return the minimum sample value for each channel
  /** @param n channel index
   */
  float getMinValue( int n=0 ) const { return min[n]; };

  /// Return the minimum sample value for each channel
  /** @param n channel index
   */
  float getMaxValue( int n=0 ) const { return max[n]; };

  /// Return the sample format type
  SampleType getSampleType() const { return sampleType; };

  /// Return the image width in pixels for a given resolution
  /** @param n resolution number (0 is default and full size image)
   */
  unsigned int getImageWidth( int n=0 ) const { return image_widths[n]; };

  /// Return the image height in pixels for a given resolution
  /** @param n resolution number (0 is default and full size image)
   */
  unsigned int getImageHeight( int n=0 ) const { return image_heights[n]; };

  /// Return the tile width in pixels for a given resolution
  /** @param n IIP resolution (tile size for full resolution image by default)
   */
  unsigned int getTileWidth( int n=-1 ) const {
    if( n == -1 ) n = 0;
    else n = getNativeResolution( n );
    if( tile_widths.size() < (size_t) n+1 ) n = 0;
    return tile_widths[n];
  };

  /// Return the tile height in pixels for a given resolution
  /** @param n IIP resolution (tile size for full reslolution image by default)
   */
  unsigned int getTileHeight( int n=-1 ) const {
    if( n == -1 ) n = 0;
    else n = getNativeResolution( n );
    if( tile_heights.size() < (size_t) n+1 ) n = 0;
    return tile_heights[n];
  };

  /// Return the color space for this image
  ColorSpace getColorSpace() const { return colorspace; };

  /// Return whether image is a single-file image stack
  bool isStack() const {
    if( stack.size() > 0 ) return true;
    return false;
  };

  /// Load stack info
  std::list <Stack> getStack() const { return stack; };

  /// Return image metadata
  /** @param index metadata field name */
  const std::string& getMetadata( const std::string& index ){
    return metadata[index];
  };

  /// Return physical resolution (DPI) in pixels/meter horizontally
  float getHorizontalDPI() const { return (dpi_units==2) ? dpi_x*100.0 : ( (dpi_units==1) ? dpi_x/0.0254 : dpi_x ); };

  /// Return physical resolution (DPI) in pixels/meter vertically
  float getVerticalDPI() const { return (dpi_units==2) ? dpi_y*100.0 : ( (dpi_units==1) ? dpi_y/0.0254 : dpi_y ); };

  /// Return whether this image type directly handles region decoding
  virtual bool regionDecoding(){ return false; };

  /// Load the appropriate codec module for this image type
  /** Used only for dynamically loading codec modules. Overloaded by DSOImage class.
      @param module the codec module path
   */
  virtual void Load( const std::string& module ) {};

  /// Return codec description: Overloaded by child class.
  virtual std::string getDescription() const { return std::string( "IIPImage Base Class" ); };

  /// Open the image: Overloaded by child class.
  virtual void openImage() { throw file_error( "IIPImage openImage called" ); };

  /// Load information about the image eg. number of channels, tile size etc.
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
   */
  virtual void loadImageInfo( int x, int y ) {};

  /// Close the image: Overloaded by child class.
  virtual void closeImage() {};


  /// Return an individual tile for a given angle and resolution
  /** Return a RawTile object: Overloaded by child class.
      @param h horizontal angle
      @param v vertical angle
      @param r resolution
      @param l quality layers
      @param t tile number
      @param e image encoding
   */
  virtual RawTile getTile( int h, int v, unsigned int r, int l, unsigned int t, ImageEncoding e = ImageEncoding::RAW ) { return RawTile(); };


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
      @return RawTile image
  */
  virtual RawTile getRegion( int ha, int va, unsigned int r, int layers, int x, int y, unsigned int w, unsigned int h ){ return RawTile(); };


  /// Assignment operator
  /** @param image IIPImage object */
  IIPImage& operator = ( IIPImage image ){
    swap( *this, image );
    return *this;
  };


  /// Setup logging for codec library errors and warnings
  static void setupLogging(){;};


  /// Get codec version
  /** @return codec version */
  static const char* getCodecVersion(){ return "0.0.0"; };


  /// Comparison equality operator
  friend int operator == ( const IIPImage&, const IIPImage& );

  /// Comparison non-equality operator
  friend int operator != ( const IIPImage&, const IIPImage& );

};


#endif
