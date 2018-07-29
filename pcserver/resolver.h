
/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

#ifndef RESOLVER_H
#define RESOLVER_H


/**
 * resolve a given file-path (pattern) to the final directory (returned in outdir)
 * and the filename rest of the given file-path (returned in pattern)
 */
int resolve(const char **pattern, charset_t cset, file_t **outdir);


/**
 * scan a given directory (dir) for a search file-pattern (pattern), returning
 * the resulting directory entry in direntry. The pattern in/out parameter
 * is moved to behind the consumed file-pattern. 
 *
 * This method can be called multiple times to find all matching directory
 * entries (as long as pattern is reset between calls).
 *
 * Note that the directory entry is potentially wrapped in case an encapsulated
 * file (e.g. .gz, .P00) or directory (.zip, .D64) is detected.
 *
 * When isdirscan is true, the directory entries for disk header and blocks free
 * are also returned where available.
 * rdflag 
 */
int resolve_scan(file_t *dir, const char **pattern, charset_t cset, bool isdirscan,
	direntry_t **outde, int *rdflag);


#endif
