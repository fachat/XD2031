/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat
    Copyright (C) 2012 Nils Eilers

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

#include "os.h"

#include <pwd.h>
#include <unistd.h>

/* patch dir separator characters to '/'
 * fs_provider (Linux / OS X), http and ftp require the slash.
 * Windows user would prefer the backslash, but Windows can cope with 
 * the forward slash too, the same is true for FatFs.
 */

char *patch_dir_separator(char *path) {
	char *newpath = path;

	while(*path) {
		if(*path == '\\') *path = dir_separator_char();
		path++;
	}

	return newpath;
}

const char* get_home_dir (void) {
        char* dir = getenv("HOME");
        if(!dir) {
                struct passwd* pwd = getpwuid(getuid());
                if(pwd) dir = pwd->pw_dir;
                else {
                        fprintf(stderr, "Unable to determine home directory.\n");
                        exit(1);
                }
        }
        return dir;
}

