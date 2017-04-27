/*  Generic compressor wrapper to jpeg library and png library

    Copyright (C) 2000-2017 Ruven Pillay - extended by Dave Beaudet

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



#ifndef _COMPRESSOR_H
#define _COMPRESSOR_H



#include <cstdio>
#include <string>
#include <stdexcept>

#include "RawTile.h"

using namespace std;

#ifdef __GNUC__
#  include <features.h>
#  if __GNUC_PREREQ(4,7)
#    define OVERRIDE override
#  else
#    define OVERRIDE 
#  endif
#else
#    define OVERRIDE 
#endif

/// Base class for IIP output images

class Compressor {

    public:

        virtual ~Compressor() {};

        /*****************************************
            All Virtual Methods
        *****************************************/

        /// Return the image header size
        virtual unsigned int getHeaderSize() { return 0; }; 

        /// Return a pointer to the image header itself
        virtual unsigned char* getHeader() { return NULL; }; 

        /// Release the header data
        virtual void finishHeader() { }; 

        /// Initialise strip based compression
        /** If we are doing a strip based encoding, we need to first initialise
            with InitCompression, then compress a single strip at a time using
            CompressStrip and finally clean up using Finish
            @param rawtile tile containing the image to be compressed
            @param strip_height pixel height of the strip we want to compress
        */
        virtual void InitCompression( const RawTile& rawtile, unsigned int strip_height ) throw (string) { logfile << "compressor here" << endl;};

        /// Compress a strip of image data
        /** @param s source image data
            @param o output buffer
            @param olen output buffer length
            @param tile_height pixel height of the tile we are compressing
        */
        virtual unsigned int CompressStrip( unsigned char* s, unsigned char* o, unsigned long olen, unsigned int tile_height ) throw (string) { return 0; }

        /// Finish the strip based compression and free memory
        /** @param output output buffer
            @return size of output generated
        */
        virtual unsigned int Finish( unsigned char* output, unsigned long outputlen ) throw (string) { return 0; }

        /// Compress an entire buffer of image data at once in one command
        /** @param t tile of image data */
        virtual int Compress( RawTile& t ) throw (string) { return 0; }

        /// Add metadata to the image header
        /** @param m metadata */
        virtual void addXMPMetadata( const string& m ) { }

        /// Add metadata to the image header
        /** @param m metadata */
        virtual string getMimeType() { return ""; }

        /// Set the compression quality
        /** @param factor Quality factor (0-100) */
        virtual void setQuality( int factor ) { }

        /// Get the current quality level
        virtual int getQuality() { return 0; }

};


#endif
