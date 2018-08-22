/* 
   XD2031 - Serial line file server for CBMs
   Copyright (C) 2018 Andre Fachat <afachat@gmx.de>
  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License only.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <errno.h>

#include "cerrno.h"
#include "wireformat.h"

// error translation

int errno_to_error(int err) {

        switch(err) {
        case EEXIST:
                return CBM_ERROR_FILE_EXISTS;
        case EACCES:
                return CBM_ERROR_NO_PERMISSION;
        case ENAMETOOLONG:
                return CBM_ERROR_FILE_NAME_TOO_LONG;
        case ENOENT:
                return CBM_ERROR_FILE_NOT_FOUND;
        case ENOSPC:
                return CBM_ERROR_DISK_FULL;
        case EROFS:
                return CBM_ERROR_WRITE_PROTECT;
        case ENOTDIR:   // mkdir, rmdir
        case EISDIR:    // open, rename
                return CBM_ERROR_FILE_TYPE_MISMATCH;
        case ENOTEMPTY:
                return CBM_ERROR_DIR_NOT_EMPTY;
        case EMFILE:
                return CBM_ERROR_NO_CHANNEL;
        case EINVAL:
                return CBM_ERROR_SYNTAX_INVAL;
        default:
                return CBM_ERROR_FAULT;
        }
}

