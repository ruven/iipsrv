// IIPImage.cc 


/*  IIP fcgi server module

    Copyright (C) 2000-2009 Ruven Pillay.

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


#include "IIPImage.h"

#ifdef HAVE_GLOB_H
#include <glob.h>
#endif

#include <cstdio>
#include <sys/stat.h>
#include <sstream>


using namespace std;



IIPImage::IIPImage()
{
  isFile = false;
  bpp = 0;
  channels = 0;
  isSet = false;
  currentX = 0;
  currentY = 90;
  timestamp = 0;
}



IIPImage::IIPImage ( const string& p )
{
  isFile = false;
  bpp = 0;
  channels = 0;
  isSet = false;
  currentX = 0;
  currentY = 90;
  timestamp = 0;
  imagePath = p;
}



// Copy constructor
IIPImage::IIPImage( const IIPImage& image )
{
  imagePath = image.imagePath;
  isFile = image.isFile;
  type = image.type;
  fileSystemPrefix = image.fileSystemPrefix;
  fileNamePattern = image.fileNamePattern;
  horizontalAnglesList = image.horizontalAnglesList;
  verticalAnglesList = image.verticalAnglesList;
  image_widths = image.image_widths;
  image_heights = image.image_heights;
  bpp = image.bpp;
  channels = image.channels;
  isSet = image.isSet;
  currentX = image.currentX;
  currentY = image.currentY;
  metadata = image.metadata;
  timestamp = image.timestamp;
}



const IIPImage& IIPImage::operator = ( const IIPImage& image )
{
  imagePath = image.imagePath;
  isFile = image.isFile;
  type = image.type;
  fileSystemPrefix = image.fileSystemPrefix;
  fileNamePattern = image.fileNamePattern;
  horizontalAnglesList = image.horizontalAnglesList;
  verticalAnglesList = image.verticalAnglesList;
  image_widths = image.image_widths;
  image_heights = image.image_heights;
  bpp = image.bpp;
  channels = image.channels;
  isSet = image.isSet;
  currentX = image.currentX;
  currentY = image.currentY;
  metadata = image.metadata;
  timestamp = image.timestamp;
  return *this;
}  



void IIPImage::testImageType()
{
  // Check whether it is a regular file
  struct stat sb;

  string path = fileSystemPrefix + imagePath;

  if( (stat(path.c_str(),&sb)==0) && S_ISREG(sb.st_mode) ){
    isFile = true;
    int dot = imagePath.find_last_of( "." );
    type = imagePath.substr( dot + 1, imagePath.length() );
    timestamp = sb.st_mtime;
  }
  else{

#ifdef HAVE_GLOB_H

    // Check for sequence
    glob_t gdat;
    string filename = path + fileNamePattern + "000_090.*";

    if( glob( filename.c_str(), 0, NULL, &gdat ) != 0 ){
      globfree( &gdat );
      string message = path + string( " is neither a file or part of an image sequence" );
      throw message;
    }
    if( gdat.gl_pathc != 1 ){
      globfree( &gdat );
      string message = string( "There are multiple file extensions matching " )  + filename;
      throw message;
    }

    string tmp( gdat.gl_pathv[0] );
    globfree( &gdat );

    isFile = false;

    int dot = tmp.find_last_of( "." );
    int len = tmp.length();

    type = tmp.substr( dot + 1, len );

    updateTimestamp( tmp );

#else
    string message = path + string( " is not a file and no glob support enabled" );
    throw message;
#endif

  }

}


void IIPImage::updateTimestamp( const string& path )
{
  // Get a modification time for our image
  struct stat sb;
  stat( path.c_str(), &sb );
  timestamp = sb.st_mtime;
}


const std::string IIPImage::getTimestamp()
{
  tm *t;
  const time_t tm1 = timestamp;
  t = gmtime( &tm1 );
  char strt[128];
  strftime( strt, 128, "%a, %d %b %Y %H:%M:%S GMT", t );

  return string(strt);
}


void IIPImage::measureVerticalAngles()
{
  verticalAnglesList.clear();

#ifdef HAVE_GLOB_H

  glob_t gdat;
  unsigned int i;

  string filename = fileSystemPrefix + imagePath + fileNamePattern + "000_*." + type;
  
  if( glob( filename.c_str(), 0, NULL, &gdat ) != 0 ){
    globfree( &gdat );
  }  

  for( i=0; i < gdat.gl_pathc; i++ ){

    // Extract angle no from path name.
    int angle;
    string tmp( gdat.gl_pathv[i] );
    int len = tmp.length() - type.length() - 1;
    string sequence_no = tmp.substr( len-3, 3 );
    istringstream(sequence_no) >> angle;
    verticalAnglesList.push_front( angle );
  }

  verticalAnglesList.sort();

  globfree( &gdat );

#endif

}



void IIPImage::measureHorizontalAngles()
{
  horizontalAnglesList.clear();

#ifdef HAVE_GLOB_H

  glob_t gdat;
  unsigned int i;

  string filename = fileSystemPrefix + imagePath + fileNamePattern + "*_090." + type;

  if( glob( filename.c_str(), 0, NULL, &gdat ) != 0 ){
    globfree( &gdat );
  }

  for( i=0; i < gdat.gl_pathc; i++ ){

    // Extract angle no from path name.
    int angle;
    string tmp( gdat.gl_pathv[i] );
    int start = string(fileSystemPrefix + imagePath + fileNamePattern).length();
    int end = tmp.find_last_of("_");
    string n = tmp.substr( start, end-start );
    istringstream(n) >> angle;
    horizontalAnglesList.push_front( angle );
  }

  horizontalAnglesList.sort();

  globfree( &gdat );

#endif

}



void IIPImage::Initialise()
{
  testImageType();

  if( !isFile ){
    // Measure sequence angles
    measureHorizontalAngles();

    // Measure vertical view angles
    measureVerticalAngles();
  }
  // If it's a single value, give the view default angles of 0 and 90
  else{
    horizontalAnglesList.push_front( 0 );
    verticalAnglesList.push_front( 90 );
  }

}




const string IIPImage::getFileName( int seq, int ang )
{
  char name[1024];

  if( isFile ) return fileSystemPrefix+imagePath;
  else{
    // The angle or spectral band indices should be a minimum of 3 digits when padded
    snprintf( name, 1024,
	      "%s%s%03d_%03d.%s", (fileSystemPrefix+imagePath).c_str(), fileNamePattern.c_str(),
	      seq, ang, type.c_str() );
    
    string file( name );

    return file;
  }
}



int operator == ( const IIPImage& A, const IIPImage& B )
{
  if( A.imagePath == B.imagePath ) return( 1 );
  else return( 0 );
}



int operator != ( const IIPImage& A, const IIPImage& B )
{
  if( A.imagePath != B.imagePath ) return( 1 );
  else return( 0 );
}


