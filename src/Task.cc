/*
    IIP Command Handler Member Functions

    Copyright (C) 2006-2024 Ruven Pillay.

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
#include "URL.h"
#include <cstdlib>
#include <cmath>
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
#ifdef HAVE_PNG
  else if( type == "ptl" ) return new PTL;
#endif
#ifdef HAVE_WEBP
  else if( type == "wtl" ) return new WTL;
#endif
#ifdef HAVE_AVIF
  else if( type == "atl" ) return new ATL;
#endif
  else if( type == "jtl" ) return new JTL;
  else if( type == "ttl" ) return new TTL;
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
  else if( type == "ctw" ) return new CTW;
  else if( type == "col" ) return new COL;
  else if( type == "cnv" ) return new CNV;
  else if( type == "iiif" ) return new IIIF;
  else return NULL;

}


void Task::checkImage(){
  if( !*(session->image) ){
    session->response->setError( "1 3", argument );
    throw string( "image not set" );
  }
}



void QLT::run( Session* session, const string& argument ){

  if( argument.empty() ) return;

  string arg = argument;
  transform( arg.begin(), arg.end(), arg.begin(), ::tolower );

  // Check whether we have a compression:quality combination
  std::string::size_type delimitter = arg.find( ":" );
  if( delimitter != string::npos ){
    string comp = arg.substr( 0, delimitter );
    int                          compression = 0;
    if     ( comp == "lzw" )     compression = 1;
    else if( comp == "deflate" ) compression = 2;
    else if( comp == "jpeg" )    compression = 3;
    else if( comp == "webp" )    compression = 4;
    else if( comp == "zstd" )    compression = 5;

    session->tiff->setCompression( compression );
    if( session->loglevel >= 2 ){
      *(session->logfile) << "QLT :: Requested compression is " << TIFFCompressor::getCompressionName(compression) << endl;
    }

    // Strip the compression prefix from our argument
    arg = arg.substr( delimitter+1, -1 );
  }

  // Get the quality factor
  int factor = atoi( arg.c_str() );

  // Check the value is realistic
  if( factor < 0 || factor > 100 ){
    if( session->loglevel >= 2 ){
      *(session->logfile) << "QLT :: Warning: quality factor of " << argument
			  << " out of bounds. Must be 0-100 for JPEG and WebP and 1-9 for PNG" << endl;
    }
  }

  session->tiff->setQuality( factor );
  session->jpeg->setQuality( factor );

#ifdef HAVE_PNG
  session->png->setQuality( factor );
#endif
#ifdef HAVE_WEBP
  session->webp->setQuality( factor );
#endif
#ifdef HAVE_AVIF
  session->avif->setQuality( factor );
#endif

  if( session->loglevel >= 2 ) *(session->logfile) << "QLT :: Requested quality is " << factor << endl;

}


void SDS::run( Session* session, const string& argument ){

  if( session->loglevel >= 3 ) *(session->logfile) << "SDS handler reached" << endl;

  // Parse the argument - check whether we have a single value or 2 separated by a camma
  std::string::size_type delimitter = argument.find( "," );
  if( delimitter != string::npos ){
    string arg1 = argument.substr( 0, delimitter );
    session->view->xangle = atoi( arg1.c_str() );
    string arg2 = argument.substr( delimitter + 1, argument.length() );
    session->view->yangle = atoi( arg2.c_str() );
  }
  else{
    session->view->xangle = atoi( argument.c_str() );
    session->view->yangle = 0;
  }

  if( session->loglevel >= 2 ) *(session->logfile) << "SDS :: set to " << session->view->xangle << ", "
						   << session->view->yangle << endl;

}


/// Syntax: MINXMAX=<channel>:<min>,<max> where channel is an int from 0 to number of channels - 1
/// and min, max are integers or floats
void MINMAX::run( Session* session, const string& argument ){

  int channel = 0;
  float min, max;
  bool all = false;

  if( session->loglevel >= 3 ) *(session->logfile) << "MINMAX handler reached" << endl;

  // Parse the argument and extract our channel index first
  int delimitter = argument.find( ":" );
  string tmp = argument.substr( 0, delimitter );

  if( tmp == "-" ) all = true;
  else channel = atoi( tmp.c_str() );

  unsigned int nc = (*session->image)->getNumChannels();

  // Sanity check for channel index
  if( channel < 0 || channel >= (int)nc ){
    if( session->loglevel >= 1 ) *(session->logfile) << "MINMAX :: Error: channel number out of bounds: "
						     << tmp.c_str() << endl;
    return;
  }

  // Parse our min
  string arg2 = argument.substr( delimitter + 1, string::npos );
  delimitter = arg2.find( "," );
  min = atof( arg2.substr( 0, delimitter ).c_str() );

  // Parse our max
  string arg3 = arg2.substr( delimitter + 1, string::npos );
  max = atof( arg3.c_str() );

  // Indicate that we have a user-defined min/max
  session->view->minmax = true;

  if( all ){
    for( unsigned int n=0; n < nc; n++ ){
      (*(session->image))->min[n] = min;
      (*(session->image))->max[n] = max;
    }
  }
  else{
    (*(session->image))->min[channel] = min;
    (*(session->image))->max[channel] = max;
  }

  if( session->loglevel >= 2 ){
    *(session->logfile) << "MINMAX :: min and max input range set to " << min << "-" << max << " for ";
    if( all ) *(session->logfile) << "all channels" << endl;
    else *(session->logfile) << "channel " << channel << endl;
  }
}


void CNT::run( Session* session, const string& argument ){

  if( session->loglevel >= 2 ) *(session->logfile) << "CNT handler reached" << endl;

  // Request for histogram equalization
  string arg = argument;
  transform( arg.begin(), arg.end(), arg.begin(), ::tolower );
  if( arg == "eq" || arg == "equalization" ){
    session->view->equalization = true;
    if( session->loglevel >= 3 ) *(session->logfile) << "CNT :: histogram equalization requested" << endl;
  }
  // Linear stretch
  else if( arg == "st" || arg == "stretch" ){
    // Use reserved value of -1 for contrast stretch
    session->view->contrast = -1;
    if( session->loglevel >= 3 ) *(session->logfile) << "CNT :: contrast stretch requested" << endl;
  }
  // Contrast adjustment by pixel multiplication
  else{
    float contrast = (float) atof( argument.c_str() );
    session->view->contrast = contrast;
    if( session->loglevel >= 3 ) *(session->logfile) << "CNT :: requested contrast adjustment is " << contrast << endl;
  }
}


void GAM::run( Session* session, const string& argument ){

  string arg = argument;
  transform( arg.begin(), arg.end(), arg.begin(), ::tolower );

  if( session->loglevel >= 2 ) *(session->logfile) << "GAM handler reached" << endl;

  // Log transform
  if( arg == "log" || arg == "logarithm" ){
    // Use reserved value of -1 for logarithm
    session->view->gamma = -1;
    if( session->loglevel >= 3 ) *(session->logfile) << "GAM :: log transform requested" << endl;
  }
  // Exponential transform
  else{
    float gamma = (float) atof( argument.c_str() );
    session->view->gamma = gamma;
    if( session->loglevel >= 3 ) *(session->logfile) << "GAM :: requested gamma adjustment is " << gamma << endl;
  }
}


void CVT::run( Session* session, const string& src ){

  // Put the argument into lower case
  string argument = src;
  transform( argument.begin(), argument.end(), argument.begin(), ::tolower );

  if( argument == "jpeg" || argument == "jpg" ){
    session->view->output_format = ImageEncoding::JPEG;
    if( session->loglevel >= 3 ) *(session->logfile) << "CVT :: JPEG output" << endl;
  }
  else if( argument == "tiff" ){
    session->view->output_format = ImageEncoding::TIFF;
    if( session->loglevel >= 3 ) *(session->logfile) << "CVT :: TIFF output" << endl;
  }
#ifdef HAVE_PNG
  else if( argument == "png" ){
    session->view->output_format = ImageEncoding::PNG;
    if( session->loglevel >= 3 ) *(session->logfile) << "CVT :: PNG output" << endl;
  }
#endif
#ifdef HAVE_WEBP
  else if( argument == "webp" ){
    session->view->output_format = ImageEncoding::WEBP;
    if( session->loglevel >= 3 ) *(session->logfile) << "CVT :: WebP output" << endl;
  }
#endif
#ifdef HAVE_AVIF
  else if( argument == "avif" ){
    session->view->output_format = ImageEncoding::AVIF;
    if( session->loglevel >= 3 ) *(session->logfile) << "CVT :: AVIF output" << endl;
  }
#endif
  else{
    session->view->output_format = ImageEncoding::JPEG;
    if( session->loglevel >= 1 ) *(session->logfile) << "CVT :: Unsupported request: '" << argument << "'. Sending JPEG" << endl;
  }

  this->send( session );
}


void WID::run( Session* session, const string& argument ){

  int requested_width = atoi( argument.c_str() );

  if( session->loglevel >= 2 ) *(session->logfile) << "WID handler reached" << endl;
  if( session->loglevel >= 3 ) *(session->logfile) << "WID :: requested width is " << requested_width << endl;

  session->view->setRequestWidth( requested_width );

}


void HEI::run( Session* session, const string& argument ){

  int requested_height = atoi( argument.c_str() );

  if( session->loglevel >= 2 ) *(session->logfile) << "HEI handler reached" << endl;
  if( session->loglevel >= 3 ) *(session->logfile) << "HEI :: requested height is " << requested_height << endl;

  session->view->setRequestHeight( requested_height );

}


void RGN::run( Session* session, const string& argument ){

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
  // give 4 values and that we have a width and height greater than zero
  if( i == 4 && region[2]>0 && region[3]>0){
    session->view->setViewLeft( region[0] );
    session->view->setViewTop( region[1] );
    session->view->setViewWidth( region[2] );
    session->view->setViewHeight( region[3] );
  }

  if( session->loglevel >= 3 ){
    *(session->logfile) << "RGN :: requested region is x:" << region[0] << ", y:"
			<< region[1] << ", w:" << region[2] << ", h:" << region[3] << endl;
  }

}


void ROT::run( Session* session, const string& argument ){

  string rotationString = argument;
  if( rotationString.substr(0,1) == "!" ){
    session->view->flip = 1;
    rotationString.erase(0,1);
  }
  float rotation = (float) atof( rotationString.c_str() );

  if( session->loglevel >= 2 ) *(session->logfile) << "ROT handler reached" << endl;
  if( session->loglevel >= 3 ) *(session->logfile) << "ROT :: requested rotation is " << rotation << " degrees" << endl;

  // Set rotation - watch for a '!180' request, which is simply a vertical flip
  if( session->view->flip == 1 && rotation == 180 ) session->view->flip = 2;
  else session->view->setRotation( rotation );
}


void JTLS::run( Session* session, const string& argument ){

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

    // Simply pass this on to our JTL send command
    JTL jtl;
    jtl.send( session, values[1], values[2] );
  }

}


void JTL::run( Session* session, const string& argument ){

  /* The argument should consist of 2 comma separated values:
     1) resolution
     2) tile number
  */

  // Parse the argument list
  int delimitter = argument.find( "," );
  int resolution = atoi( argument.substr( 0, delimitter ).c_str() );
  int tile = atoi( argument.substr( delimitter + 1, argument.length() ).c_str() );

  // Send out the requested tile
  this->send( session, resolution, tile );
}


void SHD::run( Session* session, const string& argument ){

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


void CMP::run( Session* session, const string& argument ){

  /* The argument is the colormap type: available colormaps are
     HOT, COLD, JET, BLUE, GREEN, RED
   */

  // Convert to lower case in order to do our string comparison
  string ctype = argument;
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


void INV::run( Session* session, const string& argument ){
  // Does not take an argument
  if( session->loglevel >= 2 ) *(session->logfile) << "INV handler reached" << endl;
  session->view->inverted = true;
}


void LYR::run( Session* session, const string& argument ){

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


void CTW::run( Session* session, const string& src ){

  /* Matrices should be formatted as CTW=[a,b,c;d,e,f;g,h,i] where commas separate the column
     values within a single row and semi-colons separate each row.
     Thus, the above example represents the 3x3 square matrix:
     [ a b c
       d e f
       g h i ]
     Each row represents an output channel and each column the coefficients to be used for
     each input band. There should, therefore, be as many columns (coefficients) as input
     channels and as many rows as required output channels
  */

  // First URL decode our string
  URL url( src );
  string argument = url.decode();

  if( argument.length() ){
    if( session->loglevel >= 2 ) *(session->logfile) << "CTW handler reached" << endl;
  }

  int pos1 = argument.find("[");
  int pos2 = argument.find("]");

  // Extract the contents of the array
  string line = argument.substr( pos1+1, pos2-pos1-1 );

  // Tokenize on rows
  Tokenizer col_izer( line, ";" );

  while( col_izer.hasMoreTokens() ){

    // Fill each row item
    Tokenizer row_izer( col_izer.nextToken(), "," );
    vector<float> row;
    
    while( row_izer.hasMoreTokens() ){
      try{
	row.push_back( atof( row_izer.nextToken().c_str() ) );
      }
      catch( const string& error ){
	if( session->loglevel >= 1 ) *(session->logfile) << error << endl;
      }
    }
    session->view->ctw.push_back( row );
  }

  if( session->loglevel >= 3 ){
    *(session->logfile) << "CTW :: " << session->view->ctw[0].size() << "x" << session->view->ctw.size() << " matrix: " << endl;
    for( unsigned int i=0; i<session->view->ctw.size(); i++ ){
      *(session->logfile) <<  "CTW ::   ";
      for( unsigned int j=0;j<session->view->ctw[0].size(); j++ ){
	*(session->logfile) << session->view->ctw[i][j] << " ";
      }
      *(session->logfile) << endl;
    }
  }

}


void COL::run( Session* session, const string& argument ){
  /* The argument is the output color conversion. Supported values:
     GREY/GRAY: grayscale, BINARY: binary (bilevel)
  */

  // Convert to lower case in order to do our string comparison
  string ctype = argument;
  transform( ctype.begin(), ctype.end(), ctype.begin(), ::tolower );

  if( session->loglevel >= 2 ) *(session->logfile) << "COL handler reached" << endl;
  if( session->loglevel >= 3 ) *(session->logfile) << "COL :: requested color transform to " << ctype << endl;

  if( ctype == "grey" || ctype == "gray" ) session->view->colorspace = ColorSpace::GREYSCALE;
  else if( ctype == "binary" ) session->view->colorspace = ColorSpace::BINARY;
  
}


void CNV::run( Session* session, const string& src ){

  /* The argument is a predefined convolution kernel name or a comma separated
     list of values which are entries in a convolution filter kernel matrix.
     The matrix must be square and of odd dimension.
  */

  if( session->loglevel >= 3 ) *(session->logfile) << "CNV handler reached" << endl;

  // First URL decode our string
  URL url( src );
  string argument = url.decode();

  int pos1 = argument.find("[");
  int pos2 = argument.find("]");
  string matrix = argument.substr( pos1+1, pos2-pos1-1 );

  // Extract the contents of the array and convert to lower-case if necessary
  string ctype = matrix;
  transform( ctype.begin(), ctype.end(), ctype.begin(), ::tolower );

  if( ctype == "sobel" ){
    session->view->convolution = { -1, 0, +1,
				   -2, 0, +2,
				   -1, 0, +1 };
  }
  else if( ctype == "prewitt" ){
    session->view->convolution = { +1, 0, -1,
				   +1, 0, -1,
				   +1, 0, -1 };
  }
  else if( ctype == "scharr" ){
    session->view->convolution = { -3, 0, +3,
				  -10, 0, 10,
				   -3, 0, +3 };
  }
  else if( ctype == "laplace" ){
    session->view->convolution = { -1, -1, -1,
				   -1, +8, -1,
				   -1, -1, -1 };
  }
  else if( ctype == "gaussian" ){
    session->view->convolution = { 1, 2, 1,
				   2, 4, 2,
				   1, 2, 1 };
  }
  else if( ctype == "sharpen" ){
    session->view->convolution = {  0, -1,  0,
				   -1,  5, -1,
				    0, -1,  0 };
  }
  else if( ctype == "emboss" ){
    session->view->convolution = { -2, -1, 0,
				   -1,  1, 1,
				    0,  1, 2 };
  }
  else{

    Tokenizer izer( matrix, "," );

    vector<float> kernel;

    while( izer.hasMoreTokens() && kernel.size()<26 ){
      try{
       kernel.push_back( atof( izer.nextToken().c_str() ));
      }
      catch( const string& error ){
       if( session->loglevel >= 1 ) *(session->logfile) << error << endl;
      }
    }

    unsigned int dimension = (unsigned int) sqrtf( kernel.size() );

    if( kernel.size() >= 26 ){
      if( session->loglevel >= 2 ){
	*(session->logfile) << "CNV :: Maximum convolution matrix size is 5x5. Supplied matrix: "
			    << argument << " will be ignored" << endl;
      }
    }
    else if( dimension * dimension != kernel.size() ){
      if( session->loglevel >= 2 ){
	*(session->logfile) << "CNV :: Convolution matrix must be square. Supplied matrix: "
			    << argument << " will be ignored" << endl;
      }
    }
    else if( dimension % 2 == 0 ){
      if( session->loglevel >= 2 ){
	*(session->logfile) << "CNV :: Convolution matrix must have odd dimension. Supplied matrix: "
			    << argument << " will be ignored" << endl;
      }
    }
    else {
      session->view->convolution = kernel;
    }
  }

}
