/*  IIP Server: OpenJPEG JPEG2000 handler

    Copyright (C) 2015 Moravian Library in Brno (http://www.mzk.cz/)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _OPENJPEGIMAGE_H
#define _OPENJPEGIMAGE_H

#include "IIPImage.h"

#define TILESIZE 256

// Image class for JPEG 2000 Images:
// Inherits from IIPImage. Uses the OpenJPEG library.
class OpenJPEGImage : public IIPImage {

private:
  unsigned int raster_width; // Image size
  unsigned int raster_height;

  unsigned int image_tile_width; // Tile size defined in the image
  unsigned int image_tile_height;

  unsigned int max_layers; // Quality layers

  unsigned int virtual_levels; // How many virtual levels we need to generate

  bool sgnd; // Whether the data are signed

  /**
     Main processing function
    \param res              resolution
    \param layers           number of quality levels to decode
    \param xoffset          x coordinate
    \param yoffset          y coordinate
    \param tw               width of region
    \param th               height of region
    \param tile             specific tile to decode (-1 if deconding a region)
    \param d                buffer to fill
*/
  void process(unsigned int res, int layers,
               unsigned int tw, unsigned int th,
               unsigned int xoffset, unsigned int yoffset,
               int tile, void* d) throw(file_error);

public:
  /**
    Constructor
  */
  OpenJPEGImage()
      : IIPImage()
  {
    image_tile_width = 0;
    image_tile_height = 0;
    tile_width = TILESIZE;
    tile_height = TILESIZE;
    raster_width = 0;
    raster_height = 0;
    sgnd = false;
    numResolutions = 0;
    virtual_levels = 0;
  };

  /**
    Constructor
    \param path             image path
  */
  OpenJPEGImage(const std::string& path)
      : IIPImage(path)
  {
    image_tile_width = 0;
    image_tile_height = 0;
    tile_width = TILESIZE;
    tile_height = TILESIZE;
    raster_width = 0;
    raster_height = 0;
    sgnd = false;
    numResolutions = 0;
    virtual_levels = 0;
  };

  /**
    Copy Constructor
    \param image            IIPImage object
  */
  OpenJPEGImage(const IIPImage& image)
      : IIPImage(image)
  {
    image_tile_width = 0;
    image_tile_height = 0;
    tile_width = TILESIZE;
    tile_height = TILESIZE;
    raster_width = 0;
    raster_height = 0;
    sgnd = false;
    numResolutions = image.numResolutions;
    virtual_levels = 0;
  };

  /**
    Destructor
  */
  ~OpenJPEGImage()
  {
    closeImage();
  };

  /**
    Overloaded function for opening a JP2 image
  */
  void openImage() throw(file_error);

  /**
    Overloaded function for loading JP2 image information
    \param x horizontal sequence angle
    \param y vertical sequence angle
  */
  void loadImageInfo(int x, int y) throw(file_error);

  /**
    Overloaded function for closing a JP2 image
  */
  void closeImage();

  /// Return whether this image type directly handles region decoding.
  bool regionDecoding()
  {
    return true;
  };

  /// Overloaded function for getting a particular tile
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
      @param r resolution
      @param l number of quality layers to decode
      @param t tile number
   */
  RawTile getTile(int x, int y, unsigned int r, int l,
                  unsigned int t) throw(file_error);

  /**
    Overloaded function for returning a region from image
    \param ha       horizontal angle
    \param va       vertical angle
    \param res      resolution
    \param layers   number of quality layers to decode
    \param x        x coordinate
    \param y        y coordinate
    \param w        width of region
    \param h        height of region
    \return         a RawTile object
  */
  RawTile getRegion(int ha, int va, unsigned int res, int layers,
                    int x, int y, unsigned int w, unsigned int h) throw(file_error);
};


#endif
