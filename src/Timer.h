/*  IIPImage server :: Timer class

    Copyright (C) 2005-2022 Ruven Pillay.

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


#ifndef _TIMER_H
#define _TIMER_H


#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

// Use the STL chrono classes if available
#ifdef HAVE_STL_CHRONO
#include <chrono>
#endif

#ifdef WIN32
#include "../windows/Time.h"
#endif

/// Simple Timer class to allow us to time our responses

class Timer {


 private:

#ifdef HAVE_STL_CHRONO
  /// Our start time based on the high_resolution_clock
  std::chrono::time_point<std::chrono::high_resolution_clock> start_t;
#else

  /// Time structure for sys time
  struct timeval tv;

  /// Timezone structure for sys time
  struct timezone tz;

  /// Start time in seconds
  long start_t;

  /// Start time in microseconds
  long start_u;

#endif


public:

  /// Constructor
  Timer() {;};


  /// Set our time
  /** Initialise with our start time */
  void start() {
#ifdef HAVE_STL_CHRONO
    start_t = std::chrono::high_resolution_clock::now();
#else
    tz.tz_minuteswest = 0;
    if( gettimeofday( &tv, NULL ) == 0 ){
      start_t = tv.tv_sec;
      start_u = tv.tv_usec;
    }
    else start_t = start_u = 0;
#endif
  }


  /// Return time since we were initialised in microseconds 
  long getTime() {
#ifdef HAVE_STL_CHRONO
    auto d = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - start_t );
    return d.count();
#else
    if( gettimeofday( &tv, NULL ) == 0 ) return (tv.tv_sec - start_t) * 1000000 + (tv.tv_usec - start_u);
    else return 0;
#endif
  }


};


#endif
