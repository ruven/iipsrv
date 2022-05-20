/*
    IIP Environment Variable Class

    Copyright (C) 2006-2021 Ruven Pillay.

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

#ifndef _ENVIRONMENT_H
#define _ENVIRONMENT_H


/* Define some default values
 */
#define VERBOSITY 1
#define LOGFILE "/tmp/iipsrv.log"
#define MAX_IMAGE_CACHE_SIZE 10.0
#define FILENAME_PATTERN "_pyr_"
#define JPEG_QUALITY 75
#define PNG_QUALITY 1
#define WEBP_QUALITY 50
#define MAX_CVT 5000
#define MAX_LAYERS 0
#define FILESYSTEM_PREFIX ""
#define FILESYSTEM_SUFFIX ""
#define WATERMARK ""
#define WATERMARK_PROBABILITY 1.0
#define WATERMARK_OPACITY 1.0
#define LIBMEMCACHED_SERVERS "localhost"
#define LIBMEMCACHED_TIMEOUT 86400  // 24 hours
#define INTERPOLATION 1  // 1: Bilinear
#define CORS "";
#define BASE_URL "";
#define CACHE_CONTROL "max-age=86400"; // 24 hours
#define ALLOW_UPSCALING true
#define URI_MAP ""
#define EMBED_ICC true
#define KAKADU_READMODE 0
#define IIIF_VERSION 2


#include <string>


/// Class to obtain environment variables
class Environment {


 public:

  static int getVerbosity(){
    int loglevel = VERBOSITY;
    char *envpara = getenv( "VERBOSITY" );
    if( envpara ){
      loglevel = atoi( envpara );
      // If not a realistic level, set to zero
      if( loglevel < 0 ) loglevel = 0;
    }
    return loglevel;
  }


  static std::string getLogFile(){
    char* envpara = getenv( "LOGFILE" );
    if( envpara ) return std::string( envpara );
    else return LOGFILE;
  }


  static float getMaxImageCacheSize(){
    float max_image_cache_size = MAX_IMAGE_CACHE_SIZE;
    char* envpara = getenv( "MAX_IMAGE_CACHE_SIZE" );
    if( envpara ){
      max_image_cache_size = atof( envpara );
    }
    return max_image_cache_size;
  }


  static std::string getFileNamePattern(){
    char* envpara = getenv( "FILENAME_PATTERN" );
    std::string filename_pattern;
    if( envpara ){
      filename_pattern = std::string( envpara );
    }
    else filename_pattern = FILENAME_PATTERN;

    return filename_pattern;
  }


  static int getJPEGQuality(){
    char* envpara = getenv( "JPEG_QUALITY" );
    int jpeg_quality;
    if( envpara ){
      jpeg_quality = atoi( envpara );
      if( jpeg_quality > 100 ) jpeg_quality = 100;
      if( jpeg_quality < 1 ) jpeg_quality = 1;
    }
    else jpeg_quality = JPEG_QUALITY;

    return jpeg_quality;
  }


  static int getPNGQuality(){
    char* envpara = getenv( "PNG_QUALITY" );
    int quality;
    if( envpara ){
      quality = atoi( envpara );
      if( quality > 9 ) quality = 9;
      if( quality < 0 ) quality = 0;
    }
    else quality = PNG_QUALITY;

    return quality;
  }


  static int getWebPQuality(){
    char* envpara = getenv( "WEBP_QUALITY" );
    int quality;
    if( envpara ){
      quality = atoi( envpara );
      if( quality > 100 ) quality = 900;
      if( quality < 0 ) quality = 0;
    }
    else quality = WEBP_QUALITY;

    return quality;
  }


  static int getMaxCVT(){
    char* envpara = getenv( "MAX_CVT" );
    int max_CVT;
    if( envpara ){
      max_CVT = atoi( envpara );
      // -1 indicates no maximum. Otherwise minimum is 1
      if( max_CVT < -1 ) max_CVT = 1;
      // If zero, use default
      else if( max_CVT == 0 ) max_CVT = MAX_CVT;
    }
    else max_CVT = MAX_CVT;

    return max_CVT;
  }


  static int getMaxLayers(){
    char* envpara = getenv( "MAX_LAYERS" );
    int layers;
    if( envpara ) layers = atoi( envpara );
    else layers = MAX_LAYERS;

    return layers;
  }


  static std::string getFileSystemPrefix(){
    char* envpara = getenv( "FILESYSTEM_PREFIX" );
    std::string filesystem_prefix;
    if( envpara ){
      filesystem_prefix = std::string( envpara );
    }
    else filesystem_prefix = FILESYSTEM_PREFIX;

    return filesystem_prefix;
  }


  static std::string getFileSystemSuffix(){
    char* envpara = getenv( "FILESYSTEM_SUFFIX" );
    std::string filesystem_suffix;
    if( envpara ){
      filesystem_suffix = std::string( envpara );
    }
    else filesystem_suffix = FILESYSTEM_SUFFIX;

    return filesystem_suffix;
  }


  static std::string getWatermark(){
    char* envpara = getenv( "WATERMARK" );
    std::string watermark;
    if( envpara ){
      watermark = std::string( envpara );
    }
    else watermark = WATERMARK;

    return watermark;
  }


  static float getWatermarkProbability(){
    float watermark_probability = WATERMARK_PROBABILITY;
    char* envpara = getenv( "WATERMARK_PROBABILITY" );

    if( envpara ){
      watermark_probability = atof( envpara );
      if( watermark_probability > 1.0 ) watermark_probability = 1.0;
      if( watermark_probability < 0 ) watermark_probability = 0.0;
    }

    return watermark_probability;
  }


  static float getWatermarkOpacity(){
    float watermark_opacity = WATERMARK_OPACITY;
    char* envpara = getenv( "WATERMARK_OPACITY" );

    if( envpara ){
      watermark_opacity = atof( envpara );
      if( watermark_opacity > 1.0 ) watermark_opacity = 1.0;
      if( watermark_opacity < 0 ) watermark_opacity = 0.0;
    }

    return watermark_opacity;
  }


  static std::string getMemcachedServers(){
    char* envpara = getenv( "MEMCACHED_SERVERS" );
    std::string memcached_servers;
    if( envpara ){
      memcached_servers = std::string( envpara );
    }
    else memcached_servers = LIBMEMCACHED_SERVERS;

    return memcached_servers;
  }


  static unsigned int getMemcachedTimeout(){
    char* envpara = getenv( "MEMCACHED_TIMEOUT" );
    unsigned int memcached_timeout;
    if( envpara ) memcached_timeout = atoi( envpara );
    else memcached_timeout = LIBMEMCACHED_TIMEOUT;

    return memcached_timeout;
  }


  static unsigned int getInterpolation(){
    char* envpara = getenv( "INTERPOLATION" );
    unsigned int interpolation;
    if( envpara ) interpolation = atoi( envpara );
    else interpolation = INTERPOLATION;

    return interpolation;
  }


  static std::string getCORS(){
    char* envpara = getenv( "CORS" );
    std::string cors;
    if( envpara ) cors = std::string( envpara );
    else cors = CORS;
    return cors;
  }


  static std::string getBaseURL(){
    char* envpara = getenv( "BASE_URL" );
    std::string base_url;
    if( envpara ) base_url = std::string( envpara );
    else base_url = BASE_URL;
    return base_url;
  }


  static std::string getCacheControl(){
    char* envpara = getenv( "CACHE_CONTROL" );
    std::string cache_control;
    if( envpara ) cache_control = std::string( envpara );
    else cache_control = CACHE_CONTROL;
    return cache_control;
  }


  static bool getAllowUpscaling(){
    char* envpara = getenv( "ALLOW_UPSCALING" );
    bool allow_upscaling;
    if( envpara ) allow_upscaling =  atoi( envpara ); // Implicit cast to boolean, all values other than '0' treated as true
    else allow_upscaling = ALLOW_UPSCALING;
    return allow_upscaling;
  }


  static std::string getURIMap(){
    char* envpara = getenv( "URI_MAP" );
    std::string uri_map;
    if( envpara ) uri_map = std::string( envpara );
    else uri_map = URI_MAP;
    return uri_map;
  }


  static unsigned int getEmbedICC(){
    char* envpara = getenv( "EMBED_ICC" );
    bool embed;
    if( envpara ) embed = atoi( envpara );
    else embed = EMBED_ICC;
    return embed;
  }


  static unsigned int getKduReadMode(){
    unsigned int readmode;
    char* envpara = getenv( "KAKADU_READMODE" );
    if( envpara ){
      readmode = atoi( envpara );
      if( readmode > 2 ) readmode = 2;
    }
    else readmode = KAKADU_READMODE;
    return readmode;
  }


  static unsigned int getIIIFVersion(){
    unsigned int version;
    char* envpara = getenv( "IIIF_VERSION" );
    if( envpara ){
      version = atoi( envpara );
      if( version < 1 ) version = IIIF_VERSION;
    }
    else version = IIIF_VERSION;
    return version;
  }


};


#endif
