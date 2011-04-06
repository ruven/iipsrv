// Timer class

/*  IIP fcgi server module

    Copyright (C) 2005-2011 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifndef _TIMER_H
#define _TIMER_H


#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif


#ifdef WIN32
#include "../windows/Time.h"
#endif


/// Simple Timer class to allow us to time our responses

class Timer {


 private:

  /// Time structure
  struct timeval tv;

  /// Timezone structure
  struct timezone tz;

  /// Our start time in seconds
  long start_t;

  /// The microsecond component of our start time
  long start_u;


 public:

  /// Constructor
  Timer() {;};


  /// Set our time
  /** Initialise with our start time */
  void start() {
    tz.tz_minuteswest = 0;
    if( gettimeofday( &tv, NULL ) == 0 ){
      start_t = tv.tv_sec;
      start_u = tv.tv_usec;
    }
    else start_t = start_u = 0;
  }


  /// Return time since we were initialised in microseconds 
  long getTime() {
    if( gettimeofday( &tv, NULL ) == 0 ) return (tv.tv_sec - start_t) * 1000000 + (tv.tv_usec - start_u);
    else return 0;
  }


};



#endif

