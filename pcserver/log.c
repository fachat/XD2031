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
#include <strings.h>
#include <stdarg.h>
#include <ctype.h>

static int verbose = 0;
static int newline = 1;

void set_verbose() {
	verbose = 1;
}

void log_term(const char *msg) {
	
	int newline = 0;

	printf(">>>: ");
	char c;
	while ((c = *msg) != 0) {
		if (newline) {
			printf("\n>>>: ");
		}
		if (isprint(c)) {
			putchar(c);
			newline = 0;
		} else
		if (c == 10) {
			newline = 1;
		} else
		if (c == 13) {
			// silently ignore
		} else {
			printf("{%02x}", ((int)c) & 0xff);
			newline = 0;
		}
		msg++;
	}
	printf("\n");
	fflush(stdout);
}

void log_errno(const char *msg, ...) {
       va_list args;
       va_start(args, msg);
	char buffer[1024];

	if (strchr(msg, '%') != NULL) {
		// the msg contains parameter, so we need to fix it
		vsprintf(buffer, msg, args);
		msg = buffer;
	}
        printf("ERN: %s: errno=%d: %s\n", msg, errno, strerror(errno));
	fflush(stdout);
}

void log_warn(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

	if (newline != 1) {
       		printf("WRN:");
	}
	newline = 0;
	if (msg[strlen(msg)-1]!='\n') {
		newline = 1;
	}
        vprintf(msg, args);
	fflush(stdout);
}

void log_error(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

	if (newline != 2) {
       		printf("ERR:");
	}
	newline = 0;
	if (msg[strlen(msg)-1]!='\n') {
		newline = 2;
	}
        vprintf(msg, args);
	fflush(stdout);
}

void log_info(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

       	if (newline != 3) {
		printf("INF:");
	}
	newline = 0;
	if (msg[strlen(msg)-1]!='\n') {
		newline = 3;
	}
        vprintf(msg, args);
	fflush(stdout);
}

void log_debug(const char *msg, ...) {
	if (verbose) {
       		va_list args;
       		va_start(args, msg);

       		if (newline != 4) {
			printf("DBG:");
		}
		newline = 0;
		if (msg[strlen(msg)-1]!='\n') {
			newline = 4;
		}
        	vprintf(msg, args);
	}
	fflush(stdout);
}



