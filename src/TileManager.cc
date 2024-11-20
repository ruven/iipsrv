
// Member functions for TileManager.h


/*  IIP Server: Tile Cache Handler

    Copyright (C) 2005-2024 Ruven Pillay

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



#include <cmath>
#include "TileManager.h"


using namespace std;



RawTile TileManager::getNewTile( int resolution, int tile, int xangle, int yangle, int layers, ImageEncoding ctype ){

  // If user has overriden quality factor, decode to raw format to allow us to re-encode
  ImageEncoding source_encoding = (compressor->defaultQuality() == true) ? ctype : ImageEncoding::RAW;

  // Get a tile from the IIPImage image object
  if( loglevel >= 2 ) insert_timer.start();
  RawTile ttt = image->getTile( xangle, yangle, resolution, layers, tile, source_encoding );
  if( loglevel >= 2 ) *logfile << "TileManager :: Tile decoding time: " << insert_timer.getTime()
			       << " microseconds" << endl;


  // Apply the watermark if we have one.
  // Do this before inserting into cache so that we cache watermarked tiles
  if( watermark && watermark->isSet() ){

    if( loglevel >= 4 ) insert_timer.start();
    watermark->apply( ttt.data, ttt.width, ttt.height, ttt.channels, ttt.bpc );
    if( loglevel >= 4 ) *logfile << "TileManager :: Watermark applied: " << insert_timer.getTime()
				 << " microseconds" << endl;
  }


  // If our tile is already correctly encoded, no need to re-encode, but may need to inject metadata
  if( (ttt.compressionType == ctype) && (ctype != ImageEncoding::RAW) ){

    if( loglevel >= 3 ) *logfile << "TileManager :: Returning pre-encoded tile of size " << ttt.dataLength << " bytes" << endl;

    // Need to set quality to allow cache to sort correctly
    ttt.quality = compressor->getQuality();

    // Note that injection only implemented for WebP and JPEG
    if( loglevel >= 4 ) compression_timer.start();
    compressor->injectMetadata( ttt );
    if( loglevel >= 4 ) *logfile << "TileManager :: Metadata injection time: "
				 << compression_timer.getTime() << " microseconds" << endl;
  }
  // Encode our tile
  else{

    switch( ctype ){

      case ImageEncoding::RAW:
	// Nothing to do
	break;


      case ImageEncoding::JPEG:
	// Do our JPEG compression iff we have an 8 bit per channel image
	if( ttt.bpc == 8 && (ttt.channels==1 || ttt.channels==3) ){
	  if( loglevel >= 4 ) compression_timer.start();
	  compressor->Compress( ttt );
	  if( loglevel >= 4 ) *logfile << "TileManager :: JPEG compression time: "
				       << compression_timer.getTime() << " microseconds" << endl;
	}
	break;


      case ImageEncoding::TIFF:
	if( loglevel >= 4 ) compression_timer.start();
	compressor->Compress( ttt );
	if( loglevel >= 4 ) *logfile << "TileManager :: TIFF compression time: "
				     << compression_timer.getTime() << " microseconds" << endl;
	break;


      case ImageEncoding::PNG:
	if( loglevel >= 4 ) compression_timer.start();
	compressor->Compress( ttt );
	if( loglevel >= 4 ) *logfile << "TileManager :: PNG compression time: "
				     << compression_timer.getTime() << " microseconds" << endl;
	break;


      case ImageEncoding::WEBP:
	if( loglevel >= 4 ) compression_timer.start();
	compressor->Compress( ttt );
	if( loglevel >= 4 ) *logfile << "TileManager :: WebP compression time: "
				     << compression_timer.getTime() << " microseconds" << endl;
	break;


      case ImageEncoding::AVIF:
	if( loglevel >= 4 ) compression_timer.start();
	compressor->Compress( ttt );
	if( loglevel >= 4 ) *logfile << "TileManager :: AVIF compression time: "
				     << compression_timer.getTime() << " microseconds" << endl;
	break;


      default:
	break;

    }

  }


  // Add to our tile cache
  if( loglevel >= 4 ) insert_timer.start();
  tileCache->insert( ttt );
  if( loglevel >= 4 ) *logfile << "TileManager :: Tile cache insertion time: " << insert_timer.getTime()
			       << " microseconds" << endl;


  return ttt;

}



RawTile TileManager::getTile( int resolution, int tile, int xangle, int yangle, int layers, ImageEncoding ctype ){

  RawTile* rawtile = NULL;
  string tileCompression;
  string compName;


  // Time the tile retrieval
  if( loglevel >= 3 ) tile_timer.start();


  /* Try to get the encoded tile directly from our cache first.
     Otherwise decode one from the source image and add it to the cache
   */
  switch( ctype )
    {

    case ImageEncoding::JPEG:
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, ImageEncoding::JPEG, compressor->getQuality() )) ) break;
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, ImageEncoding::RAW, 0 )) ) break;
      break;


    case ImageEncoding::TIFF:
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, ImageEncoding::TIFF, compressor->getQuality() )) ) break;
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, ImageEncoding::RAW, 0 )) ) break;
      break;


    case ImageEncoding::PNG:
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, ImageEncoding::PNG, compressor->getQuality() )) ) break;
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, ImageEncoding::RAW, 0 )) ) break;
      break;


    case ImageEncoding::WEBP:
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, ImageEncoding::WEBP, compressor->getQuality() )) ) break;
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, ImageEncoding::RAW, 0 )) ) break;
      break;


    case ImageEncoding::AVIF:
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, ImageEncoding::AVIF, compressor->getQuality() )) ) break;
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, ImageEncoding::RAW, 0 )) ) break;
      break;


    case ImageEncoding::RAW:
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, ImageEncoding::RAW, 0 )) ) break;
      break;


    default: 
      break;

    }



  if( loglevel >= 3 ){
    // Define our compression names for logging purposes
    switch( ctype ){
      case ImageEncoding::JPEG: compName = "JPEG"; break;
      case ImageEncoding::PNG: compName = "PNG"; break;
      case ImageEncoding::WEBP: compName = "WebP"; break;
      case ImageEncoding::AVIF: compName = "AVIF"; break;
      case ImageEncoding::DEFLATE: compName = "DEFLATE"; break;
      case ImageEncoding::RAW: compName = "RAW"; break;
      default: break;
    }
  }


  // If we haven't been able to get a tile, get a raw one
  if( !rawtile || (rawtile && (rawtile->timestamp != image->timestamp)) ){

    if( rawtile && (rawtile->timestamp != image->timestamp) ){
      if( loglevel >= 3 ) *logfile << "TileManager :: Tile has different timestamp "
			           << rawtile->timestamp << " - " << image->timestamp
                                   << " ... updating" << endl;
    }

    if( loglevel >= 4 ) *logfile << "TileManager :: Cache miss for resolution: " << resolution
				 << ", tile: " << tile
				 << ", compression: " << compName
				 << ", quality: " << compressor->getQuality() << endl
				 << "TileManager :: Cache size: " << tileCache->getNumElements()
				 << " tiles, " << tileCache->getMemorySize() << " MB" << endl;


    RawTile newtile = this->getNewTile( resolution, tile, xangle, yangle, layers, ctype );

    if( loglevel >= 3 ) *logfile << "TileManager :: Total tile access time: "
				 << tile_timer.getTime() << " microseconds" << endl;
    return newtile;
  }




  if( loglevel >= 3 ) *logfile << "TileManager :: Cache hit for resolution: " << resolution
			       << ", tile: " << tile
			       << ", compression: " << compName
			       << ", quality: " << compressor->getQuality() << endl
			       << "TileManager :: Cache size: "
			       << tileCache->getNumElements() << " tiles, "
			       << tileCache->getMemorySize() << " MB" << endl;


  // Check whether the compression used for out tile matches our requested compression type. If not, we must convert
  // Perform JPEG compression iff we have an 8 bit per channel image and either 1 or 3 bands
  // PNG compression can have 8 or 16 bits and alpha channels
  if( (rawtile->compressionType == ImageEncoding::RAW) &&
      ( ( ctype==ImageEncoding::JPEG && rawtile->bpc==8 && (rawtile->channels==1 || rawtile->channels==3) ) ||
	ctype==ImageEncoding::PNG || ctype==ImageEncoding::WEBP || ctype==ImageEncoding::AVIF ) ){

    // Rawtile is a pointer to the cache data, so we need to create a copy of it in case we compress it
    RawTile ttt( *rawtile );

    if( loglevel >=2 ) compression_timer.start();
    unsigned int oldlen = rawtile->dataLength;
    unsigned int newlen = compressor->Compress( ttt );
    if( loglevel >= 3 ) *logfile << "TileManager :: " << compName << " requested, but RAW data found in cache." << endl
				 << "TileManager :: " << compName << " Compression Time: "
				 << compression_timer.getTime() << " microseconds" << endl
				 << "TileManager :: Compression Ratio: " << newlen << "/" << oldlen << " = "
				 << ( (float)newlen/(float)oldlen ) << endl;

    // Add our compressed tile to the cache
    if( loglevel >= 3 ) insert_timer.start();
    tileCache->insert( ttt );
    if( loglevel >= 3 ) *logfile << "TileManager :: Tile cache insertion time: " << insert_timer.getTime()
				 << " microseconds" << endl;

    if( loglevel >= 3 ) *logfile << "TileManager :: Total tile access time: "
				 << tile_timer.getTime() << " microseconds" << endl;
    return RawTile( ttt );
  }

  if( loglevel >= 3 ) *logfile << "TileManager :: Total tile access time: "
			       << tile_timer.getTime() << " microseconds" << endl;

  return RawTile( *rawtile );


}


RawTile TileManager::getRegion( unsigned int res, int seq, int ang, int layers, unsigned int x, unsigned int y, unsigned int width, unsigned int height ){

  // If our image type can directly handle region compositing, simply return that
  if( image->regionDecoding() ){
    if( loglevel >= 3 ){
      *logfile << "TileManager getRegion :: requesting region directly from image" << endl;
    }
    return image->getRegion( seq, ang, res, layers, x, y, width, height );
  }

  // Otherwise do the compositing ourselves
  int vipsres = image->getNativeResolution( res );

  // The tile size of the source tile
  unsigned int src_tile_width = image->tile_widths[vipsres];
  unsigned int src_tile_height = image->tile_heights[vipsres];

  // The tile size of the destination tile
  unsigned int dst_tile_width = src_tile_width;
  unsigned int dst_tile_height = src_tile_height;

  // The basic tile size ie. not the current tile
  unsigned int basic_tile_width = src_tile_width;
  unsigned int basic_tile_height = src_tile_height;

  unsigned int im_width = image->image_widths[vipsres];
  unsigned int im_height = image->image_heights[vipsres];

  unsigned int rem_x = im_width % src_tile_width;
  unsigned int rem_y = im_height % src_tile_height;

  // The number of tiles in each direction
  unsigned int ntlx = (im_width / src_tile_width) + (rem_x == 0 ? 0 : 1);
  unsigned int ntly = (im_height / src_tile_height) + (rem_y == 0 ? 0 : 1);

  // Start and end tiles and pixel offsets
  unsigned int startx, endx, starty, endy, xoffset, yoffset;


  // Request for a region within image
  if( ! ( x==0 && y==0 && width==im_width && height==im_height ) ){

    // Calculate the start tiles and any offset
    startx = (unsigned int) ( x / src_tile_width );
    starty = (unsigned int) ( y / src_tile_height );
    xoffset = x % src_tile_width;
    yoffset = y % src_tile_height;

    endx = (unsigned int) ceil( (float)(width + x) / (float)src_tile_width );
    endy = (unsigned int) ceil( (float)(height + y) / (float)src_tile_height );

    if( loglevel >= 3 ){
      *logfile << "TileManager getRegion :: Total tiles in requested resolution: " << ntlx << "x" << ntly << " tiles" << endl
	       << "TileManager getRegion :: Tile start: " << startx << "," << starty << " with offset: "
	       << xoffset << "," << yoffset << endl
	       << "TileManager getRegion :: Tile end: " << endx-1 << "," << endy-1 << endl;
    }
  }
  else{
    startx = starty = xoffset = yoffset = 0;
    endx = ntlx;
    endy = ntly;
  }


  // Create an empty tile with the correct dimensions
  RawTile region( 0, res, seq, ang, width, height, 0, 0 );

  unsigned int current_height = 0;

  // Decode the image strip by strip
  for( unsigned int i=starty; i<endy; i++ ){

    unsigned long buffer_index = 0;

    // Keep track of the current pixel boundary horizontally. ie. only up
    //  to the beginning of the current tile boundary.
    unsigned int current_width = 0;

    for( unsigned int j=startx; j<endx; j++ ){

      // Time the tile retrieval
      if( loglevel >= 3 ) tile_timer.start();

      // Get a raw tile
      RawTile rawtile = this->getTile( res, (i*ntlx) + j, seq, ang, layers, ImageEncoding::RAW );

      if( loglevel >= 5 ){
	*logfile << "TileManager getRegion :: Tile access time " << tile_timer.getTime() << " microseconds for tile "
		 << (i*ntlx) + j << " at resolution " << res << endl;
      }

      // Need to initialize our output region with the actual data types we find in our raw data - these can potentially be different
      // between images which are in a sequence or image stack. To do that requires knowledge of the contents of the tiles,
      // so we do ii after retrieving our first tile and only once
      if( i==starty && j==startx ){

	region.channels = rawtile.channels;
	region.bpc = rawtile.bpc;
	region.sampleType = rawtile.sampleType;
	if( region.bpc == 1 ) region.bpc = 8;   // Assume 1 bit data has been unpacked to 8 bits per channel

	// Allocate appropriate storage for our output
	region.allocate();

	if( loglevel >= 5 ){
	  *logfile << "TileManager getRegion :: Tile data is " << rawtile.channels << " channels, "
		   << rawtile.bpc << " bits per channel" << endl;
	}
      }

      // Set the tile width and height to be that of the source tile - Use the rawtile data
      // because if we take a tile from cache the image pointer will not necessarily be pointing
      // to the the current tile
      src_tile_width = rawtile.width;
      src_tile_height = rawtile.height;
      dst_tile_width = src_tile_width;
      dst_tile_height = src_tile_height;

      // Variables for the pixel offset within the current tile
      unsigned int xf = 0;
      unsigned int yf = 0;

      // If our viewport has been set, we need to modify our start
      // and end points on the source image
      if( !( x==0 && y==0 && width==im_width && height==im_height ) ){

	unsigned int remainder;  // Remaining pixels in the final row or column

	if( j == startx ){
	  // Calculate the width used in the current tile
	  // If there is only 1 tile, the width is just the view width
	  if( j < endx - 1 ) dst_tile_width = src_tile_width - xoffset;
	  else dst_tile_width = width;
	  xf = xoffset;
	}
	else if( j == endx-1 ){
	  // If this is the final column, calculate the remaining number of pixels
	  remainder = (width+x) % basic_tile_width;
	  if( remainder != 0 ) dst_tile_width = remainder;
	}

	if( i == starty ){
	  // Calculate the height used in the current row of tiles
	  // If there is only 1 row the height is just the view height
	  if( i < endy - 1 ) dst_tile_height = src_tile_height - yoffset;
	  else dst_tile_height = height;
	  yf = yoffset;
	}
	else if( i == endy-1 ){
	  // If this is the final row, calculate the remaining number of pixels
	  remainder = (height+y) % basic_tile_height;
	  if( remainder != 0 ) dst_tile_height = remainder;
	}

	if( loglevel >= 5 ){
	  *logfile << "TileManager getRegion :: destination tile width: " << dst_tile_width
		   << ", tile height: " << dst_tile_height << endl;
	}
      }

      // Copy our tile data into the appropriate part of the strip memory
      // one whole tile width at a time
      for( unsigned int k=0; k<dst_tile_height; k++ ){

	buffer_index = (current_width*region.channels) + (k*width*region.channels) + (current_height*width*region.channels);
	unsigned int inx = ((k+yf)*rawtile.width*rawtile.channels) + (xf*rawtile.channels);

	// Simply copy the line of data across
	if( region.bpc == 8 ){
	  unsigned char* ptr = (unsigned char*) rawtile.data;
	  unsigned char* buf = (unsigned char*) region.data;
	  memcpy( &buf[buffer_index], &ptr[inx], (size_t)dst_tile_width*region.channels );
	}
	else if( region.bpc ==  16 ){
	  unsigned short* ptr = (unsigned short*) rawtile.data;
	  unsigned short* buf = (unsigned short*) region.data;
	  memcpy( &buf[buffer_index], &ptr[inx], (size_t)dst_tile_width*region.channels*2 );
	}
	else if( region.bpc == 32 && region.sampleType == SampleType::FIXEDPOINT ){
	  unsigned int* ptr = (unsigned int*) rawtile.data;
	  unsigned int* buf = (unsigned int*) region.data;
	  memcpy( &buf[buffer_index], &ptr[inx], (size_t)dst_tile_width*region.channels*4 );
	}
	else if( region.bpc == 32 && region.sampleType == SampleType::FLOATINGPOINT ){
	  float* ptr = (float*) rawtile.data;
	  float* buf = (float*) region.data;
	  memcpy( &buf[buffer_index], &ptr[inx], (size_t)dst_tile_width*region.channels*4 );
	}
      }

      current_width += dst_tile_width;
    }

    current_height += dst_tile_height;

  }

  return region;

}
