/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2011 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "WayPointFile.hpp"

#include "WayPointFileZander.hpp"
#include "WayPointFileSeeYou.hpp"
#include "WayPointFileWinPilot.hpp"

#include "Terrain/RasterTerrain.hpp"
#include "Waypoint/Waypoints.hpp"
#include "UtilsFile.hpp"
#include "OS/FileUtil.hpp"
#include "ProgressGlue.hpp"
#include "IO/FileLineReader.hpp"
#include "IO/ZipLineReader.hpp"
#include "IO/TextWriter.hpp"

#include <assert.h>

WayPointFile::WayPointFile(const TCHAR* file_name, const int _file_num):
  file_num(_file_num),
  terrain(NULL)
{
  _tcscpy(file, file_name);
}

static bool is_closing_quote_char(TCHAR const *s) {
  // Perform look-ahead to check if the detected quote_char terminates
  // the quoted field. true for (optional) ' 's followed by ',' or
  // end of input string.
  while (*s != _T('\0')) {
    if (*s == _T(','))
      return true;
    if (*s != _T(' '))
      return false;
    s++;
  }
  return true;
}

size_t
WayPointFile::extractParameters(const TCHAR *src, TCHAR *dst,
                                const TCHAR **arr, size_t sz,
                                const bool trim, const TCHAR quote_char)
{
  assert(src != NULL);
  assert(dst != NULL);
  assert(arr != NULL);
  assert(sz > 0);

  TCHAR *d = dst;
  TCHAR const *s = src;
  bool in_quote = false;

  size_t i = 0;
  *d = _T('\0');
  arr[i] = d;

  // pointer to last character of d after which ' ' might be discarded for
  // trimming. NULL can be used to test for start of field condition.
  TCHAR *last_non_discardable_char = NULL;

  do {
    if (quote_char && *s == quote_char) {
      // Handle quote characters if enabled, current char of src is quote_char
      if (in_quote && is_closing_quote_char(s+1)) {
        // If currently in quoted string leave quote handling depending on
        // look-ahead performed in is_closing_quote_char()
        in_quote = false;
      } else if (!in_quote && last_non_discardable_char == NULL) {
        // Quote start detected at beginning of new field
        in_quote = true;
      } else {
        // Quote inside quoted string (but not end), copy to dst
        last_non_discardable_char = d;
        *d++ = *s;
        if (*(s+1) == quote_char) {
          // Spreadsheets use "" while exporting a single " contained in
          // a textfield to a CSV file - skip second "
          s++;
        }
      }
    } else if (*s == _T('\0') || ((*s == _T(',') && !in_quote))) {
      // end of src or field separator (',') outside quoted part detected
      // field content is already completely copied but may contain trailing spaces
      if (trim && (last_non_discardable_char != NULL))
        // remove trailing unquoted whitespace from dst field
        d = last_non_discardable_char+1;

      // 0-terminate field and update pointer to individual field
      *d++ = _T('\0');
      if (++i < sz)
        // if size permits remember start of next field.
        arr[i] = d;

      // start processing of next field
      last_non_discardable_char = NULL;
    } else {
      // main part copying from src to dst
      if (trim && !in_quote) {
        // we are not inside a quoted string and therefore need to trim
        // leading and trailing spaces. Drop whitespace if dst is still empty
        if (*s == _T(' ')) {
          if (last_non_discardable_char != NULL)
            // dst field is not empty - append ' '
            *d++ = *s;
        } else {
          // no space, remember position and append to result
          last_non_discardable_char = d;
          *d++ = *s;
        }
      } else {
        // normal copy src to dst, all chars are retained
        // all characters are not subject to trimming (even spaces)
        last_non_discardable_char = d;
        *d++ = *s;
      }
    }
    // advance src until end reached or no space in field pointers
  } while (*s++ != _T('\0') && i < sz);

  return i;
}

void
WayPointFile::check_altitude(Waypoint &new_waypoint, bool alt_ok)
{
  if (terrain == NULL || alt_ok)
    return;

  // Load waypoint altitude from terrain
  const short t_alt = terrain->GetTerrainHeight(new_waypoint.Location);
  if (RasterBuffer::is_special(t_alt))
    new_waypoint.Altitude = fixed_zero;
  else
    // TERRAIN_VALID
    new_waypoint.Altitude = (fixed)t_alt;
}

bool
WayPointFile::Parse(Waypoints &way_points, bool compressed)
{
  // If no file loaded yet -> return false
  if (file[0] == 0)
    return false;

  ProgressGlue::SetRange(25);

  // If normal file
  if (!compressed) {
    // Try to open waypoint file
    FileLineReader reader(file);
    if (reader.error())
      return false;

    double filesize = std::max(reader.size(), 1l);

    // Read through the lines of the file
    TCHAR *line;
    for (unsigned i = 0; (line = reader.read()) != NULL; i++) {
      // and parse them
      parseLine(line, i, way_points);

      if ((i & 0x3f) == 0) {
        unsigned status = reader.tell() * 25 / filesize;
        ProgressGlue::SetValue(status);
      }
    }
  // If compressed file inside map file
  } else {
    // convert path to ascii
    ZipLineReader reader(file);
    if (reader.error())
      return false;

    double filesize = std::max(reader.size(), 1l);

    // Read through the lines of the file
    TCHAR *line;
    for (unsigned i = 0; (line = reader.read()) != NULL; i++) {
      // and parse them
      parseLine(line, i, way_points);

      if ((i & 0x3f) == 0) {
        unsigned status = reader.tell() * 25 / filesize;
        ProgressGlue::SetValue(status);
      }
    }
  }

  return true;
}

bool
WayPointFile::Save(const Waypoints &way_points)
{
  // No filename -> return
  if (file[0] == 0)
    return false;

  // Not writable -> return
  if (!IsWritable())
    return false;

  // Try to open waypoint file for writing
  TextWriter writer(file);
  if (writer.error())
    return false;

  saveFile(writer, way_points);

  // and tell everyone we saved successfully
  return true;
}

WayPointFile*
WayPointFile::create(const TCHAR* filename, int the_filenum)
{
  // If filename is empty -> clear and return NULL pointer
  if (string_is_empty(filename))
    return NULL;

  // Test if file exists
  if (!File::Exists(filename)) {
    // Test if file exists in zip archive
    ZipSource zip(filename);
    if (zip.error())
      // If the file doesn't exist return NULL pointer
      return NULL;
  }

  // If WinPilot waypoint file -> save type and return true
  if (MatchesExtension(filename, _T(".dat")) ||
      MatchesExtension(filename, _T(".xcw")))
    return new WayPointFileWinPilot(filename, the_filenum);

  // If SeeYou waypoint file -> save type and return true
  if (MatchesExtension(filename, _T(".cup")))
    return new WayPointFileSeeYou(filename, the_filenum);

  // If Zander waypoint file -> save type and return true
  if (MatchesExtension(filename, _T(".wpz")))
    return new WayPointFileZander(filename, the_filenum);

  // unknown
  return NULL;
}
