/*  IIP Server: Kakadu JPEG2000 handler


    Development supported by Moravian Library in Brno (Moravska zemska
    knihovna v Brne, http://www.mzk.cz/) R&D grant MK00009494301 & Old
    Maps Online (http://www.oldmapsonline.org/) from the Ministry of
    Culture of the Czech Republic.


    Copyright (C) 2009-2016 IIPImage.
    Author: Ruven Pillay

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


void KakaduImage::openImage() throw (file_error)
{
  string filename = getFileName( currentX, currentY );

  // Update our timestamp
  updateTimestamp( filename );

  // Set our error handlers
  kdu_customize_warnings(&pretty_cout);
  kdu_customize_errors(&pretty_cerr);

#ifdef DEBUG
  Timer timer;
  timer.start();
#endif

  // Open the JPX or JP2 file
  try{
    src.open( filename.c_str(), true );
    if( jpx_input.open( &src, false ) != 1 ) throw 1;
  }
  catch (...){
    throw file_error( "Kakadu :: Unable to open '"+filename+"'"); // Rethrow the exception
  }


  // Get our JPX codestream
  try{
    jpx_stream = jpx_input.access_codestream(0);
    if( !jpx_stream.exists() ) throw 1;
  }
  catch (...){
    throw file_error( "Kakadu :: No codestream in file '"+filename+"'"); // Rethrow exception
  }


  // Open the underlying JPEG2000 codestream
  input = NULL;
  input = jpx_stream.open_stream();

  // Create codestream
  codestream.create(input);
  if( !codestream.exists() ) throw file_error( "Kakadu :: Unable to create codestream for '"+filename+"'"); // Throw exception

  // Set up the cache size and allow restarting
  //codestream.augment_cache_threshold(1024);
  codestream.set_fast();
  codestream.set_persistent();
  //  codestream.enable_restart();

  // Load our metadata if not already loaded
  if( bpc == 0 ) loadImageInfo( currentX, currentY );

#ifdef DEBUG
  logfile << "Kakadu :: openImage() :: " << timer.getTime() << " microseconds" << endl;
#endif

}


void KakaduImage::loadImageInfo( int seq, int ang ) throw(file_error)
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

  image_widths.push_back(layer_size.x);
  image_heights.push_back(layer_size.y);
  channels = codestream.get_num_components();
  numResolutions = codestream.get_min_dwt_levels();
  bpc = codestream.get_bit_depth(0,true);

  unsigned int w = layer_size.x;
  unsigned int h = layer_size.y;

#ifdef DEBUG
  logfile << "Kakadu :: DWT Levels: " << numResolutions << endl;
  logfile << "Kakadu :: Resolution : " << w << "x" << h << endl;
#endif

  // Loop through each resolution and calculate the image dimensions - 
  // We calculate ourselves rather than relying on get_dims() to force a similar
  // behaviour to TIFF with resolutions at floor(x/2) rather than Kakadu's default ceil(x/2) 
  for( unsigned int c=1; c<numResolutions; c++ ){
    //    codestream.apply_input_restrictions(0,0,c,1,NULL,KDU_WANT_OUTPUT_COMPONENTS);
    //    kdu_dims layers;
    //    codestream.get_dims(0,layers,true);
    //    image_widths.push_back(layers.size.x);
    //    image_heights.push_back(layers.size.y);
    w = floor( w/2.0 );
    h = floor( h/2.0 );
    image_widths.push_back(w);
    image_heights.push_back(h);
#ifdef DEBUG
    logfile << "Kakadu :: Resolution : " << w << "x" << h << endl;
#endif
  }

  // If we don't have enough resolutions to fit a whole image into a single tile
  // we need to generate them ourselves virtually. Fortunately, the
  // kdu_region_decompressor function is able to handle the downsampling for us for one extra level.
  // Extra downsampling has to be done ourselves
  unsigned int n = 1;
  w = image_widths[0];
  h = image_heights[0];
  while( (w>tile_width) || (h>tile_height) ){
    n++;
    w = floor( w/2.0 );
    h = floor( h/2.0 );
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

  if( n > numResolutions ) virtual_levels = n-numResolutions-1;
  numResolutions = n;


  // Check for a palette and LUT - only used for bilevel images for now
  int cmp, plt, stream_id,format=0;
#if defined(KDU_MAJOR_VERSION) && (KDU_MAJOR_VERSION >= 7) && (KDU_MINOR_VERSION >= 8)
  // API change for get_colour_mapping in Kakadu 7.8
  j2k_channels.get_colour_mapping(0,cmp,plt,stream_id,format);
#else
  j2k_channels.get_colour_mapping(0,cmp,plt,stream_id);
#endif

  j2k_palette = jpx_stream.access_palette();

  if( j2k_palette.exists() && j2k_palette.get_num_luts()>0 ){
    int entries = j2k_palette.get_num_entries();
    float *lt = new float[entries];
    j2k_palette.get_lut(0,lt);    // Note that we extract only first LUT
    // Force to unsigned format, scale to 8 bit and load these into our LUT vector
    for( int n=0; n<entries; n++ ){
      lut.push_back((int)((lt[n]+0.5)*255));
    }
    delete[] lt;
#ifdef DEBUG
    logfile << "Kakadu :: Palette with " << j2k_palette.get_num_luts() << " LUT and " << entries
	    << " entries/LUT with values " << lut[0] << "," << lut[1] << endl;
#endif
  }

  // if the ICC color profile hasn't already been set for this image, try to read it
  if ( icc_profile_buf == NULL ) {
    int proflen=0;
    unsigned char *buf=NULL;

    // Kakadu's method for fetching the ICC color profile
    buf = const_cast<unsigned char*>(j2k_colour.get_icc_profile(&proflen));

    // make a copy of the icc profile since changing TIFF directory frees the original memory
    if ( proflen > 0 ) {
      icc_profile_buf = new unsigned char[proflen];
      memcpy(&icc_profile_buf[0], buf, proflen);
      icc_profile_len = proflen;
    }
  }

  // Set our colour space - we let Kakadu automatically handle CIELAB->sRGB conversion for the time being
  if( channels == 1 ) colourspace = GREYSCALE;
  else{
    jp2_colour_space cs = j2k_colour.get_space();
    if( cs == JP2_sRGB_SPACE || cs == JP2_iccRGB_SPACE || cs == JP2_esRGB_SPACE || cs == JP2_CIELab_SPACE ) colourspace = sRGB;
    //else if ( cs == JP2_CIELab_SPACE ) colourspace = CIELAB;
    else {
#ifdef DEBUG
    	logfile << "WARNING : colour space not found, setting sRGB colour space value" << endl;
#endif
    	colourspace = sRGB;
    }
  }


  // Get the number of quality layers - must first open a tile, however
  kdu_tile kt = codestream.open_tile(kdu_coords(0,0),NULL);
  quality_layers = codestream.get_max_tile_layers();
#ifdef DEBUG
  string cs;
  switch( j2k_colour.get_space() ){
    case JP2_sRGB_SPACE:
      cs = "JP2_sRGB_SPACE";
      break;
    case JP2_sLUM_SPACE:
      cs =  "JP2_sLUM_SPACE";
      break;
    case JP2_CIELab_SPACE:
      cs = "JP2_CIELab_SPACE";
      break;
    default:
      cs = j2k_colour.get_space();
      break;
  }
  logfile << "Kakadu :: " << bpc << " bit data" << endl
	  << "Kakadu :: " << channels << " channels" << endl
	  << "Kakadu :: colour space: " << cs << endl
	  << "Kakadu :: " << quality_layers << " quality layers detected" << endl;
#endif
  kt.close();

  // For bilevel images, force channels to 1 as we sometimes come across such images which claim 3 channels
  if( bpc == 1 ) channels = 1;

  // Get the max and min values for our data type
  //double sminvalue[4], smaxvalue[4];
  for( unsigned int i=0; i<channels; i++ ){
    min.push_back( 0.0 );
    if( bpc > 8 && bpc <= 16 ) max.push_back( 65535.0 );
    else max.push_back( 255.0 );
  }

  isSet = true;
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
RawTile KakaduImage::getTile( int seq, int ang, unsigned int res, int layers, unsigned int tile ) throw (file_error)
{

  // Scale up our output bit depth to the nearest factor of 8
  unsigned obpc = bpc;
  if( bpc <= 16 && bpc > 8 ) obpc = 16;
  else if( bpc <= 8 ) obpc = 8;

#ifdef DEBUG
  Timer timer;
  timer.start();
#endif

  if( res > numResolutions ){
    ostringstream tile_no;
    tile_no << "Kakadu :: Asked for non-existent resolution: " << res;
    throw file_error( tile_no.str() );
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
    tile_no << "Kakadu :: Asked for non-existent tile: " << tile;
    throw file_error( tile_no.str() );
  }

  // Alter the tile size if it's in the last column
  if( ( tile % ntlx == ntlx - 1 ) && ( rem_x != 0 ) ) {
    tw = rem_x;
  }

  // Alter the tile size if it's in the bottom row
  if( ( tile / ntlx == ntly - 1 ) && rem_y != 0 ) {
    th = rem_y;
  }


  // Calculate the pixel offsets for this tile
  int xoffset = (tile % ntlx) * tile_width;
  int yoffset = (unsigned int) floor((double)(tile/ntlx)) * tile_height;

#ifdef DEBUG
  logfile << "Kakadu :: Tile size: " << tw << "x" << th << "@" << channels << endl;
#endif


  // Create our Rawtile object and initialize with data
  RawTile rawtile( tile, res, seq, ang, tw, th, channels, obpc );


  // Create our raw tile buffer and initialize some values
  if( obpc == 16 ) rawtile.data = new unsigned short[tw*th*channels];
  else if( obpc == 8 ) rawtile.data = new unsigned char[tw*th*channels];
  else throw file_error( "Kakadu :: Unsupported number of bits" );

  rawtile.dataLength = tw*th*channels*obpc/8;
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
RawTile KakaduImage::getRegion( int seq, int ang, unsigned int res, int layers, int x, int y, unsigned int w, unsigned int h ) throw (file_error)
{
  // Scale up our output bit depth to the nearest factor of 8
  unsigned int obpc = bpc;
  if( bpc <= 16 && bpc > 8 ) obpc = 16;
  else if( bpc <= 8 ) obpc = 8;

#ifdef DEBUG
  Timer timer;
  timer.start();
#endif

  RawTile rawtile( 0, res, seq, ang, w, h, channels, obpc );

  if( obpc == 16 ) rawtile.data = new unsigned short[w*h*channels];
  else if( obpc == 8 ) rawtile.data = new unsigned char[w*h*channels];
  else throw file_error( "Kakadu :: Unsupported number of bits" );

  rawtile.dataLength = w*h*channels*obpc/8;
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;

  process( res, layers, x, y, w, h, rawtile.data );

#ifdef DEBUG
  logfile << "Kakadu :: getRegion() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;

}


// Main processing function
void KakaduImage::process( unsigned int res, int layers, int xoffset, int yoffset, unsigned int tw, unsigned int th, void *d ) throw (file_error)
{
  // Scale up our output bit depth to the nearest factor of 8
  unsigned int obpc = bpc;
  if( bpc <= 16 && bpc > 8 ) obpc = 16;
  else if( bpc <= 8 ) obpc = 8;

  int vipsres = ( numResolutions - 1 ) - res;

  // Handle virtual resolutions
  if( res < virtual_levels ){
    unsigned int factor = 1 << (virtual_levels-res);
    xoffset *= factor;
    yoffset *= factor;
    tw *= factor;
    th *= factor;
    vipsres = numResolutions - 1 - virtual_levels;
#ifdef DEBUG
  logfile << "Kakadu :: using smallest existing resolution " << virtual_levels << endl;
#endif
  }

  // Set the number of layers to half of the number of detected layers if we have not set the
  // layers parameter manually. If layers is set to less than 0, use all layers.
  if( layers < 0 ) layers = quality_layers;
  else if( layers == 0 ) layers = ceil( quality_layers/2.0 );

  // Also make sure we have at least 1 layer
  if( layers < 1 ) layers = 1;


  // Set up the bounding box for our tile
  kdu_dims image_dims, canvas_dims;
  canvas_dims.pos = kdu_coords( xoffset, yoffset );
  canvas_dims.size = kdu_coords( tw, th );

  // Check our codestream status - throw exception for malformed codestreams
  if( !codestream.exists() ) throw file_error( "Kakadu :: Malformed JPEG2000 - unable to access codestream");

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

    // Note that we set max channels rather than leave the default to strip off alpha channels
    codestream.apply_input_restrictions( 0, channels, vipsres, layers, &image_dims, KDU_WANT_OUTPUT_COMPONENTS );

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

    if( obpc == 16 ){
      stripe_buffer = new kdu_uint16[tw*stripe_heights[0]*channels];
      buffer = new unsigned short[tw*th*channels];
    }
    else if( obpc == 8 ){
      stripe_buffer = new kdu_byte[tw*stripe_heights[0]*channels];
      buffer = new unsigned char[tw*th*channels];
    }

    // Keep track of changes in stripe heights
    int previous_stripe_heights = stripe_heights[0];


    while( continues ){


      decompressor.get_recommended_stripe_heights( comp_dims.size.y,
						   1024, stripe_heights, NULL );


      // If we have a larger stripe height, allocate new memory for this
      if( stripe_heights[0] > previous_stripe_heights ){

	// First delete then re-allocate our buffers
	delete_buffer( stripe_buffer );
	if( obpc == 16 ){
	  stripe_buffer = new kdu_uint16[tw*stripe_heights[0]*channels];
	}
	else if( obpc == 8 ){
	  stripe_buffer = new kdu_byte[tw*stripe_heights[0]*channels];
	}

#ifdef DEBUG
	logfile << "Kakadu :: Stripe height increase: re-allocating memory for height " << stripe_heights[0] << endl;
#endif
      }

      // Check for zero height, which can occur with incorrect position or size parameters
      if( stripe_heights[0] == 0 ){
#ifdef DEBUG
	logfile << "Kakadu :: Error: Zero stripe height" << endl;
#endif
	throw 1;
      }


      if( obpc == 16 ){
	// Set these to false to get unsigned 16 bit values
	bool s[3] = {false,false,false};
	continues = decompressor.pull_stripe( (kdu_int16*) stripe_buffer, stripe_heights, NULL, NULL, NULL, NULL, s );
      }
      else if( obpc == 8 ){
	continues = decompressor.pull_stripe( (kdu_byte*) stripe_buffer, stripe_heights, NULL, NULL, NULL );
      }


#ifdef DEBUG
      logfile << "Kakadu :: stripe pulled" << endl;
#endif

      // Copy the data into the supplied buffer
      void *b1, *b2;
      if( obpc == 16 ){
	b1 = &( ((kdu_uint16*)stripe_buffer)[0] );
	b2 = &( ((unsigned short*)buffer)[index] );
      }
      else{ // if( obpc == 8 ){
	b1 = &( ((kdu_byte*)stripe_buffer)[0] );
	b2 = &( ((unsigned char*)buffer)[index] );

	/* Handle 1 bit bilevel images, which we output scaled to 8 bits
	   - ideally we would do this in the Kakadu pull_stripe function,
	   but the precisions parameter seems not to work as expected.
	   When requesting OUTPUT_COMPONENTS, data is provided as 0 or 128,
	   so simply scale this up to [0,255]
	*/
	if( bpc == 1 ){

	  unsigned int k = tw * stripe_heights[0] * channels;

	  // Deal with inverted LUTs - we should really handle LUTs more generally, however
	  if( !lut.empty() && lut[0]>lut[1] ){
	    for( unsigned int n=0; n<k; n++ ){
	      ((kdu_byte*)stripe_buffer)[n] =  ~(-((kdu_byte*)stripe_buffer)[n] >> 8);
	    }
	  }
	  else{
	    for( unsigned int n=0; n<k; n++ ){
	      ((kdu_byte*)stripe_buffer)[n] =  (-((kdu_byte*)stripe_buffer)[n] >> 8);
	    }
	  }
	}
      }

      memcpy( b2, b1, tw * stripe_heights[0] * channels * obpc/8 );

      // Advance our output buffer pointer
      index += tw * stripe_heights[0] * channels;

#ifdef DEBUG
      logfile << "Kakadu :: stripe complete with height " << stripe_heights[0] << endl;
#endif

    }


    if( !decompressor.finish() ){
      throw file_error( "Kakadu :: Error indicated by finish()" );
    }


    // Shrink virtual resolution tiles
    if( res < virtual_levels ){

#ifdef DEBUG
      logfile << "Kakadu :: resizing tile to virtual resolution with factor " << (1 << (virtual_levels-res)) << endl;
#endif

      unsigned int n = 0;
      unsigned int factor = 1 << (virtual_levels-res);
      for( unsigned int j=0; j<th; j+=factor ){
	for( unsigned int i=0; i<tw; i+=factor ){
	  for( unsigned int k=0; k<channels; k++ ){
	    // Handle 16 and 8 bit data
	    if( obpc==16 ){
	      ((unsigned short*)d)[n++] = ((unsigned short*)buffer)[j*tw*channels + i*channels + k];
	    }
	    else if( obpc==8 ){
	      ((unsigned char*)d)[n++] = ((unsigned char*)buffer)[j*tw*channels + i*channels + k];
	    }
	  }
	}
      }
    }
    else memcpy( d, buffer, tw*th*channels * obpc/8 );

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
    throw file_error( "Kakadu :: Core Exception Caught"); // Rethrow the exception
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
    if( bpc <= 16 && bpc > 8 ) delete[] (kdu_uint16*) buffer;
    else if( bpc<=8 ) delete[] (kdu_byte*) buffer;
  }


}
