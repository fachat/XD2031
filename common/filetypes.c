/**************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat, Nils Eilers

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301, USA.

***************************************************************************/

/** @file filetypes.c
 * Convert filetypes to strings and vice versa
 */

#ifdef __STRICT_ANSI__		// enable strcasecmp()
#undef __STRICT_ANSI__
#endif

#include "archcompat.h"

#include <inttypes.h>
#include <string.h>

// TODO: how about DIR ?

#define KNOWN_EXT 5
static const char extensions[KNOWN_EXT][5] IN_ROM =
	{ ".DEL", ".SEQ", ".PRG", ".USR", ".REL" };

/** @brief Converts filename[.ext] to filetype
 *
 * Compares case insensitive
 *
 *  @param     filename              A filename which may contain a known filetype
 *  @param     no_extension_default  Returned if filename has no extension
 *  @param     default_unknown       Returned if extension is unknown
 *  @returns                         filetype as defined in wireformat.h,
 *                                   no_extension_default if the filename has no ext.
 *                                   default_unknown if unknown
*/

int8_t extension_to_filetype (char* filename, uint8_t no_extension_default, uint8_t default_unknown) {
   if (!filename) return default_unknown;
   if (!strchr(filename, '.')) return no_extension_default;
   if (strlen(filename) < 4) return default_unknown;
   char *p = filename + strlen(filename) - 4;
   for (uint8_t i=0; i < KNOWN_EXT; i++) {
      if (!strcasecmp(p, extensions[i])) return i;
   }
   return default_unknown;
}


/** @brief Convert filetype to extension string
 *
 * Default to .PRG for undefined filetypes (which should not happen)
 *
 * @param    filetype    0-4, as defined in wireformat.h
 * @returns              pointer to string with .ext
 *
*/

const char* filetype_to_extension (uint8_t filetype) {
   if (filetype > (KNOWN_EXT - 1)) return (extensions[2]); // default to PRG
   return extensions[filetype];
}
