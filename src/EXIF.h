/*  EXIF metadata handling:
    Extracts EXIF data from a TIFF and writes it back as an EXIF IFD

    Copyright (C) 2026 Ruven Pillay

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


#ifndef _EXIF_H
#define _EXIF_H

#include <cstdint>
#include <string>
#include <vector>
#include <tiffio.h>

namespace exif {

  /// Extract the EXIF and GPS directories from a TIFF as a standalone EXIF blob
  /** The blob is a classic TIFF with an IFD0 referencing the EXIF IFD
      (tag 0x8769) and, if present, the GPS IFD (tag 0x8825).
      @param source TIFF to read the directories from
      @param data output buffer receiving the standalone EXIF blob
      @param exif_offset offset of the EXIF directory (0 if absent)
      @param gps_offset offset of the GPS directory (0 if absent)
      @return true on success
  */
  bool extract( TIFF* source, std::vector<unsigned char>& data,
                uint64_t exif_offset, uint64_t gps_offset );


  /// Write an EXIF blob into a TIFF as EXIF and GPS IFDs
  /** @param destination TIFF to write the directories to
      @param data standalone EXIF blob, with or without the "Exif\0\0" prefix
      @param exif_offset output offset of the written EXIF directory (0 if none)
      @param gps_offset output offset of the written GPS directory (0 if none)
      @return true on success
  */
  bool write( TIFF* destination, const std::string& data,
              uint64_t& exif_offset, uint64_t& gps_offset );

}

#endif
