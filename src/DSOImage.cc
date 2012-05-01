
/*  DSO module loader for external image types

    Copyright (C) 2000-2012 Ruven Pillay.

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


#ifdef ENABLE_DL

#include "DSOImage.h"

#include <dlfcn.h>


using namespace std;
 
    
// Typedefs for typecasting our loaded module functions
//  - there's probably a more elegant way to do this :/

typedef char* (*tile_func) (int, int, int*, int*, int*);
typedef void* (*func) (void*);






void DSOImage::Load( const string& s ) throw (string)
{
  modulePath = s;

  char* error;
  libHandle = NULL;

  loadLibrary();


  // Check for the file extension it handles

  void *getParameter;
  getParameter = dlsym( libHandle, "get_file_extension" );  
  error = dlerror();
  if( error != 0 ){
    throw string( error );
  }

  // This doesn't seem to work! So use typedef casting instead :o
//   char *ext = static_cast <char *()> (getParameter) ();

  func f = (func) getParameter;

  type = (char*) (*f) (NULL);


  // Check for the description

  getParameter = dlsym( libHandle, "get_description" );
  error = dlerror();
  if( error != 0 ){
    throw string( error );
  }
  f = (func) getParameter;
  description = (char*) (*f) (NULL);


  unloadLibrary();
}




// Unload the library on destruction

DSOImage::~DSOImage()
{
  closeImage();
  unloadLibrary();
}




void DSOImage::loadLibrary() throw (string)
{
  libHandle = dlopen( modulePath.c_str(), RTLD_NOW );

  if( libHandle == NULL ){
    char* error = dlerror();
    if( error != NULL ) throw string( error );
    else throw string( "Error in loading module " + modulePath );
  } 
}




void DSOImage::unloadLibrary() throw (string)
{
  if( libHandle ){
    dlclose( libHandle );
    char* error = dlerror();
    if( error != NULL ) throw string( error );
    libHandle = NULL;
  }
}




// Get the error string from the module
string DSOImage::getError()
{
  void *getParameter;
  getParameter = dlsym( libHandle, "get_error" );
  char* error = dlerror();
  if( error != NULL ){
    return string( error );
  }
  func g = (func) getParameter;
  char *result = (char*) (*g) (NULL);
  return string( result );
}




// Open the image specified by path and get the image and tile
//  widths and heights

void DSOImage::openImage() throw (string)
{
  loadLibrary();

  string path = getFileName( currentX, 90 );

  void *getParameter;
  getParameter = dlsym( libHandle, "open_image" );
  char* error = dlerror();
  if( error != NULL ){
    throw string( error );
  }
  func f = (func) getParameter;
  int result = (int) (*f) ( (void*) path.c_str() );
  if( result ) throw getError();


  getParameter = dlsym( libHandle, "get_tile_width" );
  error = dlerror();
  if( error != NULL ){
    throw string( error );
  }
  f = (func) getParameter;
  tile_width = (int) (*f) (NULL);


  getParameter = dlsym( libHandle, "get_tile_width" );
  error = dlerror();
  if( error != NULL ){
    throw string( error );
  }
  f = (func) getParameter;
  tile_height = (int) (*f) (NULL);


  getParameter = dlsym( libHandle, "get_image_width" );
  error = dlerror();
  if( error != NULL ){
    throw string( error );
  }
  f = (func) getParameter;
  image_widths.push_back( (int) (*f) (NULL) );


  getParameter = dlsym( libHandle, "get_image_height" );  
  error = dlerror();
  if( error != NULL ){
    throw string( error );
  }
  f = (func) getParameter;
  image_heights.push_back( (int) (*f) (NULL) );


  getParameter = dlsym( libHandle, "get_num_resolutions" );  
  error = dlerror();
  if( error != NULL ){
    throw string( error );
  }
  f = (func) getParameter;
  numResolutions = (int) (*f) (NULL);


}





void DSOImage::closeImage() throw (string)
{
  if( libHandle ){
    
    void *getParameter;
    getParameter = dlsym( libHandle, "close_image" );
    
    char *error = dlerror();
    if( error != 0 ){
      throw string( error );
    }
    
    func f = (func) getParameter;
    int result = (int) (*f) (NULL);
    if( result ) throw getError();
  }
}





RawTile DSOImage::getTile( int seq, int angle, unsigned int resolution, int layer, unsigned int tile ) throw (string)
{
  // Make sure we are on the correct image
  if( (currentX != seq) && (currentY != angle) ){

    closeImage();
    
    // Get the right file name
    string path = getFileName( seq, angle );
    
    // Open the image again
    void *getParameter;
    getParameter = dlsym( libHandle, "open_image" );
    char* error = dlerror();
    if( error != NULL ){
      throw string( error );
    }
    func f = (func) getParameter;
    int result = (int) (*f) ( (void*) path.c_str() );
    if( result ) throw getError();
  }

    
  void *getParameter;
  getParameter = dlsym( libHandle, "get_tile" );
  
  char *error = dlerror();
  if( error != 0 ){
    throw string( error );
  }

  tile_func f = (tile_func) getParameter;
  int data_len, w, h;
  unsigned char *data = (unsigned char*) (*f) ( tile, resolution, &w, &h, &data_len );
  
  if( !data ) throw getError();

  RawTile rawtile( tile, resolution, seq, angle,
		   w, h, 3, 8 );
  rawtile.data = data;
  rawtile.dataLength = data_len;
  return rawtile;
}  


#endif
