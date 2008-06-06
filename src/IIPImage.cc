// IIPImage.cc 


/*  IIP fcgi server module

    Copyright (C) 2000-2008 Ruven Pillay.

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
#include <fstream>


using namespace std;



IIPImage::IIPImage()
{
  isFile = false;
  bpp = 0;
  channels = 0;
  isSet = false;
  currentX = 0;
  currentY = 90;
}



IIPImage::IIPImage ( const string& p )
{
  imagePath = p;
  isFile = false;
  bpp = 0;
  channels = 0;
  isSet = false;
  currentX = 0;
  currentY = 90;
}



// Copy constructor
IIPImage::IIPImage( const IIPImage& image )
{
  imagePath = image.imagePath;
  isFile = image.isFile;
  type = image.type;
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
}



const IIPImage& IIPImage::operator = ( const IIPImage& image )
{
  imagePath = image.imagePath;
  isFile = image.isFile;
  type = image.type;
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
  return *this;
}  



void IIPImage::testImageType()
{

  ifstream file( imagePath.c_str() );

  if( file ){
    isFile = true;
  }
  file.close();
  

  if( isFile ){
    // ie is regular file
    int dot = imagePath.find_last_of( "." );
    type = imagePath.substr( dot + 1, imagePath.length() );
  }
  else{

#ifdef HAVE_GLOB_H

    // Check for sequence
    glob_t gdat;
    string filename = imagePath + fileNamePattern + "000_090.*";
      
    if( glob( filename.c_str(), 0, NULL, &gdat ) != 0 ){
      globfree( &gdat );
      string message = imagePath + fileNamePattern + string( " is neither a file or part of an image sequence" );
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

#else
    string message = imagePath + string( " is not a file and no glob support enabled" );
    throw message;
#endif

  }

}



void IIPImage::measureVerticalAngles()
{
  verticalAnglesList.clear();

#ifdef HAVE_GLOB_H

  glob_t gdat;
  unsigned int i;

  string filename = imagePath + fileNamePattern + "000_*." + type;
  
  if( glob( filename.c_str(), 0, NULL, &gdat ) != 0 ){
    globfree( &gdat );
  }  

  for( i=0; i < gdat.gl_pathc; i++ ){

    // Extract angle no from path name.
    int angle;
    string tmp( gdat.gl_pathv[i] );
    int len = tmp.length();
    string sequence_no = tmp.substr( len - 7, len - 4 );
    angle = atoi( sequence_no.c_str() );
    verticalAnglesList.push_front( angle );
  }

  verticalAnglesList.sort();
//   noVerticalImages = gdat.gl_pathc;

  globfree( &gdat );

#endif

}



void IIPImage::measureHorizontalAngles()
{
  horizontalAnglesList.clear();

#ifdef HAVE_GLOB_H

  glob_t gdat;
  unsigned int i;

  string filename = imagePath + fileNamePattern + "*_090." + type;
  
  if( glob( filename.c_str(), 0, NULL, &gdat ) != 0 ){
    globfree( &gdat );
  }  

  for( i=0; i < gdat.gl_pathc; i++ ){

    // Extract angle no from path name.
    int angle;
    string tmp( gdat.gl_pathv[i] );
    int len = tmp.length();
    string horizontalAnglesList_no = tmp.substr( len - 11, len - 9 );
    angle = atoi( (char*) horizontalAnglesList_no.c_str() );
    horizontalAnglesList.push_front( angle );
  }

  horizontalAnglesList.sort();
//   noSequenceImages = gdat.gl_pathc;
  
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



list <int> IIPImage::getVerticalViewsList()
{ 
  return verticalAnglesList;
}



list <int> IIPImage::getHorizontalViewsList()
{ 
  return horizontalAnglesList;
}



string IIPImage::getFileName( int seq, int ang )
{
  char name[1024];
  string suffix;

  if( isFile ) return imagePath;
  else{
    snprintf( name, 1024,
	      "%s%s%.3d_%.3d.%s", imagePath.c_str(), fileNamePattern.c_str(),
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


