/*
    IIP Environment Variable Class

    Copyright (C) 2006 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#define MAX_CVT 5000

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


  static int getMaxCVT(){
    char* envpara = getenv( "MAX_CVT" );
    int max_CVT;
    if( envpara ){
      max_CVT = atoi( envpara );
      if( max_CVT < 64 ) max_CVT = 64;
      if( max_CVT == -1 ) max_CVT = -1;
    }
    else max_CVT = MAX_CVT;

    return max_CVT;
  }


};


#endif
