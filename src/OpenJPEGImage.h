#ifndef _OPENJPEGIMAGE_H
#define _OPENJPEGIMAGE_H

#include "IIPImage.h"
#include <cstdio>

#include <stdio.h> // openjpeg.h needs FILE* 
#include <openjpeg.h>
#include <iostream>
#include <fstream>

#define TILESIZE 256

extern std::ofstream logfile;

// Image class for JPEG 2000 Images: Inherits from IIPImage. Uses the OpenJPEG library.
class OpenJPEGImage : public IIPImage {

private:
 
 	FILE* fsrc; // Compressed source file
	
	unsigned int raster_width; // Image size
	unsigned int raster_height;
	
	unsigned int image_tile_width; // Tile size defined in the image
	unsigned int image_tile_height;
	
	int sgnd; // Whether the data are signed
	
	unsigned int max_layers; // Quality layers
	
	unsigned int virtual_levels; // How many virtual levels we need to generate

	/**
		 Main processing function
      		\param tw		width of region
      		\param th		height of region
      		\param xoffset		x coordinate
      		\param yoffset		y coordinate
		\param res		resolution
      		\param layers		number of quality levels to decode
      		\param tile		specific tile to decode (-1 if deconding a region)
      		\param d		buffer to fill
	*/
	void process(unsigned int tw, unsigned int th, unsigned int xoffset, unsigned int yoffset, unsigned int res, int layers, int tile, void* d) throw (std::string);

public:

	/** 
		Constructor
	*/
	OpenJPEGImage() : IIPImage(){ 
		image_tile_width = 0; image_tile_height = 0;
		tile_width = TILESIZE; tile_height = TILESIZE;
    		raster_width = 0; raster_height = 0;
    		sgnd = 0; numResolutions = 0; virtual_levels = 0;
	};

	/** 
		Constructor
		\param path		image path
	*/
	OpenJPEGImage(const std::string& path) : IIPImage(path){
		image_tile_width = 0; image_tile_height = 0;
		tile_width = TILESIZE; tile_height = TILESIZE;
    		raster_width = 0; raster_height = 0;
    		sgnd = 0; numResolutions = 0; virtual_levels = 0;
	};

	/** 
		Copy Constructor
		\param image		IIPImage object
	*/
	OpenJPEGImage(const IIPImage& image): IIPImage(image) {
		tile_width = TILESIZE; tile_height = TILESIZE;
		numResolutions = image.numResolutions;
		virtual_levels = 0;
	};

	/** 
		Destructor
	*/
	~OpenJPEGImage() { closeImage(); };

	/** 
		Overloaded function for opening a JP2 image
	*/
	void openImage() throw (std::string);

	/** 
		Overloaded function for loading JP2 image information
		\param x horizontal sequence angle
		\param y vertical sequence angle
	*/
	void loadImageInfo(int x, int y) throw (std::string);

	/** 
		Overloaded function for closing a JP2 image
	*/
	void closeImage();

	/**
		Overloaded function for getting a particular tile
		\param x	horizontal sequence angle
		\param y	vertical sequence angle
		\param r	resolution
		\param l	number of quality layers to decode
		\param t	tile number
	*/
    RawTile getTile(int x, int y, unsigned int r, int l, unsigned int t) throw (std::string);

	/** 
		Overloaded function for returning a region from image
 		\param ha	horizontal angle
		\param va	vertical angle
		\param r	resolution
		\param l	number of quality layers to decode
		\param x	x coordinate
		\param y	y coordinate
		\param w	width of region
		\param h	height of region
		\param b	buffer to fill
		\return		a RawTile object
	*/
	void getRegion(int ha, int va, unsigned int r, int l, int x, int y, unsigned int w, unsigned int h, unsigned char* b) throw (std::string);
};

#endif
