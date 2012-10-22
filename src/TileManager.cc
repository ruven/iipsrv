
// Member functions for TileManager.h


/*  IIP Server: Tile Cache Handler

    Copyright (C) 2005-2012 Ruven Pillay.

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



#include "TileManager.h"


using namespace std;



RawTile TileManager::getNewTile( int resolution, int tile, int xangle, int yangle, int layers, CompressionType c ){

  if( loglevel >= 2 ) *logfile << "TileManager :: Cache Miss for resolution: " << resolution << ", tile: " << tile << endl
			       << "TileManager :: Cache Size: " << tileCache->getNumElements()
			       << " tiles, " << tileCache->getMemorySize() << " MB" << endl;


  RawTile ttt;
  int len = 0;

  // Get our raw tile from the IIPImage image object
  ttt = image->getTile( xangle, yangle, resolution, layers, tile );


  // Apply the watermark if we have one.
  // Do this before inserting into cache so that we cache watermarked tiles
  if( watermark && watermark->isSet() ){

    if( loglevel >= 2 ) insert_timer.start();
    unsigned int tw = ttt.padded? image->getTileWidth() : ttt.width;
    unsigned int th = ttt.padded? image->getTileHeight() : ttt.height;

    watermark->apply( ttt.data, tw, th, ttt.channels, ttt.bpc );
    if( loglevel >= 2 ) *logfile << "TileManager :: Watermark applied: " << insert_timer.getTime()
				 << " microseconds" << endl;
  }


  // We need to crop our edge tiles if they are not the full tile size
  if( ((ttt.width != image->getTileWidth()) || (ttt.height != image->getTileHeight())) && ttt.padded ){
    if( loglevel >= 5 ) * logfile << "TileManager :: Cropping tile" << endl;
    this->crop( &ttt );
  }


  // Add our uncompressed tile directly into our cache
  if( c == UNCOMPRESSED ){
    // Add to our tile cache
    if( loglevel >= 2 ) insert_timer.start();
    tileCache->insert( ttt );
    if( loglevel >= 2 ) *logfile << "TileManager :: Tile cache insertion time: " << insert_timer.getTime()
				 << " microseconds" << endl;
    return ttt;
  }


  switch( c ){

  case JPEG:

    // Do our JPEG compression iff we have an 8 bit per channel image
    if( ttt.bpc == 8 ){
      if( loglevel >=2 ) compression_timer.start();
      len = jpeg->Compress( ttt );
      if( loglevel >= 2 ) *logfile << "TileManager :: JPEG Compression Time: "
				   << compression_timer.getTime() << " microseconds" << endl;
    }
    break;


  case DEFLATE:

    // No deflate for the time being ;-)
    if( loglevel >= 2 ) *logfile << "TileManager :: DEFLATE Compression requested: Not currently available" << endl;
    break;


  default:

    break;

  }


  // Add to our tile cache
  if( loglevel >= 2 ) insert_timer.start();
  tileCache->insert( ttt );
  if( loglevel >= 2 ) *logfile << "TileManager :: Tile cache insertion time: " << insert_timer.getTime()
			       << " microseconds" << endl;


  return ttt;

}



void TileManager::crop( RawTile *ttt ){

  int tw = image->getTileWidth();
  int th = image->getTileHeight();

  if( loglevel >= 3 ){
    *logfile << "TileManager :: Edge tile: Base size: " << tw << "x" << th
	     << ": This tile: " << ttt->width << "x" << ttt->height
	     << endl;
  }

  // Create a new buffer, fill it with the old data, then copy
  // back the cropped part into the RawTile buffer
  int len = tw * th * ttt->channels * ttt->bpc/8;
  unsigned char* buffer = (unsigned char*) malloc( len );
  unsigned char* src_ptr = (unsigned char*) memcpy( buffer, ttt->data, len );
  unsigned char* dst_ptr = (unsigned char*) ttt->data;

  // Copy one scanline at a time
  for( unsigned int i=0; i<ttt->height; i++ ){
    len =  ttt->width * ttt->channels * ttt->bpc/8;
    memcpy( dst_ptr, src_ptr, len );
    dst_ptr += len;
    src_ptr += tw * ttt->channels * ttt->bpc/8;
  }

  free( buffer );

  // Reset the data length
  len = ttt->width * ttt->height * ttt->channels * ttt->bpc/8;
  ttt->dataLength = len;
  ttt->padded = false;

}




RawTile TileManager::getTile( int resolution, int tile, int xangle, int yangle, int layers, CompressionType c ){

  RawTile* rawtile = NULL;
  string tileCompression;
  string compName;


  // Time the tile retrieval
  if( loglevel >= 2 ) tile_timer.start();


  /* Try to get this tile from our cache first as a JPEG, then uncompressed
     Otherwise decode one from the source image and add it to the cache
   */
  switch( c )
    {

    case JPEG:
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					  xangle, yangle, JPEG, jpeg->getQuality() )) ) break;
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, DEFLATE, 0 )) ) break;
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, UNCOMPRESSED, 0 )) ) break;
      break;


    case DEFLATE:

      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, DEFLATE, 0 )) ) break;
      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, UNCOMPRESSED, 0 )) ) break;
      break;


    case UNCOMPRESSED:

      if( (rawtile = tileCache->getTile( image->getImagePath(), resolution, tile,
					 xangle, yangle, UNCOMPRESSED, 0 )) ) break;
      break;


    default: 
      break;

    }


  // If we haven't been able to get a tile, get a raw one
  if( !rawtile || (rawtile && (rawtile->timestamp < image->timestamp)) ){

    if( rawtile && (rawtile->timestamp < image->timestamp) ){
      if( loglevel >= 3 ) *logfile << "TileManager :: Tile has old timestamp "
			           << rawtile->timestamp << " - " << image->timestamp
                                   << " ... updating" << endl;
    }

    RawTile newtile = this->getNewTile( resolution, tile, xangle, yangle, layers, c );

    if( loglevel >= 2 ) *logfile << "TileManager :: Total Tile Access Time: "
				 << tile_timer.getTime() << " microseconds" << endl;
    return newtile;
  }


  // Define our compression names
  switch( rawtile->compressionType ){
    case JPEG: compName = "JPEG"; break;
    case DEFLATE: compName = "DEFLATE"; break;
    case UNCOMPRESSED: compName = "UNCOMPRESSED"; break;
    default: break;
  }

  if( loglevel >= 2 ) *logfile << "TileManager :: Cache Hit for resolution: " << resolution
			       << ", tile: " << tile
			       << ", compression: " << compName << endl
			       << "TileManager :: Cache Size: "
			       << tileCache->getNumElements() << " tiles, "
			       << tileCache->getMemorySize() << " MB" << endl;


  // Check whether the compression used for out tile matches our requested compression type.
  // If not, we must convert

  if( c == JPEG && rawtile->compressionType == UNCOMPRESSED ){

    // Rawtile is a pointer to the cache data, so we need to create a copy of it in case we compress it
    RawTile ttt( *rawtile );

    // Do our JPEG compression iff we have an 8 bit per channel image
    if( rawtile->bpc == 8 ){

      // Crop if this is an edge tile
      if( ( (ttt.width != image->getTileWidth()) || (ttt.height != image->getTileHeight()) ) && ttt.padded ){
	if( loglevel >= 5 ) * logfile << "TileManager :: Cropping tile" << endl;
	this->crop( &ttt );
      }

      if( loglevel >=2 ) compression_timer.start();
      unsigned int oldlen = rawtile->dataLength;
      unsigned int newlen = jpeg->Compress( ttt );
      if( loglevel >= 2 ) *logfile << "TileManager :: JPEG requested, but UNCOMPRESSED compression found in cache." << endl
				   << "TileManager :: JPEG Compression Time: "
				   << compression_timer.getTime() << " microseconds" << endl
				   << "TileManager :: Compression Ratio: " << newlen << "/" << oldlen << " = "
				   << ( (float)newlen/(float)oldlen ) << endl;

      // Add our compressed tile to the cache
      if( loglevel >= 2 ) insert_timer.start();
      tileCache->insert( ttt );
      if( loglevel >= 2 ) *logfile << "TileManager :: Tile cache insertion time: " << insert_timer.getTime()
				   << " microseconds" << endl;

      if( loglevel >= 2 ) *logfile << "TileManager :: Total Tile Access Time: "
				   << tile_timer.getTime() << " microseconds" << endl;
      return RawTile( ttt );
    }
  }

  if( loglevel >= 2 ) *logfile << "TileManager :: Total Tile Access Time: "
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

  // The tile size of the source tile
  unsigned int src_tile_width = image->getTileWidth();
  unsigned int src_tile_height = image->getTileHeight();

  // The tile size of the destination tile
  unsigned int dst_tile_width = src_tile_width;
  unsigned int dst_tile_height = src_tile_height;

  // The basic tile size ie. not the current tile
  unsigned int basic_tile_width = src_tile_width;
  // unsigned int basic_tile_height = src_tile_height;

  int num_res = image->getNumResolutions();
  unsigned int im_width = image->image_widths[num_res-res-1];
  unsigned int im_height = image->image_heights[num_res-res-1];

  unsigned int rem_x = im_width % src_tile_width;
  unsigned int rem_y = im_height % src_tile_height;

  // The number of tiles in each direction
  unsigned int ntlx = (im_width / src_tile_width) + (rem_x == 0 ? 0 : 1);
  unsigned int ntly = (im_height / src_tile_height) + (rem_y == 0 ? 0 : 1);

  // Start and end tiles and pixel offsets
  unsigned int startx, endx, starty, endy, xoffset, yoffset;


  if( ! ( x==0 && y==0 && width==im_width && height==im_height ) ){
    // Calculate the start tiles
    startx = (unsigned int) ( x / src_tile_width );
    starty = (unsigned int) ( y / src_tile_height );
    xoffset = x % src_tile_width;
    yoffset = y % src_tile_height;
    endx = (unsigned int) ( (width + x) / src_tile_width ) + 1;
    endy = (unsigned int) ( (height + y) / src_tile_height ) + 1;

    if( loglevel >= 3 ){
      *logfile << "TileManager getRegion :: Tile Start: " << startx << "," << starty << ","
	       << xoffset << "," << yoffset << endl
	       << "TileManager getRegion :: End Tiles: " << endx << "," << endy << endl;
    }
  }
  else{
    startx = starty = xoffset = yoffset = 0;
    endx = ntlx;
    endy = ntly;
  }


  unsigned int channels = image->getNumChannels();
  unsigned int bpp = image->getNumBitsPerPixel();
  SampleType sampleType = image->getSampleType();

  // Create an empty tile with the correct dimensions
  RawTile region( 0, res, seq, ang, width, height, channels, bpp );
  region.dataLength = width * height * channels * bpp/8;
  region.sampleType = sampleType;

  // Allocate memory for the region
  if( bpp == 32 && sampleType == _FIXED ) region.data = new int[width*height*channels];
  else if( bpp == 32 && sampleType == _FLOAT ) region.data = new float[width*height*channels];
  else if( bpp == 16 ) region.data = new unsigned short[width*height*channels];
  else region.data = new unsigned char[width*height*channels];

  unsigned int current_height = 0;

  // Decode the image strip by strip
  for( unsigned int i=starty; i<endy; i++ ){

    unsigned int buffer_index = 0;

    // Keep track of the current pixel boundary horizontally. ie. only up
    //  to the beginning of the current tile boundary.
    unsigned int current_width = 0;

    for( unsigned int j=startx; j<endx; j++ ){

      // Time the tile retrieval
      if( loglevel >= 2 ) tile_timer.start();

      // Get an uncompressed tile
      RawTile rawtile = this->getTile( res, (i*ntlx) + j, seq, ang, layers, UNCOMPRESSED );

      if( loglevel >= 2 ){
	*logfile << "TileManager getRegion :: Tile access time " << tile_timer.getTime() << " microseconds for tile "
		 << (i*ntlx) + j << " at resolution " << res << endl;
      }


      // Only print this out once per image
      if( (loglevel >= 4) && (i==starty) && (j==starty) ){
	*logfile << "TileManager getRegion :: Tile data is " << rawtile.channels << " channels, "
		 << rawtile.bpc << " bits per channel" << endl;
      }

      // Set the tile width and height to be that of the source tile
      // - Use the rawtile data because if we take a tile from cache
      //   the image pointer will not necessarily be pointing to the
      //   the current tile
      //	src_tile_width = (*session->image)->getTileWidth();
      //	src_tile_height = (*session->image)->getTileHeight();
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

	if( j == startx ){
	  // Calculate the width used in the current tile
	  // If there is only 1 tile, the width is just the view width
	  if( j < endx - 1 ) dst_tile_width = src_tile_width - xoffset;
	  else dst_tile_width = width;
	  xf = xoffset;
	}
	else if( j == endx-1 ){
	  dst_tile_width = (width+x) % basic_tile_width;
	}

	if( i == starty ){
	  // Calculate the height used in the current row of tiles
	  // If there is only 1 row the height is just the view height
	  if( i < endy - 1 ) dst_tile_height = src_tile_height - yoffset;
	  else dst_tile_height = height;
	  yf = yoffset;
	}
	else if( i == endy-1 ){
	  dst_tile_height = (height+y) % basic_tile_width;
	}

	if( loglevel >= 4 ){
	  *logfile << "TileManager getRegion :: destination tile height: " << dst_tile_height
		   << ", tile width: " << dst_tile_width << endl;
	}
      }


      // Copy our tile data into the appropriate part of the strip memory
      // one whole tile width at a time
      for( unsigned int k=0; k<dst_tile_height; k++ ){

	buffer_index = (current_width*channels) + (k*width*channels) + (current_height*width*channels);
	unsigned int inx = ((k+yf)*rawtile.width*channels) + (xf*channels);

	// Simply copy the line of data across
	if( bpp == 32 && sampleType == _FIXED ){
	  int* ptr = (int*) rawtile.data;
	  int* buf = (int*) region.data;
	  memcpy( &buf[buffer_index], &ptr[inx], dst_tile_width*channels*4 );
	}
	else if( bpp == 32 && sampleType == _FLOAT ){
	  float* ptr = (float*) rawtile.data;
	  float* buf = (float*) region.data;
	  memcpy( &buf[buffer_index], &ptr[inx], dst_tile_width*channels*4 );
	}
	else if( bpp ==  16 ){
	  unsigned short* ptr = (unsigned short*) rawtile.data;
	  unsigned short* buf = (unsigned short*) region.data;
	  memcpy( &buf[buffer_index], &ptr[inx], dst_tile_width*channels*2 );
	}
	else{
	  unsigned char* ptr = (unsigned char*) rawtile.data;
	  unsigned char* buf = (unsigned char*) region.data;
	  memcpy( &buf[buffer_index], &ptr[inx], dst_tile_width*channels );
	}
      }

      current_width += dst_tile_width;
    }

    current_height += dst_tile_height;

  }

  return region;

}
