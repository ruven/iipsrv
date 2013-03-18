/*  IIP Server: Kakadu JPEG2000 handler


    Development supported by Moravian Library in Brno (Moravska zemska
    knihovna v Brne, http://www.mzk.cz/) R&D grant MK00009494301 & Old
    Maps Online (http://www.oldmapsonline.org/) from the Ministry of
    Culture of the Czech Republic.


    Copyright (C) 2009-2012 IIPImage.
    Authors: Ruven Pillay & Petr Pridal

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


#include "KakaduImage.h"
#include <kdu_compressed.h>
#include <cmath>
#include <sstream>

// Required for get_nprocs_conf() on Linux
#ifdef NPROCS
#include <sys/sysinfo.h>
#endif

// On Mac OS X, define our own get_nprocs_conf()
#if defined (__APPLE__) || defined(__FreeBSD__)
#include <pthread.h>
#include <sys/sysctl.h>
unsigned int get_nprocs_conf(){
  int numProcessors = 0;
  size_t size = sizeof(numProcessors);
  int returnCode = sysctlbyname("hw.ncpu", &numProcessors, &size, NULL, 0);
  if( returnCode != 0 ) return 1;
  else return (unsigned int)numProcessors;

}
#define NPROCS
#endif


#include "Timer.h"
//#define DEBUG 1


using namespace std;

#ifdef DEBUG
extern std::ofstream logfile;
#endif


void KakaduImage::openImage() throw (string)
{

  if( isSet ) return;

  string filename = getFileName( currentX, currentY );

  // Check if our image has been modified
  updateTimestamp(filename);

  // Set our error handlers
  kdu_customize_warnings(&pretty_cout);
  kdu_customize_errors(&pretty_cerr);

  Timer timer;
  timer.start();

  // Open the JPX or JP2 file
  try{
    src.open( filename.c_str(), true );
  }
  catch (...){
    throw string( "Kakadu :: Unable to open '"+filename+"'"); // Rethrow the exception
  }

  if( jpx_input.open( &src, false ) != 1 ) throw string( "Kakadu :: Error opening '"+filename+"'" );

  // Get our JPX codestream
  jpx_stream = jpx_input.access_codestream(0);

  // Open the underlying JPEG2000 codestream
  input = NULL;
  input = jpx_stream.open_stream();
  codestream.create(input);

  // Set up the cache size and allow restarting
  //codestream.augment_cache_threshold(1024);
  codestream.set_fast();
  codestream.set_persistent();
  //  codestream.enable_restart();

  loadImageInfo( currentX, currentY );
  isSet = true;

#ifdef DEBUG
  logfile << "Kakadu :: openImage() :: " << timer.getTime() << " microseconds" << endl;
#endif

}


void KakaduImage::loadImageInfo( int seq, int ang ) throw(string)
{
  jp2_channels j2k_channels;
  jp2_palette j2k_palette;
  jp2_resolution j2k_resolution;
  jp2_colour j2k_colour;
  kdu_coords layer_size;

  jpx_layer_source jpx_layer = jpx_input.access_layer(0);

  j2k_channels = jpx_layer.access_channels();
  j2k_resolution = jpx_layer.access_resolution();
  j2k_colour = jpx_layer.access_colour(0);
  layer_size = jpx_layer.get_layer_size();

  int cmp, plt, stream_id;
  j2k_channels.get_colour_mapping(0,cmp,plt,stream_id);
  j2k_palette = jpx_stream.access_palette();

  image_widths.push_back(layer_size.x);
  image_heights.push_back(layer_size.y);
  channels = j2k_channels.get_num_colours();
  numResolutions = codestream.get_min_dwt_levels();
  bpp = codestream.get_bit_depth(0,true);

  // Loop through each resolution and get the image dimensions
  for( unsigned int c=1; c<numResolutions; c++ ){
   codestream.apply_input_restrictions(0,0,c,1,NULL,KDU_WANT_OUTPUT_COMPONENTS );
   kdu_dims layers;
   codestream.get_dims(0,layers,true);
   image_widths.push_back(layers.size.x);
   image_heights.push_back(layers.size.y);
#ifdef DEBUG
   logfile << "Kakadu :: Resolution : " << layers.size.x << "x" << layers.size.y << std::endl;
#endif
  }

  // If we don't have enough resolutions to fit a whole image into a single tile
  // we need to generate them ourselves virtually. Fortunately, the
  // kdu_region_decompressor function is able to handle the downsampling for us for one extra level.
  // Extra downsampling has to be done ourselves
  unsigned int n = 1;
  unsigned int w = image_widths[0];
  unsigned int h = image_heights[0];
  while( (w>tile_width) || (h>tile_height) ){
    n++;
    // We use ceil() rather than floor() because Kakadu generates it's resolutions in this way
    // TIFFs resolutions, on the other hand, are floor()
    w = ceil( w/2.0 );
    h = ceil( h/2.0 );
    if( n > numResolutions ){
      image_widths.push_back(w);
      image_heights.push_back(h);
    }
  }


  if( n > numResolutions ){
#ifdef DEBUG
    logfile << "Kakadu :: Warning! Insufficient resolution levels in JPEG2000 stream. Will generate " << n-numResolutions << " extra levels dynamically -" << endl
	    << "Kakadu :: However, you are advised to regenerate the file with at least " << n << " levels" << endl;
#endif
  }

  if( n > numResolutions+1 ) virtual_levels = n-numResolutions-1;
  numResolutions = n;


  // Set our colour space - we let Kakadu automatically handle CIELAB->sRGB conversion for the time being
  if( channels == 1 ) colourspace = GREYSCALE;
  else{
    jp2_colour_space cs = j2k_colour.get_space();
    if( cs == JP2_sRGB_SPACE || cs == JP2_CIELab_SPACE ) colourspace = sRGB;
    //else if ( cs == JP2_CIELab_SPACE ) colourspace = CIELAB;
  }


  // Get the number of quality layers - must first open a tile, however
  kdu_tile kt = codestream.open_tile(kdu_coords(0,0),NULL);
  max_layers = codestream.get_max_tile_layers();
#ifdef DEBUG
  logfile << "Kakadu :: " << bpp << " bit data" << endl
	  << "Kakadu :: " << channels << " channels" << endl
	  << "Kakadu :: colour space " << j2k_colour.get_space() << endl
	  << "Kakadu :: " << max_layers << " quality layers detected" << endl;
#endif
  kt.close();

  // JPEG doesn't handle bilevel images, so pack these into 8 bit greyscale
  if( bpp == 1 ) bpp = 8;

  // Get the max and min values for our data type
  double sminvalue[4], smaxvalue[4];
  for( int i=0; i<channels; i++ ){
    min.push_back( 0.0 );
    if( bpp == 16 ) max.push_back( 65535.0 );
    else max.push_back( 255.0 );
  }

}


// Close our image descriptors
void KakaduImage::closeImage()
{
#ifdef DEBUG
  Timer timer;
  timer.start();
#endif

  // Close our codestream - need to make sure it exists or it'll crash
  if( codestream.exists() ) codestream.destroy();

  // Close our JP2 family and JPX files
  src.close();

  jpx_input.close();

#ifdef DEBUG
  logfile << "Kakadu :: closeImage() :: " << timer.getTime() << " microseconds" << endl;
#endif
}


// Get an invidual tile
RawTile KakaduImage::getTile( int seq, int ang, unsigned int res, int layers, unsigned int tile ) throw (string)
{
  Timer timer;
  timer.start();

  if( res > numResolutions ){
    ostringstream tile_no;
    tile_no << "Kakadu :: Asked for non-existant resolution: " << res;
    throw tile_no.str();
  }

  int vipsres = ( numResolutions - 1 ) - res;

  unsigned int tw = tile_width;
  unsigned int th = tile_height;


  // Get the width and height for last row and column tiles
  unsigned int rem_x = image_widths[vipsres] % tile_width;
  unsigned int rem_y = image_heights[vipsres] % tile_height;


  // Calculate the number of tiles in each direction
  unsigned int ntlx = (image_widths[vipsres] / tw) + (rem_x == 0 ? 0 : 1);
  unsigned int ntly = (image_heights[vipsres] / th) + (rem_y == 0 ? 0 : 1);

  if( tile >= ntlx*ntly ){
    ostringstream tile_no;
    tile_no << "Kakadu :: Asked for non-existant tile: " << tile;
    throw tile_no.str();
  }


  // Alter the tile size if it's in the last column
  bool edge_x = false;
  if( ( tile % ntlx == ntlx - 1 ) && ( rem_x != 0 ) ) {
    tw = rem_x;
    edge_x = true;
  }

  // Alter the tile size if it's in the bottom row
  bool edge_y = false;
  if( ( tile / ntlx == ntly - 1 ) && rem_y != 0 ) {
    th = rem_y;
    edge_y = true;
  }


  // Calculate the pixel offsets for this tile
  int xoffset = (tile % ntlx) * tile_width;
  int yoffset = (unsigned int) floor((double)(tile/ntlx)) * tile_height;

#ifdef DEBUG
  logfile << "Kakadu :: Tile size: " << tw << "x" << th << " @" << channels << endl;
#endif


  // Create our Rawtile object and initialize with data
  RawTile rawtile( tile, res, seq, ang, tw, th, channels, bpp );


  // Create our raw tile buffer and initialize some values
  if( bpp == 16 ) rawtile.data = new unsigned short[tw*th*channels];
  else if( bpp == 8 ) rawtile.data = new unsigned char[tw*th*channels];
  else throw string( "Kakadu :: Unsupported number of bits" );

  rawtile.dataLength = tw*th*channels*bpp/8;
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;

  // Process the tile
  process( res, layers, xoffset, yoffset, tw, th, rawtile.data );


#ifdef DEBUG
  logfile << "Kakadu :: bytes parsed: " << codestream.get_total_bytes(true) << endl;
  logfile << "Kakadu :: getTile() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;

}


// Get an entire region and not just a tile
RawTile KakaduImage::getRegion( int seq, int ang, unsigned int res, int layers, int x, int y, unsigned int w, unsigned int h ) throw (string)
{
#ifdef DEBUG
  Timer timer;
  timer.start();
#endif
  RawTile rawtile( 0, res, seq, ang, w, h, channels, bpp );
  if( bpp == 16 ) rawtile.data = new unsigned short[w*h*channels];
  else if( bpp == 8 ) rawtile.data = new unsigned char[w*h*channels];
  else throw string( "Kakadu :: Unsupported number of bits" );
  rawtile.dataLength = w*h*channels*bpp/8;
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  process( res, layers, x, y, w, h, rawtile.data );
#ifdef DEBUG
  logfile << "Kakadu :: getRegion() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;

}


// Main processing function
void KakaduImage::process( unsigned int res, int layers, int xoffset, int yoffset, unsigned int tw, unsigned int th, void *d ) throw (string)
{
  int vipsres = ( numResolutions - 1 ) - res;

  // Handle virtual resolutions
  if( res < virtual_levels ){
    int factor = (int) pow(2.0,(int)(virtual_levels - res));

    xoffset *= factor;
    yoffset *= factor;
    tw *= factor;
    th *= factor;
    vipsres = numResolutions - 1 - virtual_levels;
    //adjust dimensions in nonvirtual layer if needed
    if(xoffset + tw > image_widths[vipsres]) tw = image_widths[vipsres] - xoffset;
    if(yoffset + th > image_heights[vipsres]) th = image_heights[vipsres] - yoffset;
  }

  // Set the number of layers to half of the number of detected layers if we have not set the
  // layers parameter manually. If layers is set to less than 0, use all layers.
  if( layers < 0 ) layers = max_layers;
  else if( layers == 0 ) layers = ceil( max_layers/2.0 );

  // Also make sure we have at least 1 layer
  if( layers < 1 ) layers = 1;


  // Set up the bounding box for our tile
  kdu_dims image_dims, canvas_dims;
  canvas_dims.pos = kdu_coords( xoffset, yoffset );
  canvas_dims.size = kdu_coords( tw, th );

  // Apply our resolution restrictions to calculate the rendering zone on the highest resolution
  // canvas
  codestream.apply_input_restrictions( 0,0,vipsres,layers,&canvas_dims,KDU_WANT_OUTPUT_COMPONENTS );
  codestream.map_region( 0, canvas_dims, image_dims, true );


  // Create some worker threads
#ifdef NPROCS
  int num_threads = get_nprocs_conf();
#else
  int num_threads = 0;
#endif


  kdu_thread_env env, *env_ref = NULL;
  if( num_threads > 0 ){
    env.create();
    for (int nt=0; nt < num_threads; nt++){
      // Unable to create all the threads requested
      if( !env.add_thread() ) num_threads = nt;
    }
    env_ref = &env;
  }



#ifdef DEBUG
  logfile << "Kakadu :: decompressor init with " << num_threads << " threads" << endl;
  logfile << "Kakadu :: decoding " << layers << " quality layers" << endl;
#endif


  // Setup tile and stripe buffers
  void *buffer = NULL;
  void *stripe_buffer = NULL;
  int *stripe_heights = NULL;


  try{

    codestream.apply_input_restrictions( 0, 0, vipsres, layers, &image_dims, KDU_WANT_OUTPUT_COMPONENTS );

    decompressor.start( codestream, false, true, env_ref, NULL );

    stripe_heights = new int[channels];
    codestream.get_dims(0,comp_dims,true);

#ifdef DEBUG
    logfile << "Kakadu :: decompressor starting" << endl;

    logfile << "Kakadu :: requested region on high resolution canvas: position: "
	    << image_dims.pos.x << "x" << image_dims.pos.y
	    << ". size: " << image_dims.size.x << "x" << image_dims.size.y << endl;

    logfile << "Kakadu :: mapped resolution region size: " << comp_dims.size.x << "x" << comp_dims.size.y << endl;
    logfile << "Kakadu :: About to pull stripes" << endl;
#endif


    int index = 0;
    bool continues = true;

    // Get our stripe heights so that we can allocate our stripe buffer
    // Assume that first stripe height is largest
    decompressor.get_recommended_stripe_heights( comp_dims.size.y,
						 1024, stripe_heights, NULL );

#ifdef DEBUG
    logfile << "Kakadu :: Allocating memory for stripe height " << stripe_heights[0] << endl;
#endif
    // Create our buffers
    if( bpp == 16 ){
      stripe_buffer = new kdu_uint16[tw*th*channels];
      buffer = new unsigned short[tw*th*channels];
    }
    else if( bpp == 8 ){
      stripe_buffer = new kdu_byte[tw*th*channels];
      buffer = new unsigned char[tw*th*channels];
    }


    while( continues ){

      decompressor.get_recommended_stripe_heights( comp_dims.size.y,
						   1024, stripe_heights, NULL );
      if( bpp == 16 ){
	bool s = false;
	continues = decompressor.pull_stripe( (kdu_int16*) stripe_buffer, stripe_heights, NULL, NULL, NULL, NULL, &s );
      }
      else if( bpp == 8 ){
	continues = decompressor.pull_stripe( (kdu_byte*) stripe_buffer, stripe_heights, NULL, NULL, NULL );
      }

#ifdef DEBUG
      logfile << "Kakadu :: stripe pulled" << endl;
#endif

      // Copy the data into the supplied buffer
      void *b1, *b2;
      if( bpp == 16 ){
	b1 = &( ((kdu_uint16*)stripe_buffer)[0] );
	b2 = &( ((unsigned short*)buffer)[index] );
      }
      else if( bpp == 8 ){
	b1 = &( ((kdu_byte*)stripe_buffer)[0] );
	b2 = &( ((unsigned char*)buffer)[index] );
      }

      memcpy( b2, b1, tw * stripe_heights[0] * channels * bpp/8 );

      // Advance our buffer pointer
      index += tw * stripe_heights[0] * channels;

#ifdef DEBUG
      logfile << "Kakadu :: stripe complete with height " << stripe_heights[0] << endl;
#endif

    }


    if( !decompressor.finish() ){
      throw( "Kakadu :: Error indicated by finish()" );
    }


    // Shrink virtual resolution tiles
    if( res < virtual_levels ){

#ifdef DEBUG
      logfile << "Kakadu :: resizing tile to virtual resolution" << endl;
#endif
      unsigned int n = 0;
      unsigned int factor = pow(2.0,(int)(virtual_levels-res));
      for( unsigned int j=0; j<th; j+=factor ){
	for( unsigned int i=0; i<tw; i+=factor ){
	  for( unsigned int k=0; k<channels; k++ ){
	    // Handle 16 and 8 bit data
	    if( bpp==16 ){
	      ((unsigned short*)d)[n++] = ((unsigned short*)buffer)[j*tw*channels + i*channels + k];
	    }
	    else if( bpp==8 ){
	      ((unsigned char*)d)[n++] = ((unsigned char*)buffer)[j*tw*channels + i*channels + k];
	    }
	  }
	}
      }
    }
    else memcpy( d, buffer, tw*th*channels * bpp/8 );

    // Delete our local buffer
    delete_buffer( buffer );

#ifdef DEBUG
    logfile << "Kakadu :: decompressor completed" << endl;
#endif


  }
  catch (...){
    // Shut down our decompressor, delete our buffers, destroy our threads and codestream before rethrowing the exception
    decompressor.finish();
    if( env.exists() ) env.destroy();
    delete_buffer( stripe_buffer );
    delete_buffer( buffer );
    if( stripe_heights ) delete[] stripe_heights;
    throw string( "Kakadu :: Core Exception Caught"); // Rethrow the exception
  }


  // Destroy our threads
  if( env.exists() ) env.destroy();

  // Delete our stripe buffer
  delete_buffer( stripe_buffer );
  if( stripe_heights ) delete[] stripe_heights;

}


// Delete our buffers
void KakaduImage::delete_buffer( void* buffer ){
  if( buffer ){
    if( bpp == 16 ) delete[] (kdu_uint16*) buffer;
    else if( bpp == 8 ) delete[] (kdu_byte*) buffer;
  }


}
