/*
    IIP Command Handler Member Functions

    Copyright (C) 2006-2013 Ruven Pillay.

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


#include "Task.h"
#include "Tokenizer.h"
#include <iostream>
#include <algorithm>


using namespace std;



Task* Task::factory( const string& t ){

  // Convert the command string to lower case to handle incorrect
  // viewer implementations

  string type = t;
  transform( type.begin(), type.end(), type.begin(), ::tolower );

  if( type == "obj" ) return new OBJ;
  else if( type == "fif" ) return new FIF;
  else if( type == "qlt" ) return new QLT;
  else if( type == "sds" ) return new SDS;
  else if( type == "minmax" ) return new MINMAX;
  else if( type == "cnt" ) return new CNT;
  else if( type == "gam" ) return new GAM;
  else if( type == "wid" ) return new WID;
  else if( type == "hei" ) return new HEI;
  else if( type == "rgn" ) return new RGN;
  else if( type == "rot" ) return new ROT;
  else if( type == "til" ) return new TIL;
//  else if( type == "ptl" ) return new PTL;
  else if( type == "jtl" ) return new JTL;
  else if( type == "jtls" ) return new JTLS;
  else if( type == "icc" ) return new ICC;
  else if( type == "cvt" ) return new CVT;
  else if( type == "shd" ) return new SHD;
  else if( type == "cmp" ) return new CMP;
  else if( type == "inv" ) return new INV;
  else if( type == "zoomify" ) return new Zoomify;
  else if( type == "spectra" ) return new SPECTRA;
  else if( type == "pfl" ) return new PFL;
  else if( type == "lyr" ) return new LYR;
  else if( type == "deepzoom" ) return new DeepZoom;
  else if( type == "iiif" ) return new IIIF;
  else return NULL;

}


void Task::checkImage(){
  if( !*(session->image) ){
    session->response->setError( "1 3", argument );
    throw string( "image not set" );
  }
}



void QLT::run( Session* session, const std::string& argument ){

  if( argument.length() ){

    int factor = atoi( argument.c_str() );

    // Check the value is realistic
    if( factor < 0 || factor > 100 ){
      if( session->loglevel >= 2 ){
	*(session->logfile) << "QLT :: JPEG Quality factor of " << argument
			    << " out of bounds. Must be 0-100" << endl;
      }
    }

    session->jpeg->setQuality( factor );
  }

}


void SDS::run( Session* session, const std::string& argument ){

  if( session->loglevel >= 3 ) *(session->logfile) << "SDS handler reached" << endl;

  // Parse the argument list
  int delimitter = argument.find( "," );
  string tmp = argument.substr( 0, delimitter );
  session->view->xangle = atoi( tmp.c_str() );
  string arg2 = argument.substr( delimitter + 1, argument.length() );

  delimitter = arg2.find( "," );
  tmp = arg2.substr( 0, delimitter );
  session->view->yangle = atoi( tmp.c_str() );

  if( session->loglevel >= 2 ) *(session->logfile) << "SDS :: set to " << session->view->xangle << ", "
						   << session->view->yangle << endl;

}


void MINMAX::run( Session* session, const std::string& argument ){

  if( session->loglevel >= 3 ) *(session->logfile) << "MINMAX handler reached" << endl;

  // Parse the argument list
  int delimitter = argument.find( "," );
  string tmp = argument.substr( 0, delimitter );
  int nchan = atoi( tmp.c_str() ) - 1;
  string arg2 = argument.substr( delimitter + 1, argument.length() );

  delimitter = arg2.find( "," );
  tmp = arg2.substr( 0, delimitter );
  (*(session->image))->min[nchan] = atof( tmp.c_str() );
  string arg3 = arg2.substr( delimitter + 1, arg2.length() );

  delimitter = arg3.find( "," );
  tmp = arg3.substr( 0, delimitter );
  (*(session->image))->max[nchan] = atof( tmp.c_str() );

  if( session->loglevel >= 2 ) *(session->logfile) << "MINMAX :: set to " << (*(session->image))->min[nchan] << ", "
						   << (*(session->image))->max[nchan] << " for channel " << nchan << endl;
}


void CNT::run( Session* session, const std::string& argument ){

  float contrast = (float) atof( argument.c_str() );

  if( session->loglevel >= 2 ) *(session->logfile) << "CNT handler reached" << endl;
  if( session->loglevel >= 3 ) *(session->logfile) << "CNT :: requested contrast adjustment is " << contrast << endl;

  session->view->setContrast( contrast );
}


void GAM::run( Session* session, const std::string& argument ){

  float gamma = (float) atof( argument.c_str() );

  if( session->loglevel >= 2 ) *(session->logfile) << "GAM handler reached" << endl;
  if( session->loglevel >= 3 ) *(session->logfile) << "GAM :: requested gamma adjustment is " << gamma << endl;

  session->view->setGamma( gamma );
}


void WID::run( Session* session, const std::string& argument ){

  int requested_width = atoi( argument.c_str() );

  if( session->loglevel >= 2 ) *(session->logfile) << "WID handler reached" << endl;
  if( session->loglevel >= 3 ) *(session->logfile) << "WID :: requested width is " << requested_width << endl;

  session->view->setRequestWidth( requested_width );

}


void HEI::run( Session* session, const std::string& argument ){

  int requested_height = atoi( argument.c_str() );

  if( session->loglevel >= 2 ) *(session->logfile) << "HEI handler reached" << endl;
  if( session->loglevel >= 3 ) *(session->logfile) << "HEI :: requested height is " << requested_height << endl;

  session->view->setRequestHeight( requested_height );

}


void RGN::run( Session* session, const std::string& argument ){

  Tokenizer izer( argument, "," );
  int i = 0;

  if( session->loglevel >= 2 ) *(session->logfile) << "RGN handler reached" << endl;

  float region[4];
  while( izer.hasMoreTokens() && i<4 ){
    try{
      region[i++] = atof( izer.nextToken().c_str() );
    }
    catch( const string& error ){
      if( session->loglevel >= 1 ) *(session->logfile) << error << endl;
    }
  }

  // Only load this information if our argument was correctly parsed to
  // give 4 values
  if( i == 4 ){
    session->view->setViewLeft( region[0] );
    session->view->setViewTop( region[1] );
    session->view->setViewWidth( region[2] );
    session->view->setViewHeight( region[3] );
  }

  if( session->loglevel >= 3 ){
    *(session->logfile) << "RGN :: requested region is " << region[0] << ", "
			<< region[1] << ", " << region[2] << ", " << region[3] << endl;
  }

}


void ROT::run( Session* session, const std::string& argument ){

  float rotation = (float) atof( argument.c_str() );

  if( session->loglevel >= 2 ) *(session->logfile) << "ROT handler reached" << endl;
  if( session->loglevel >= 3 ) *(session->logfile) << "ROT :: requested rotation is " << rotation << " degrees" << endl;

  session->view->setRotation( rotation );
}


void JTLS::run( Session* session, const std::string& argument ){

  /* The argument is comma separated into 4:
     1) xangle
     2) resolution
     3) tile number
     4) yangle
     This is a legacy command. Clients should use SDS to specity the x,y angle and JTL
     for the resolution and tile number.
  */

  Tokenizer izer( argument, "," );
  int i = 0;

  if( session->loglevel >= 2 ) *(session->logfile) << "JTLS handler reached" << endl;

  int values[4];
  while( izer.hasMoreTokens() && i<4 ){
    try{
      values[i++] = atoi( izer.nextToken().c_str() );
    }
    catch( const string& error ){
      if( session->loglevel >= 1 ) *(session->logfile) << error << endl;
    }
  }

  if( i == 4 ){
    session->view->xangle = values[0];
    session->view->yangle = values[3];
    char tmp[128];
    snprintf( tmp, 56, "%d,%d", values[1], values[2] );
    string str = tmp;
    JTL jtl;
    jtl.run( session, str );
  }


}


void SHD::run( Session* session, const std::string& argument ){

  /* The argument is comma separated into the 3D angles of incidence of the
     light source in degrees for the angle in the horizontal plane from 12 o'clock
     and downwards in the vertical plane, where 0 represents a source pointing
     horizontally
  */

  Tokenizer izer( argument, "," );
  int i = 0;

  if( session->loglevel >= 2 ) *(session->logfile) << "SHD handler reached" << endl;

  int values[2];
  while( izer.hasMoreTokens() && i<2 ){
    try{
      values[i++] = atoi( izer.nextToken().c_str() );
    }
    catch( const string& error ){
      if( session->loglevel >= 1 ) *(session->logfile) << error << endl;
    }
  }

  if( i == 2 ){
    session->view->shaded = true;
    session->view->shade[0] = values[0];
    session->view->shade[1] = values[1];
  }

  if( session->loglevel >= 3 ) *(session->logfile) << "SHD :: requested shade incidence angle is "
						   << values[0] << "," << values[1]  << endl;
}

void CMP::run( Session* session, const std::string& argument ){

  /* The argument is the colormap type: available colormaps are
     HOT, COLD, JET, BLUE, GREEN, RED
  */

  string ctype = argument.c_str();
  transform( ctype.begin(), ctype.end(), ctype.begin(), ::tolower );

  if( session->loglevel >= 2 ) *(session->logfile) << "CMP handler reached" << endl;
  if( session->loglevel >= 3 ) *(session->logfile) << "CMP :: requested colormap is " << ctype << endl;
  session->view->cmapped = true;

  if (ctype=="hot") session->view->cmap = HOT;
  else if (ctype=="cold") session->view->cmap = COLD;
  else if (ctype=="jet") session->view->cmap = JET;
  else if (ctype=="blue") session->view->cmap = BLUE;
  else if (ctype=="green") session->view->cmap = GREEN;
  else if (ctype=="red") session->view->cmap = RED;
  else session->view->cmapped = false;
}

void INV::run( Session* session, const std::string& argument ){

  if( session->loglevel >= 2 ) *(session->logfile) << "INV handler reached" << endl;
  session->view->inverted = true;
}

void LYR::run( Session* session, const std::string& argument ){

  if( argument.length() ){

    int layer = atoi( argument.c_str() );

    if( session->loglevel >= 2 ) *(session->logfile) << "LYR handler reached" << endl;
    if( session->loglevel >= 3 ) *(session->logfile) << "LYR :: requested layer is " << layer << endl;


    // Check the value is realistic
    if( layer < 1 || layer > 256 ){
      if( session->loglevel >= 2 ){
        *(session->logfile) << "LYR :: Number of quality layers " << argument
                            << " out of bounds. Must be 1-256" << endl;
      }
    }

    session->view->setLayers( layer );
  }

}

