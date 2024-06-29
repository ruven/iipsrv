/*  IIPImage Server: OpenJPEG JPEG2000 handler

    Copyright (C) 2019-2024 Ruven Pillay.

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


#include "OpenJPEGImage.h"
#include "Logger.h"
#include <sstream>
#include <cmath>
#ifdef OPENJPEG_DEBUG
#include "Timer.h"
#endif

// Detect High Throughput JPEG2000
#define J2K_CCP_CBLKSTY_HT 0x40
#define J2K_CCP_CBLKSTY_HTMIXED 0x80


using namespace std;


// Reference our logging object
extern Logger logfile;


// Handle info, warning and error messages from OpenJPEG
static void error_callback( const char* msg, void* ){
  stringstream ss;
  ss << "OpenJPEG error :: " << msg;
  throw file_error( ss.str() );
}

#ifdef OPENJPEG_DEBUG
static void warning_callback( const char* msg, void* ){
  if( IIPImage::logging ) logfile << "OpenJPEG warning :: " << msg << endl;
}
static void info_callback( const char* msg, void* ){
  if( IIPImage::logging ) logfile << "OpenJPEG info :: " << msg;
}
#endif



void OpenJPEGImage::openImage()
{
  string filename = getFileName( currentX, currentY );

  // Update our timestamp
  updateTimestamp( filename );

  // Create decompression codec
  _codec = opj_create_decompress( OPJ_CODEC_JP2 );

  // Set info, warning and error handlers for codec - these need to be done after codec initialization
#ifdef OPENJPEG_DEBUG
  opj_set_info_handler( _codec, info_callback, NULL );
  opj_set_warning_handler( _codec, warning_callback, NULL );
#endif
  opj_set_error_handler( _codec, error_callback, NULL );

  // Setup decoder
  opj_dparameters_t parameters; // Set default decoder parameters
  opj_set_default_decoder_parameters( &parameters );
  if( !opj_setup_decoder( _codec, &parameters ) ){
    throw file_error( "OpenJPEG :: openImage() :: error setting up decoder" );
  }

#ifdef OPENJPEG_DEBUG
  Timer timer;
  timer.start();
#endif

  // Open the JPEG2000 file in read mode
  if( !(_stream = opj_stream_create_default_file_stream( filename.c_str(), true) ) ){
    throw file_error( "OpenJPEG :: Unable to open '" + filename + "'" );
  }

#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: openImage() :: " << "Stream created" << endl;
#endif

  // Read header
  if( !opj_read_header( _stream, _codec, &_image ) ){
    throw file_error( "OpenJPEG :: process() :: opj_read_header() failed" );
  }

#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: openImage() :: " << "Header read" << endl;
#endif

  // Load our metadata if not already loaded
  if( bpc == 0 ) loadImageInfo( currentX, currentY );

#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: openImage() :: " << timer.getTime() << " microseconds" << endl;
#endif

}



void OpenJPEGImage::closeImage()
{
#ifdef OPENJPEG_DEBUG
  Timer timer;
  timer.start();
#endif

  if( _codec && _stream ) opj_end_decompress( _codec, _stream );
  if( _codec ){
    opj_destroy_codec( _codec );
    _codec = NULL;
  }
  if( _stream ){
    opj_stream_destroy( _stream );
    _stream = NULL;
  }
  if( _image ){
    opj_image_destroy( _image );
    _image = NULL;
  }

#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: closeImage() :: " << timer.getTime() << " microseconds" << endl;
#endif
}



void OpenJPEGImage::loadImageInfo( int seq, int ang )
{

#ifdef OPENJPEG_DEBUG
  Timer timer;
  timer.start();
#endif

  // Get info structure
  opj_codestream_info_v2_t* cst_info = opj_get_cstr_info( _codec );
  numResolutions = cst_info->m_default_tile_info.tccp_info[0].numresolutions;
  quality_layers = cst_info->m_default_tile_info.numlayers;


  // High Throughput JPEG2000
#ifdef J2K_CCP_CBLKSTY_HT
  if( (cst_info->m_default_tile_info.tccp_info[0].cblksty & J2K_CCP_CBLKSTY_HT) != 0 ||
      (cst_info->m_default_tile_info.tccp_info[0].cblksty & J2K_CCP_CBLKSTY_HTMIXED) != 0 ){
    logfile << "OpenJPEG :: HTJ2K codestream" << endl;
  }
#endif


  // Close our info structure
  opj_destroy_cstr_info( &cst_info );


  channels = _image->numcomps;
  bpc = _image->comps[0].prec;


  // Empty any existing list of available resolution sizes
  image_widths.clear();
  image_heights.clear();

  // Save first resolution level
  unsigned int w = _image->x1 - _image->x0;
  unsigned int h = _image->y1 - _image->y0;
  image_widths.push_back(w);
  image_heights.push_back(h);

#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: DWT Levels: " << numResolutions << endl;
  logfile << "OpenJPEG :: Resolution : " << w << "x" << h << endl;
#endif

  // Loop through each resolution and calculate the image dimensions - 
  // We force a similar behaviour to TIFF with resolutions at floor(x/2)
  // rather than OpenJPEG's default ceil(x/2) 
  for( unsigned int c=1; c<numResolutions; c++ ){
    w = floor( w/2.0 );
    h = floor( h/2.0 );
    image_widths.push_back(w);
    image_heights.push_back(h);
#ifdef OPENJPEG_DEBUG
    logfile << "OpenJPEG :: Resolution : " << w << "x" << h << endl;
#endif
  }


  // If we don't have enough resolutions to fit a whole image into a single tile
  // we need to generate them ourselves virtually.
  unsigned int n = 1;
  w = image_widths[0];
  h = image_heights[0];
  while( (w>tile_widths[0]) || (h>tile_heights[0]) ){
    n++;
    w = floor( w/2.0 );
    h = floor( h/2.0 );
    if( n > numResolutions ){
      image_widths.push_back(w);
      image_heights.push_back(h);
    }
  }

  if( n > numResolutions ){
#ifdef OPENJPEG_DEBUG
    logfile << "OpenJPEG :: Warning! Insufficient resolution levels in JPEG2000 stream. Will generate "
	    << n-numResolutions << " extra levels dynamically -" << endl
	    << "OpenJPEG :: However, you are advised to regenerate the file with at least " << n << " levels" << endl;
#endif
    virtual_levels = n-numResolutions;
  }
  numResolutions = n;


  // Need to assign basic colorspace information
  if( channels == 1 ){
    colorspace = (bpc==1)? ColorSpace::BINARY : ColorSpace::GREYSCALE;
  }
  else if( channels == 3 ) colorspace = ColorSpace::sRGB;

  // Color space details
  string cs;
  switch( _image->color_space ){
    case OPJ_CLRSPC_SRGB:
      cs = "sRGB";
      break;
    case  OPJ_CLRSPC_SYCC:
      cs = "YUV";
      break;
    case OPJ_CLRSPC_CMYK:
      cs = "CMYK";
      break;
    case OPJ_CLRSPC_EYCC:
      cs = "e-YCC";
      break;
    case OPJ_CLRSPC_UNSPECIFIED:
      cs = "Unspecified";
      break;
    default:
      cs = "Unknown";
      break;
  }


#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: " << bpc << " bit data" << endl
	  << "OpenJPEG :: " << channels << " channels" << endl
	  << "OpenJPEG :: color space: " << cs << endl
	  << "OpenJPEG :: " << quality_layers << " quality layers detected" << endl;
#endif

  // For bilevel images, force channels to 1 as we sometimes come across such images which claim 3 channels
  if( bpc == 1 ) channels = 1;

  // Get the max and min values for our data type
  for( unsigned int i=0; i<channels; i++ ){
    min.push_back( 0.0 );
    if( bpc > 8 && bpc < 16 ) max.push_back( 1<<bpc );
    if( bpc == 16 ) max.push_back( 65535.0 );
    else max.push_back( 255.0 );
  }
  
  // Indicate that our metadata has been read
  isSet = true;


#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: loadImageInfo() :: " << timer.getTime() << " microseconds" << endl;
#endif
}



// Get an individual tile
RawTile OpenJPEGImage::getTile( int seq, int ang, unsigned int res, int layers, unsigned int tile, ImageEncoding e )
{

  // Scale up our output bit depth to the nearest factor of 8
  unsigned obpc = bpc;
  if( bpc <= 16 && bpc > 8 ) obpc = 16;
  else if( bpc <= 8 ) obpc = 8;

#ifdef OPENJPEG_DEBUG
  Timer timer;
  timer.start();
#endif

  if( res > numResolutions ){
    ostringstream tile_no;
    tile_no << "OpenJPEG :: Asked for non-existent resolution: " << res;
    throw file_error( tile_no.str() );
  }

  int vipsres = getNativeResolution( res );

  unsigned int tw = tile_widths[0];
  unsigned int th = tile_heights[0];
  
  // Get the width and height for last row and column tiles
  unsigned int rem_x = image_widths[vipsres] % tile_widths[0];
  unsigned int rem_y = image_heights[vipsres] % tile_heights[0];

  // Calculate the number of tiles in each direction
  unsigned int ntlx = (image_widths[vipsres] / tile_widths[0]) + (rem_x == 0 ? 0 : 1);
  unsigned int ntly = (image_heights[vipsres] / tile_heights[0]) + (rem_y == 0 ? 0 : 1);

  // Check whether requested tile exists
  if( tile >= ntlx*ntly ){
    ostringstream tile_no;
    tile_no << "OpenJPEG :: Asked for non-existent tile: " << tile;
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
  int xoffset = (tile % ntlx) * tile_widths[0];
  int yoffset = (unsigned int) floor((double)(tile/ntlx)) * tile_heights[0];
  
#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: Tile size: " << tw << "x" << th << " @" << channels << endl;
#endif

  // OpenJPEG only supports 8 or 16 bit images
  if( !( (obpc == 8) || (obpc == 16) ) ) throw file_error( "OpenJPEG :: Unsupported number of bits" );

  // Create our Rawtile object and initialize with data
  RawTile rawtile( tile, res, seq, ang, tw, th, channels, obpc );
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.allocate();

  // Process the tile
  process( res, layers, xoffset, yoffset, tw, th, rawtile.data );

#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: getTile() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;
}



// Get an entire region and not just a tile
RawTile OpenJPEGImage::getRegion( int ha, int va, unsigned int res, int layers, int x, int y, unsigned int w, unsigned int h ){

  // Scale up our output bit depth to the nearest factor of 8
  unsigned int obpc = bpc;
  if( bpc <= 16 && bpc > 8 ) obpc = 16;
  else if( bpc <= 8 ) obpc = 8;
  
#ifdef OPENJPEG_DEBUG
  Timer timer;
  timer.start();
#endif

  // OpenJPEG only supports 8 or 16 bit images
  if( !( (obpc == 8) || (obpc == 16) ) ) throw file_error( "OpenJPEG :: Unsupported number of bits" );

  RawTile rawtile( 0, res, ha, va, w, h, channels, obpc );
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.allocate();

  process( res, layers, x, y, w, h, rawtile.data );

#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: getRegion() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;
}



// Main processing function
void OpenJPEGImage::process( unsigned int res, int layers, int xoffset, int yoffset, unsigned int tw, unsigned int th, void *d )
{
  // Unfortunately, it's not currently possible to re-use OpenJPEG's stream or image structures,
  // so re-open if necessary
  if( !_image ) openImage();

  // Scale up our output bit depth to the nearest factor of 8
  unsigned int obpc = bpc;
  if( bpc <= 16 && bpc > 8 ) obpc = 16;
  else if( bpc <= 8 ) obpc = 8;

  unsigned int factor = 1;                  // Downsampling factor - set it to default value
  int vipsres = (numResolutions - 1) - res; // Reverse resolution number

  // Calculate number of extra resolutions needed that have not been encoded in the image
  if( res < virtual_levels ){
    factor = 2 * (virtual_levels - res);
    xoffset *= factor;
    yoffset *= factor;
    tw *= factor;
    th *= factor;
    // Set our resolution level back to the smallest original resolution
    vipsres = numResolutions - 1 - virtual_levels;
#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: using smallest existing resolution " << virtual_levels << endl;
#endif
  }

  // Set the number of layers to half of the number of detected layers if we have not set the
  // layers parameter manually. If layers is set to less than 0, use all layers.
  if( layers < 0 ) layers = quality_layers;
  else if( layers == 0 ) layers = ceil( quality_layers/2.0 );

  // Also make sure we have at least 1 layer
  if( layers < 1 ) layers = 1;


  // Set number of quality layer and resolution
  opj_dparameters_t params;
  params.cp_layer = layers;
  params.cp_reduce = vipsres;


  if( !opj_setup_decoder( _codec, &params ) ){
    throw file_error( "OpenJPEG :: process() :: opj_setup_decoder() failed" );
  }


  // Set resolution - hack for openjpeg up to 2.2.0
  for( OPJ_UINT32 i = 0; i < _image->numcomps; i++ ){
    _image->comps[i].factor = vipsres;
  }

  // Image location and size at requested resolution
  unsigned int x0 = xoffset << vipsres;
  unsigned int y0 = yoffset << vipsres;
  unsigned int w0 = (xoffset + tw) << vipsres;
  unsigned int h0 = (yoffset + th) << vipsres;

#ifdef OPENJPEG_DEBUG
  logfile << "OpenJPEG :: decoding " << layers << " quality layers" << endl;
  logfile << "OpenJPEG :: requested region at requested resolution: position: "
	  << xoffset << "x" << yoffset << ". size: " << tw << "x" << th << endl;
  logfile << "OpenJPEG :: region size mapped to full resolution: " << (tw<<vipsres) << "x" << (th<<vipsres) << endl;
#endif


  // Define our decoding region
  if( !opj_set_decode_area( _codec, _image, x0, y0, w0, h0 ) ){
    throw file_error( "OpenJPEG :: process() :: opj_set_decode_area() failed" );
  }

  // Perform decoding
  if( !opj_decode( _codec, _stream, _image ) ){
    throw file_error( "OpenJPEG :: process() :: opj_decode() failed" );
  }

  // Extract any ICC profile - unfortunately, can only get ICC profile after decoding
  int icc_length = _image->icc_profile_len;
  const char* icc = (const char*) _image->icc_profile_buf;
  if( icc_length > 0 ) metadata.insert( {"icc",string(icc,icc_length)} );
#ifdef OPENJPEG_DEBUG
  if( icc_length > 0 ){
    logfile << "OpenJPEG :: ICC profile detected with size " << icc_length << endl;
  }
#endif

  // Copy our decoded data by looping over all pixels
  size_t n = 0;

  for( unsigned int j=0; j < th; j += factor ){
    for( unsigned int i = 0; i < tw; i += factor ){
      size_t index = tw*j + i;
      for( unsigned int k = 0; k < channels; k++ ){
        // Handle 16 and 8 bit data:
	// OpenJPEG's output data is 32 bit unsigned int, so just mask of the bottom 2 bytes
	// for 16 bit output or bottom 1 byte for 8 bit
	if( obpc == 16 ){
	  ((unsigned short*)d)[n++] =(  (_image->comps[k].data[index]) & 0x0000ffff );
	}
	// Binary (bi-level) images need to be scaled up to 8 bits
	else if( bpc == 1 ){
	  ((unsigned char*)d)[n++] = ((_image->comps[k].data[index]) & 0x000000f) * 255;
	}
	else{
	  ((unsigned char*)d)[n++] = (_image->comps[k].data[index]) & 0x000000ff;
	}
      }
    }
  }

  // We need to close the image here in case we try to use the OpenJPEG
  // stream or image structures multiple times in the same request pipeline
  closeImage();

}
