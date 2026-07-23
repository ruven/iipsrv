/*  IIPImage Server: Grok JPEG2000 handler

    Copyright (C) 2019-2026 Ruven Pillay.
    Modified for Grok support

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


#include "GrokImage.h"
#include "Logger.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>
#include <cmath>
#ifdef GROK_DEBUG
#include "Timer.h"
#endif

using namespace std;


// Reference our logging object
extern Logger logfile;

// ---------------------------------------------------------------------------
// Message callbacks
// ---------------------------------------------------------------------------

// Handle info, warning and error messages from Grok
static void error_callback( const char* msg, void* )
{
  if( IIPImage::logging ) logfile << "Grok error :: " << msg << endl;
}

#ifdef GROK_DEBUG
static void warning_callback( const char* msg, void* )
{
  if( IIPImage::logging ) logfile << "Grok warning :: " << msg << endl;
}
static void info_callback( const char* msg, void* )
{
  if( IIPImage::logging ) logfile << "Grok info :: " << msg;
}
#endif

static std::once_flag grok_init_flag;

static void grok_do_init() {
  grk_msg_handlers msg_handlers = {};
  msg_handlers.error_callback = error_callback;
#ifdef GROK_DEBUG
  msg_handlers.info_callback = info_callback;
  msg_handlers.warn_callback = warning_callback;
#endif
  grk_set_msg_handlers( msg_handlers );
  uint32_t num_threads = 2;
  const char* thread_setting = std::getenv( "GROK_THREADS" );
  if( thread_setting && *thread_setting ){
    char* end = nullptr;
    unsigned long parsed = std::strtoul( thread_setting, &end, 10 );
    if( end != thread_setting && *end == '\0' &&
        parsed <= std::numeric_limits<uint32_t>::max() ){
      num_threads = static_cast<uint32_t>( parsed );
    }
  }

  // Initialize the process-wide Grok runtime.  GROK_THREADS=0 uses all CPUs.
  grk_initialize( nullptr, num_threads, nullptr );
  if( IIPImage::logging ) logfile << "Grok initialized using " << (num_threads == 0 ? "all" : std::to_string(num_threads)) << " threads." << endl;

  // Release the process-wide worker pool on a normal process exit.
  std::atexit( [](){ grk_deinitialize(); } );
}

void GrokImage::initDecompressParams()
{
  _decompress_params = {};
  _decompress_params.core.reduce = 0;
  _decompress_params.core.layers_to_decompress = 0; // All layers
  _decompress_params.core.tile_cache_strategy = GRK_TILE_CACHE_NONE;
  _decompress_params.core.skip_allocate_composite = false;
  _decompress_params.asynchronous = false;
}

// ---------------------------------------------------------------------------
// openImage
// ---------------------------------------------------------------------------

void GrokImage::openImage()
{
  string filename = getFileName( currentX, currentY );

  // Update our timestamp
  updateTimestamp( filename );

  std::call_once( grok_init_flag, grok_do_init );

  // Setup stream parameters for file
  _stream_params = {};
  if( filename.size() >= GRK_PATH_LEN ){
    throw file_error( "Grok :: openImage() :: file path exceeds GRK_PATH_LEN: " + filename );
  }
  std::strncpy(_stream_params.file, filename.c_str(), GRK_PATH_LEN - 1);
  _stream_params.file[GRK_PATH_LEN - 1] = '\0';
  _stream_params.is_read_stream = true;
  _stream_params.use_stdio = false; // Use memory mapping for better performance

#ifdef GROK_DEBUG
  Timer timer;
  timer.start();
#endif

  // Create decompression codec using grk_decompress_init
  _codec = grk_decompress_init( &_stream_params, &_decompress_params );
  if( !_codec ){
    throw file_error( "Grok :: openImage() :: Unable to create decompression codec for '" + filename + "'" );
  }

#ifdef GROK_DEBUG
  logfile << "Grok :: openImage() :: " << "Codec created" << endl;
#endif

  // Grok consumes colour/output settings from grk_header_info, not from
  // grk_decompress_parameters.  PNG describes the normalized in-memory
  // representation iipsrv expects: palettes expanded and YCC/CMYK converted
  // while grayscale remains single-channel.
  _header = {};
  _header.decompress_fmt = GRK_FMT_PNG;
  _header.color_space = GRK_CLRSPC_UNKNOWN;
  _header.force_rgb = false;
  _header.apply_palette = true;
  _header.upsample = true;
  _header.split_by_component = false;
  _header.single_tile_decompress = false;

  if( !grk_decompress_read_header( _codec, &_header ) ){
    grk_object_unref(_codec);
    _codec=nullptr;
    throw file_error( "Grok :: openImage() :: grk_decompress_read_header() failed" );
  }
  _header_read = true;

#ifdef GROK_DEBUG
  logfile << "Grok :: openImage() :: " << "Header read" << endl;
#endif

  // Load our metadata if not already loaded
  if( bpc == 0 ) loadImageInfo( currentX, currentY );

#ifdef GROK_DEBUG
  logfile << "Grok :: openImage() :: " << timer.getTime() << " microseconds" << endl;
#endif
}



void GrokImage::closeImage()
{
#ifdef GROK_DEBUG
  Timer timer;
  timer.start();
#endif

  if( _codec ){
    grk_object_unref( _codec ); // This deallocate also _image
    _codec = nullptr;
  }
  _image = nullptr; // _image is owned by the codec – do NOT unref it separately.
  _header_read = false;

#ifdef GROK_DEBUG
  logfile << "Grok :: closeImage() :: " << timer.getTime() << " microseconds" << endl;
#endif
}



void GrokImage::loadImageInfo( int seq, int ang )
{

#ifdef GROK_DEBUG
  Timer timer;
  timer.start();
#endif

  if( !_header_read || _header.header_image.numcomps == 0 ||
      !_header.header_image.comps ){
    throw file_error( "Grok :: loadImageInfo() :: invalid image header" );
  }

  numResolutions = _header.numresolutions;
  quality_layers = _header.num_layers;

  // Check for HTJ2K (High Throughput JPEG2000) - check code block style flags
  if( (_header.cblk_sty & 0x40) != 0 || (_header.cblk_sty & 0x80) != 0 ){
    logfile << "Grok :: HTJ2K codestream" << endl;
  }

  channels = _header.header_image.decompress_num_comps;
  if( channels == 0 ) channels = _header.header_image.numcomps;
  bpc = _header.header_image.decompress_prec;
  if( bpc == 0 ) bpc = _header.header_image.comps[0].prec;
  if( bpc == 0 || bpc > 16 ){
    throw file_error( "Grok :: loadImageInfo() :: unsupported component precision" );
  }

  // Get image dimensions
  unsigned int w = _header.header_image.x1 - _header.header_image.x0;
  unsigned int h = _header.header_image.y1 - _header.header_image.y0;

  // Empty any existing list of available resolution sizes
  image_widths.clear();
  image_heights.clear();

  // Save first resolution level
  image_widths.push_back(w);
  image_heights.push_back(h);

#ifdef GROK_DEBUG
  logfile << "Grok :: DWT Levels: " << numResolutions << endl;
  logfile << "Grok :: Resolution : " << w << "x" << h << endl;
#endif

  // Loop through each resolution and calculate the image dimensions -
  // for JPEG2000, these are defined as ceil(x/2)
  for( unsigned int c=1; c<numResolutions; c++ ){
    w = ceil( w / 2.0 );
    h = ceil( h / 2.0 );
    image_widths.push_back(w);
    image_heights.push_back(h);
#ifdef GROK_DEBUG
    logfile << "Grok :: Resolution : " << w << "x" << h << endl;
#endif
  }

  // If we don't have enough resolutions to fit a whole image into a single tile
  // we need to generate them ourselves virtually.
  unsigned int n = 1;
  w = image_widths[0];
  h = image_heights[0];
  while( (w>tile_widths[0]) || (h>tile_heights[0]) ){
    n++;
    w = ceil( w / 2.0 );
    h = ceil( h / 2.0 );
    if( n > numResolutions ){
      image_widths.push_back(w);
      image_heights.push_back(h);
    }
  }

  if( n > numResolutions ){
#ifdef GROK_DEBUG
    logfile << "Grok :: Warning! Insufficient resolution levels in JPEG2000 stream. Will generate "
            << n-numResolutions << " extra levels dynamically -" << endl
            << "Grok :: However, you are advised to regenerate the file with at least " << n << " levels" << endl;
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
  GRK_COLOR_SPACE grk_colorspace = _header.header_image.decompress_colour_space;
  if( grk_colorspace == GRK_CLRSPC_UNKNOWN )
    grk_colorspace = _header.header_image.color_space;
  switch( grk_colorspace ){
    case GRK_CLRSPC_SRGB:
      cs = "sRGB";
      colorspace = ColorSpace::sRGB;
      break;
    case GRK_CLRSPC_GRAY:
      cs = "Grayscale";
      colorspace = (bpc == 1) ? ColorSpace::BINARY : ColorSpace::GREYSCALE;
      break;
    case GRK_CLRSPC_SYCC:
      cs = "YUV";
      break;
    case GRK_CLRSPC_DEFAULT_CIE:
    case GRK_CLRSPC_CUSTOM_CIE:
      cs = "CIELab";
      colorspace = ColorSpace::CIELAB;
      break;
    case GRK_CLRSPC_CMYK:
      cs = "CMYK";
      break;
    case GRK_CLRSPC_UNKNOWN:
      cs = "Unknown";
      break;
    default:
      cs = "Other";
      break;
  }

#ifdef GROK_DEBUG
  logfile << "Grok :: " << bpc << " bit data" << endl
          << "Grok :: " << channels << " channels" << endl
          << "Grok :: color space: " << cs << endl
          << "Grok :: " << quality_layers << " quality layers detected" << endl;
#endif

  // Get the max and min values for our data type
  min.clear();
  max.clear();
  const float max_sample = static_cast<float>( (1U << bpc) - 1U );
  for( unsigned int i=0; i<channels; i++ ){
    min.push_back( 0.0 );
    max.push_back( max_sample );
  }

  // Indicate that our metadata has been read
  isSet = true;

#ifdef GROK_DEBUG
  logfile << "Grok :: loadImageInfo() :: " << timer.getTime() << " microseconds" << endl;
#endif
}



// Helper to convert planar to interleaved
void GrokImage::planarToInterleaved( const grk_image* img, void* interleaved_data,
                                     unsigned int width, unsigned int height,
                                     unsigned int channels, unsigned int out_bpc,
                                     unsigned int factor)
{
  if( !img || !interleaved_data || channels == 0 || img->numcomps < channels ){
    throw file_error( "Grok :: planarToInterleaved() :: invalid decoded image" );
  }

  const unsigned int obpc = (out_bpc > 8 && out_bpc <= 16) ? 16 : 8;
  const uint32_t output_max = (obpc == 16) ? 65535U : 255U;

  size_t n = 0;
  for( unsigned int c = 0; c < channels; c++ ){
    const grk_image_comp& comp = img->comps[c];
    const uint64_t needed_width = width ? (uint64_t)(width - 1) * factor + 1 : 0;
    const uint64_t needed_height = height ? (uint64_t)(height - 1) * factor + 1 : 0;
    if( !comp.data || comp.prec == 0 || comp.prec > 16 ||
        comp.w < needed_width || comp.h < needed_height ){
      throw file_error( "Grok :: planarToInterleaved() :: decoded component dimensions do not match request" );
    }
  }

  for( unsigned int j = 0; j < height; j++ ){
    const unsigned int src_y = j * factor;
    for( unsigned int i = 0; i < width; i++ ){
      const unsigned int src_x = i * factor;
      for( unsigned int c = 0; c < channels; c++ ){
        const grk_image_comp& comp = img->comps[c];
        const size_t src_stride = comp.stride ? comp.stride : comp.w;
        const size_t index = static_cast<size_t>(src_y) * src_stride + src_x;
        int64_t sample = static_cast<const int32_t*>(comp.data)[index];
        const uint32_t source_max = (1U << comp.prec) - 1U;
        if( comp.sgnd ) sample += 1LL << (comp.prec - 1);
        sample = std::max<int64_t>( 0, std::min<int64_t>( source_max, sample ) );
        const uint32_t scaled = static_cast<uint32_t>(
          (static_cast<uint64_t>(sample) * output_max + source_max / 2U) / source_max
        );

        if( obpc == 16 ){
          static_cast<unsigned short*>(interleaved_data)[n++] =
            static_cast<unsigned short>( scaled );
        }
        else{
          static_cast<unsigned char*>(interleaved_data)[n++] =
            static_cast<unsigned char>( scaled );
        }
      }
    }
  }
}



// Get an individual tile
RawTile GrokImage::getTile( int seq, int ang, unsigned int res, int layers, unsigned int tile, ImageEncoding e )
{

  // Scale up our output bit depth to the nearest factor of 8
  unsigned obpc = bpc;
  if( bpc <= 16 && bpc > 8 ) obpc = 16;
  else if( bpc <= 8 ) obpc = 8;

#ifdef GROK_DEBUG
  Timer timer;
  timer.start();
#endif

  if( res >= numResolutions ){
    ostringstream tile_no;
    tile_no << "Grok :: Asked for non-existent resolution: " << res;
    throw file_error( tile_no.str() ); // FIXME: release allocated resources
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
    tile_no << "Grok :: Asked for non-existent tile: " << tile;
    throw file_error( tile_no.str() ); // FIXME: release allocated resources
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

#ifdef GROK_DEBUG
  logfile << "Grok :: Tile size: " << tw << "x" << th << " @" << channels << endl;
#endif

  // Grok supports 8 or 16 bit images
  if( !( (obpc == 8) || (obpc == 16) ) ) throw file_error( "Grok :: Unsupported number of bits" ); // FIXME: release allocated resources

  // Create our Rawtile object and initialize with data
  RawTile rawtile( tile, res, seq, ang, tw, th, channels, obpc );
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.allocate();

  // Process the tile
  process( res, layers, xoffset, yoffset, tw, th, rawtile.data );

#ifdef GROK_DEBUG
  logfile << "Grok :: getTile() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;
}



// Get an entire region and not just a tile
RawTile GrokImage::getRegion( int ha, int va, unsigned int res, int layers, int x, int y, unsigned int w, unsigned int h ){

  if( res >= numResolutions ){
    ostringstream region_no;
    region_no << "Grok :: Asked for non-existent resolution: " << res;
    throw file_error( region_no.str() );
  }

  // Scale up our output bit depth to the nearest factor of 8
  unsigned int obpc = bpc;
  if( bpc <= 16 && bpc > 8 ) obpc = 16;
  else if( bpc <= 8 ) obpc = 8;

#ifdef GROK_DEBUG
  Timer timer;
  timer.start();
#endif

  // Grok supports 8 or 16 bit images
  if( !( (obpc == 8) || (obpc == 16) ) ) throw file_error( "Grok :: Unsupported number of bits" ); // FIXME: release allocated resources

  RawTile rawtile( 0, res, ha, va, w, h, channels, obpc );
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.allocate();

  process( res, layers, x, y, w, h, rawtile.data );

#ifdef GROK_DEBUG
  logfile << "Grok :: getRegion() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;
}



// Main processing function
void GrokImage::process( unsigned int res, int layers, int xoffset, int yoffset, unsigned int tw, unsigned int th, void *d )
{
  std::lock_guard<std::mutex> lock(_decode_mutex);
  // Re-open if necessary
  if( !_codec ) openImage();

  const unsigned int output_width = tw;
  const unsigned int output_height = th;
  unsigned int factor = 1;
  int vipsres = getNativeResolution( res ); // Reverse resolution number

  // Calculate number of extra resolutions needed that have not been encoded in the image
  if( res < virtual_levels ){
    factor = 1U << (virtual_levels - res);
    xoffset *= factor;
    yoffset *= factor;
    tw *= factor;
    th *= factor;
    // Set our resolution level back to the smallest original resolution
    vipsres = numResolutions - 1 - virtual_levels;
#ifdef GROK_DEBUG
  logfile << "Grok :: using smallest existing resolution " << virtual_levels << endl;
#endif
  }

  // Set the number of layers to half of the number of detected layers if we have not set the
  // layers parameter manually. If layers is set to less than 0, use all layers.
  if( layers < 0 ) layers = quality_layers;
  else if( layers == 0 ) layers = ceil( quality_layers/2.0 );

  // Also make sure we have at least 1 layer
  if( layers < 1 ) layers = 1;

  // Update decompression parameters for this request
  _decompress_params.core.reduce = vipsres;
  _decompress_params.core.layers_to_decompress = layers;

  if( xoffset < 0 || yoffset < 0 || vipsres < 0 ||
      static_cast<size_t>(vipsres) >= image_widths.size() ){
    throw file_error( "Grok :: process() :: invalid decode coordinates" );
  }

  const uint64_t x0 = static_cast<unsigned int>( xoffset );
  const uint64_t y0 = static_cast<unsigned int>( yoffset );
  const uint64_t x1 = x0 + tw;
  const uint64_t y1 = y0 + th;
  const uint64_t level_width = image_widths[vipsres];
  const uint64_t level_height = image_heights[vipsres];
  if( x0 >= level_width || y0 >= level_height ||
      x1 > level_width || y1 > level_height ){
    throw file_error( "Grok :: process() :: requested region exceeds image bounds" );
  }

  // Grok 20.3.x accepts window coordinates directly in reduced output space.
  _decompress_params.dw_x0 = x0;
  _decompress_params.dw_y0 = y0;
  _decompress_params.dw_x1 = x1;
  _decompress_params.dw_y1 = y1;
  _decompress_params.dw_reduced = true;

#ifdef GROK_DEBUG
  logfile << "Grok :: decoding " << layers << " quality layers" << endl;
  logfile << "Grok :: requested region at requested resolution: position: "
          << xoffset << "x" << yoffset << ". size: " << tw << "x" << th << endl;
  logfile << "Grok :: region size at native reduced resolution: " << tw << "x" << th << endl;
#endif

  if( IIPImage::logging ){
    logfile << "Grok :: window x0=" << x0 << " y0=" << y0
            << " x1=" << x1 << " y1=" << y1
            << " (reduced image " << level_width << "x" << level_height << ")"
            << " vipsres=" << vipsres << endl;
  }

  // Update the codec with new parameters
  if( !grk_decompress_update( &_decompress_params, _codec ) ){
    closeImage();
    throw file_error( "Grok :: process() :: grk_decompress_update() failed" );
  }

  // Perform decoding - Grok will decode the specified region
  if( !grk_decompress( _codec, nullptr ) ){
    closeImage();
    throw file_error( "Grok :: process() :: grk_decompress() failed" );
  }

  // Wait for decompression to complete (synchronous mode)
  grk_decompress_wait( _codec, nullptr );

  // Get the decoded image
  _image = grk_decompress_get_image( _codec );
  if( !_image ){
    closeImage();
    throw file_error( "Grok :: process() :: grk_decompress_get_image() failed" );
  }

  // Convert planar to interleaved
  try{
    planarToInterleaved( _image, d, output_width, output_height, channels, bpc, factor );
  }
  catch( ... ){
    closeImage();
    throw;
  }

  // Extract any ICC profile - available in header
  if( _header.header_image.meta && _header.header_image.meta->color.icc_profile_len > 0 ){
    string icc( (const char*)_header.header_image.meta->color.icc_profile_buf,
                _header.header_image.meta->color.icc_profile_len );
    metadata.insert( {"icc", icc} );
#ifdef GROK_DEBUG
    logfile << "Grok :: ICC profile detected with size "
            << _header.header_image.meta->color.icc_profile_len << endl;
#endif
  }

  // We need to close the image here in case we try to use the Grok
  // stream or image structures multiple times in the same request pipeline
  // closeImage();
}
