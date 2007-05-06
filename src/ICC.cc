/*
    IIP ICC Command Handler

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


#include "Task.h"

// lcms not finished at the moment. Disable it for now.
#undef LCMS

#ifdef LCMS
#include <lcms/lcms.h>



using namespace std;


void ICC::run( Session* session, std::string argument ){

  unsigned char icc_profile[1024];

  // Parse the argument list
  delimitter = argument.find( "," );
  string tmp = argument.substr( 0, delimitter );
  unsigned int icc_len = atoi( tmp.c_str() );
  argument = argument.substr( delimitter + 1, argument.length() );

  delimitter = argument.find( "," );
  tmp = argument.substr( 0, delimitter );
// 	  icc_profile = tmp.c_str();
  argument = argument.substr( delimitter + 1, argument.length() );

  cmsHPROFILE out_profile, lab_profile, sRGB_profile;
  cmsHTRANSFORM hTransform;
  LPcmsCIExyY WhitePoint;

  out_profile = cmsOpenProfileFromMem( (LPVOID) tmp.c_str(),
				       (DWORD) icc_len );
  lab_profile = cmsCreateLabProfile( WhitePoint );
  sRGB_profile = cmsCreate_sRGBProfile();

  cmsWhitePointFromTemp( 6504, WhitePoint );
  lab_profile = cmsCreateLabProfile( WhitePoint );
  hTransform = cmsCreateTransform( sRGB_profile, TYPE_RGB_8,
				   out_profile, TYPE_RGB_8,
				   INTENT_ABSOLUTE_COLORIMETRIC, 0);
  cmsDoTransform( );
  cmsDeleteTransform( hTransform );
  cmsCloseProfile( out_profile );
  cmsCloseProfile( lab_profile );
  cmsCloseProfile( sRGB_profile );

}


#else


void ICC::run( Session* session, std::string argument ){ ; };


#endif
