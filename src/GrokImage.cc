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
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>
#ifdef GROK_DEBUG
#include "Timer.h"
#endif

using namespace std;


// Reference our logging object
extern Logger logfile; // NOSONAR: iipsrv owns one shared mutable logger.

namespace {

using MutexLock = std::unique_lock<std::mutex>;

// Grok's public C callback ABI requires an opaque client-data parameter.
void errorCallback( const char* message, void* client_data ) // NOSONAR
{
  auto* logger = static_cast<Logger*>( client_data );
  if( IIPImage::logging && logger ) *logger << "Grok error :: " << message << endl;
}

#ifdef GROK_DEBUG
void warningCallback( const char* message, void* client_data ) // NOSONAR
{
  auto* logger = static_cast<Logger*>( client_data );
  if( IIPImage::logging && logger ) *logger << "Grok warning :: " << message << endl;
}

void infoCallback( const char* message, void* client_data ) // NOSONAR
{
  auto* logger = static_cast<Logger*>( client_data );
  if( IIPImage::logging && logger ) *logger << "Grok info :: " << message;
}
#endif

std::once_flag& grokInitFlag()
{
  static std::once_flag flag;
  return flag;
}

uint32_t configuredThreadCount()
{
  constexpr uint32_t default_threads = 2;
  const char* setting = std::getenv( "GROK_THREADS" );
  if( !setting || !*setting ) return default_threads;

  char* end = nullptr;
  const auto parsed = std::strtoul( setting, &end, 10 );
  if( end == setting || *end != '\0' ||
      parsed > std::numeric_limits<uint32_t>::max() ){
    return default_threads;
  }
  return static_cast<uint32_t>( parsed );
}

void deinitializeGrok()
{
  grk_deinitialize();
}

void initializeGrok()
{
  grk_msg_handlers msg_handlers = {};
  msg_handlers.error_callback = errorCallback;
  msg_handlers.error_data = &logfile;
#ifdef GROK_DEBUG
  msg_handlers.info_callback = infoCallback;
  msg_handlers.info_data = &logfile;
  msg_handlers.warn_callback = warningCallback;
  msg_handlers.warn_data = &logfile;
#endif
  grk_set_msg_handlers( msg_handlers );

  // Initialize the process-wide Grok runtime.  GROK_THREADS=0 uses all CPUs.
  const auto thread_count = configuredThreadCount();
  grk_initialize( nullptr, thread_count, nullptr );
  if( IIPImage::logging ){
    logfile << "Grok initialized using "
            << (thread_count == 0 ? "all" : std::to_string(thread_count))
            << " threads." << endl;
  }

  // Release the process-wide worker pool on a normal process exit.
  std::atexit( deinitializeGrok );
}

unsigned int ceilHalf( unsigned int value )
{
  return value / 2U + value % 2U;
}

uint32_t scaleSample( const grk_image_comp& component, size_t index,
                      uint32_t output_max )
{
  auto sample = static_cast<int64_t>(
    static_cast<const int32_t*>( component.data )[index]
  );
  const auto source_max = (1U << component.prec) - 1U;
  if( component.sgnd ) sample += 1LL << (component.prec - 1);

  const auto clamped = std::max<int64_t>(
    0, std::min<int64_t>( source_max, sample )
  );
  return static_cast<uint32_t>(
    (static_cast<uint64_t>(clamped) * output_max + source_max / 2U) /
    source_max
  );
}

void validateDecodedImage( const grk_image& image, unsigned int width,
                           unsigned int height, unsigned int channel_count,
                           unsigned int factor )
{
  if( !image.comps || channel_count == 0 || image.numcomps < channel_count ){
    throw file_error( "Grok :: invalid decoded image components" );
  }

  const uint64_t required_width =
    width == 0 ? 0 : static_cast<uint64_t>(width - 1) * factor + 1;
  const uint64_t required_height =
    height == 0 ? 0 : static_cast<uint64_t>(height - 1) * factor + 1;

  for( unsigned int channel = 0; channel < channel_count; ++channel ){
    const auto& component = image.comps[channel];
    if( !component.data || component.prec == 0 || component.prec > 16 ||
        component.w < required_width || component.h < required_height ){
      throw file_error( "Grok :: decoded component dimensions do not match request" );
    }
  }
}

template<typename OutputSample>
void interleaveDecodedImage( const grk_image& image, OutputSample* output,
                             unsigned int width, unsigned int height,
                             unsigned int channel_count, unsigned int factor )
{
  const auto output_max = static_cast<uint32_t>(
    std::numeric_limits<OutputSample>::max()
  );
  size_t output_index = 0;

  for( unsigned int output_y = 0; output_y < height; ++output_y ){
    const auto source_y = output_y * factor;
    for( unsigned int output_x = 0; output_x < width; ++output_x ){
      const auto source_x = output_x * factor;
      for( unsigned int channel = 0; channel < channel_count; ++channel ){
        const auto& component = image.comps[channel];
        const auto source_stride = component.stride ? component.stride : component.w;
        const auto source_index =
          static_cast<size_t>(source_y) * source_stride + source_x;
        output[output_index++] = static_cast<OutputSample>(
          scaleSample( component, source_index, output_max )
        );
      }
    }
  }
}

void copyDecodedImage( const grk_image& image, RawTile& output,
                       unsigned int width, unsigned int height,
                       unsigned int channel_count, unsigned int factor )
{
  if( !output.data ){
    throw file_error( "Grok :: decoded output buffer is null" );
  }
  validateDecodedImage( image, width, height, channel_count, factor );

  if( output.bpc == 16 ){
    interleaveDecodedImage(
      image, static_cast<uint16_t*>(output.data), width, height,
      channel_count, factor
    );
    return;
  }
  if( output.bpc == 8 ){
    interleaveDecodedImage(
      image, static_cast<uint8_t*>(output.data), width, height,
      channel_count, factor
    );
    return;
  }
  throw file_error( "Grok :: unsupported output precision" );
}

} // namespace

constexpr unsigned int GrokImage::DEFAULT_TILE_SIZE;

void GrokImage::CodecDeleter::operator()( grk_object* codec ) const noexcept
{
  grk_object_unref( codec );
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

void GrokImage::initTileDimensions()
{
  if( tile_widths.empty() ) tile_widths.push_back( DEFAULT_TILE_SIZE );
  if( tile_heights.empty() ) tile_heights.push_back( DEFAULT_TILE_SIZE );
}

// ---------------------------------------------------------------------------
// openImage
// ---------------------------------------------------------------------------

void GrokImage::openImage()
{
  string filename = getFileName( currentX, currentY );

  // Update our timestamp
  updateTimestamp( filename );

  std::call_once( grokInitFlag(), initializeGrok );

  // Setup stream parameters for file
  _stream_params = {};
  if( filename.size() >= GRK_PATH_LEN ){
    throw file_error( "Grok :: openImage() :: file path exceeds GRK_PATH_LEN: " + filename );
  }
  std::copy( filename.begin(), filename.end(), _stream_params.file );
  _stream_params.file[filename.size()] = '\0';
  _stream_params.is_read_stream = true;
  _stream_params.use_stdio = false; // Use memory mapping for better performance

#ifdef GROK_DEBUG
  Timer timer;
  timer.start();
#endif

  // Create decompression codec using grk_decompress_init
  _codec.reset( grk_decompress_init( &_stream_params, &_decompress_params ) );
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

  if( !grk_decompress_read_header( _codec.get(), &_header ) ){
    closeImage();
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

  _codec.reset();
  _image = nullptr; // _image is owned by the codec – do NOT unref it separately.
  _header_read = false;

#ifdef GROK_DEBUG
  logfile << "Grok :: closeImage() :: " << timer.getTime() << " microseconds" << endl;
#endif
}


void GrokImage::populateResolutionLevels( unsigned int width, unsigned int height )
{
  if( width == 0 || height == 0 || numResolutions == 0 ){
    throw file_error( "Grok :: invalid image dimensions or resolution levels" );
  }

  const auto native_level_count = numResolutions;
  image_widths.clear();
  image_heights.clear();
  image_widths.reserve( native_level_count );
  image_heights.reserve( native_level_count );
  image_widths.push_back( width );
  image_heights.push_back( height );

#ifdef GROK_DEBUG
  logfile << "Grok :: DWT Levels: " << native_level_count << endl;
  logfile << "Grok :: Resolution : " << width << "x" << height << endl;
#endif

  auto level_width = width;
  auto level_height = height;
  for( unsigned int level = 1; level < native_level_count; ++level ){
    level_width = ceilHalf( level_width );
    level_height = ceilHalf( level_height );
    image_widths.push_back( level_width );
    image_heights.push_back( level_height );
#ifdef GROK_DEBUG
    logfile << "Grok :: Resolution : " << level_width << "x"
            << level_height << endl;
#endif
  }

  unsigned int required_level_count = 1;
  level_width = width;
  level_height = height;
  while( level_width > tile_widths.front() ||
         level_height > tile_heights.front() ){
    level_width = ceilHalf( level_width );
    level_height = ceilHalf( level_height );
    ++required_level_count;
    if( required_level_count > native_level_count ){
      image_widths.push_back( level_width );
      image_heights.push_back( level_height );
    }
  }

  virtual_levels = required_level_count > native_level_count
    ? required_level_count - native_level_count
    : 0;
#ifdef GROK_DEBUG
  if( virtual_levels > 0 ){
    logfile << "Grok :: Warning! Insufficient resolution levels in JPEG2000 "
            << "stream. Will generate " << virtual_levels
            << " extra levels dynamically." << endl
            << "Grok :: Regenerate the file with at least "
            << required_level_count << " levels for best performance." << endl;
  }
#endif
  numResolutions = required_level_count;
}


unsigned int GrokImage::getOutputBitsPerChannel() const
{
  if( bpc == 0 || bpc > 16 ){
    throw file_error( "Grok :: unsupported number of bits" );
  }
  return bpc <= 8 ? 8 : 16;
}


GrokImage::TileGeometry GrokImage::getTileGeometry(
  unsigned int resolution, unsigned int tile
) const
{
  if( resolution >= numResolutions ){
    throw file_error(
      "Grok :: asked for non-existent resolution: " +
      std::to_string(resolution)
    );
  }

  const auto native_resolution = getNativeResolution( resolution );
  if( native_resolution < 0 ||
      static_cast<size_t>(native_resolution) >= image_widths.size() ){
    throw file_error( "Grok :: invalid native resolution" );
  }

  const auto base_width = tile_widths.front();
  const auto base_height = tile_heights.front();
  if( base_width == 0 || base_height == 0 ){
    throw file_error( "Grok :: invalid tile dimensions" );
  }

  const auto level_width = image_widths[native_resolution];
  const auto level_height = image_heights[native_resolution];
  const auto columns = (level_width + base_width - 1U) / base_width;
  const auto rows = (level_height + base_height - 1U) / base_height;
  const auto tile_count = static_cast<uint64_t>(columns) * rows;
  if( tile >= tile_count ){
    throw file_error(
      "Grok :: asked for non-existent tile: " + std::to_string(tile)
    );
  }

  const auto column = tile % columns;
  const auto row = tile / columns;
  const auto x = static_cast<uint64_t>(column) * base_width;
  const auto y = static_cast<uint64_t>(row) * base_height;
  if( x > static_cast<uint64_t>(std::numeric_limits<int>::max()) ||
      y > static_cast<uint64_t>(std::numeric_limits<int>::max()) ){
    throw file_error( "Grok :: tile offset exceeds supported coordinate range" );
  }

  return {
    std::min( base_width, level_width - static_cast<unsigned int>(x) ),
    std::min( base_height, level_height - static_cast<unsigned int>(y) ),
    static_cast<int>( x ),
    static_cast<int>( y )
  };
}



void GrokImage::loadImageInfo( int, int )
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

  const auto width = _header.header_image.x1 - _header.header_image.x0;
  const auto height = _header.header_image.y1 - _header.header_image.y0;
  populateResolutionLevels( width, height );

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
  const auto max_sample = static_cast<float>( (1U << bpc) - 1U );
  min.assign( channels, 0.0F );
  max.assign( channels, max_sample );

  // Indicate that our metadata has been read
  isSet = true;

#ifdef GROK_DEBUG
  logfile << "Grok :: loadImageInfo() :: " << timer.getTime() << " microseconds" << endl;
#endif
}
// Get an individual tile
RawTile GrokImage::getTile( int seq, int ang, unsigned int res, int layers, unsigned int tile, ImageEncoding e )
{
  (void)e;

#ifdef GROK_DEBUG
  Timer timer;
  timer.start();
#endif

  const auto geometry = getTileGeometry( res, tile );
  const auto output_bpc = getOutputBitsPerChannel();

#ifdef GROK_DEBUG
  logfile << "Grok :: Tile size: " << geometry.width << "x"
          << geometry.height << " @" << channels << endl;
#endif

  RawTile rawtile(
    tile, res, seq, ang, geometry.width, geometry.height, channels, output_bpc
  );
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.allocate();

  process(
    res, layers, geometry.x, geometry.y, geometry.width, geometry.height, rawtile
  );

#ifdef GROK_DEBUG
  logfile << "Grok :: getTile() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;
}



// Get an entire region and not just a tile
RawTile GrokImage::getRegion( int ha, int va, unsigned int res, int layers, int x, int y, unsigned int w, unsigned int h ){

  if( res >= numResolutions ){
    throw file_error(
      "Grok :: asked for non-existent resolution: " + std::to_string(res)
    );
  }

#ifdef GROK_DEBUG
  Timer timer;
  timer.start();
#endif

  RawTile rawtile(
    0, res, ha, va, w, h, channels, getOutputBitsPerChannel()
  );
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.allocate();

  process( res, layers, x, y, w, h, rawtile );

#ifdef GROK_DEBUG
  logfile << "Grok :: getRegion() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;
}



// Main processing function
void GrokImage::process( unsigned int res, int layers, int xoffset, int yoffset,
                         unsigned int tw, unsigned int th, RawTile& output )
{
  MutexLock decode_lock( _decode_mutex );
  if( !_codec ) openImage();
  if( xoffset < 0 || yoffset < 0 ){
    throw file_error( "Grok :: process() :: invalid decode coordinates" );
  }

  const unsigned int output_width = tw;
  const unsigned int output_height = th;
  unsigned int factor = 1;
  auto native_resolution = getNativeResolution( res );
  uint64_t decode_x = static_cast<unsigned int>( xoffset );
  uint64_t decode_y = static_cast<unsigned int>( yoffset );
  uint64_t decode_width = tw;
  uint64_t decode_height = th;

  if( res < virtual_levels ){
    const auto shift = virtual_levels - res;
    if( shift >= std::numeric_limits<unsigned int>::digits ){
      throw file_error( "Grok :: process() :: virtual resolution factor overflow" );
    }
    factor = 1U << shift;
    decode_x *= factor;
    decode_y *= factor;
    decode_width *= factor;
    decode_height *= factor;
    native_resolution = numResolutions - 1 - virtual_levels;
#ifdef GROK_DEBUG
    logfile << "Grok :: using smallest existing resolution "
            << virtual_levels << endl;
#endif
  }

  if( layers < 0 ) layers = quality_layers;
  else if( layers == 0 ) layers = (quality_layers + 1) / 2;
  if( layers < 1 ) layers = 1;

  if( native_resolution < 0 ||
      static_cast<size_t>(native_resolution) >= image_widths.size() ){
    throw file_error( "Grok :: process() :: invalid decode coordinates" );
  }

  const uint64_t decode_right = decode_x + decode_width;
  const uint64_t decode_bottom = decode_y + decode_height;
  const uint64_t level_width = image_widths[native_resolution];
  const uint64_t level_height = image_heights[native_resolution];
  if( decode_x >= level_width || decode_y >= level_height ||
      decode_right > level_width || decode_bottom > level_height ){
    throw file_error( "Grok :: process() :: requested region exceeds image bounds" );
  }

  _decompress_params.core.reduce = native_resolution;
  _decompress_params.core.layers_to_decompress = layers;
  _decompress_params.dw_x0 = decode_x;
  _decompress_params.dw_y0 = decode_y;
  _decompress_params.dw_x1 = decode_right;
  _decompress_params.dw_y1 = decode_bottom;
  _decompress_params.dw_reduced = true;

#ifdef GROK_DEBUG
  logfile << "Grok :: decoding " << layers << " quality layers" << endl;
  logfile << "Grok :: requested region at requested resolution: position: "
          << xoffset << "x" << yoffset << ". size: " << tw << "x" << th
          << endl;
  logfile << "Grok :: region size at native reduced resolution: "
          << decode_width << "x" << decode_height << endl;
#endif

  if( IIPImage::logging ){
    logfile << "Grok :: window x0=" << decode_x << " y0=" << decode_y
            << " x1=" << decode_right << " y1=" << decode_bottom
            << " (reduced image " << level_width << "x" << level_height << ")"
            << " native_resolution=" << native_resolution << endl;
  }

  if( !grk_decompress_update( &_decompress_params, _codec.get() ) ){
    closeImage();
    throw file_error( "Grok :: process() :: grk_decompress_update() failed" );
  }

  if( !grk_decompress( _codec.get(), nullptr ) ){
    closeImage();
    throw file_error( "Grok :: process() :: grk_decompress() failed" );
  }

  grk_decompress_wait( _codec.get(), nullptr );

  _image = grk_decompress_get_image( _codec.get() );
  if( !_image ){
    closeImage();
    throw file_error( "Grok :: process() :: grk_decompress_get_image() failed" );
  }

  try{
    copyDecodedImage(
      *_image, output, output_width, output_height, channels, factor
    );
  }
  catch( ... ){
    closeImage();
    throw;
  }

  // Extract any ICC profile - available in header
  if( _header.header_image.meta && _header.header_image.meta->color.icc_profile_len > 0 ){
    string icc(
      reinterpret_cast<const char*>(
        _header.header_image.meta->color.icc_profile_buf
      ),
      _header.header_image.meta->color.icc_profile_len
    );
    metadata.emplace( "icc", std::move(icc) );
#ifdef GROK_DEBUG
    logfile << "Grok :: ICC profile detected with size "
            << _header.header_image.meta->color.icc_profile_len << endl;
#endif
  }
}
