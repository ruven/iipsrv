/*  Missing time implementations for Windows

    Copyright (C) 2011-2012 Ruven Pillay.

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

#ifndef TIME_H
#define TIME_H

// Define mising time related functions on Windows and add round function for MSVC compiler
#ifdef WIN32

#include <Windows.h>

struct timezone
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};

time_t timegm(struct tm*);
char *strptime(const char*, const char*, struct tm*);
int gettimeofday(struct timeval*, struct timezone*);
double round(double);

#endif
#endif
