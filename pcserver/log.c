/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

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


#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

void log_errno(const char *msg) {
        printf(">> %s: errno=%d: %s\n", msg, errno, strerror(errno));
}

void log_warn(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

       printf("WRN:");
        vprintf(msg, args);
}

void log_error(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

       printf("ERR:");
        vprintf(msg, args);
}

void log_info(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

       printf("INF:");
        vprintf(msg, args);
}

void log_debug(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

       printf("DBG:");
        vprintf(msg, args);
}



