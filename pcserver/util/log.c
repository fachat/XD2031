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
#include <ctype.h>
#include <inttypes.h>

#include "petscii.h"
#include "terminal.h"

#ifndef LOG_PREFIX
#define	LOG_PREFIX	""
#endif

static int verbose = 0;

static const char* spaces = "                                                                  ";

static enum lastlog {
	lastlog_anything, lastlog_warn, lastlog_error, lastlog_info, lastlog_debug
} newline = lastlog_anything;

void set_verbose(int flag) {
	verbose = flag;
}

void log_term(const char *msg) {
	
	int newline = 0;

	color_log_term();
	printf(LOG_PREFIX ">>>: ");
	char c;
	while ((c = *msg) != 0) {
		if (newline) {
			printf("\n" LOG_PREFIX ">>>: ");
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
	color_default();
	fflush(stdout);
}

void log_errno(const char *msg, ...) {
       va_list args;
       va_start(args, msg);
	char buffer[1024];

	color_log_error();
	if (strchr(msg, '%') != NULL) {
		// the msg contains parameter, so we need to fix it
		vsprintf(buffer, msg, args);
		msg = buffer;
	}
        printf(LOG_PREFIX "ERN: %s: errno=%d: %s\n", msg, errno, strerror(errno));
	newline = lastlog_anything;
	color_default();
	fflush(stdout);
}

void log_warn(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

	color_log_warn();
	if (newline != lastlog_warn) {
       		printf(LOG_PREFIX "WRN:");
	}
	newline = lastlog_anything;
	if (msg[strlen(msg)-1]!='\n') {
		newline = lastlog_warn;
	}
        vprintf(msg, args);
	color_default();
	fflush(stdout);
}

void log_error(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

	color_log_error();
	if (newline != lastlog_error) {
       		printf(LOG_PREFIX "ERR:");
	}
	newline = lastlog_anything;
	if (msg[strlen(msg)-1]!='\n') {
		newline = lastlog_error;
	}
        vprintf(msg, args);
	color_default();
	fflush(stdout);
}

void log_info(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

	color_log_info();
	if (newline != lastlog_info) {
		printf(LOG_PREFIX "INF:");
	}
	newline = lastlog_anything;
	if (msg[strlen(msg)-1]!='\n') {
		newline = lastlog_info;
	}
        vprintf(msg, args);
	color_default();
	fflush(stdout);
}

void log_debug(const char *msg, ...) {
	if (verbose) {
       		va_list args;
       		va_start(args, msg);

		color_log_debug();
		if (newline != lastlog_debug) {
			printf(LOG_PREFIX "DBG:");
		}
		newline = lastlog_anything;
		if (msg[strlen(msg)-1]!='\n') {
			newline = lastlog_debug;
		}
        	vprintf(msg, args);
		color_default();
	}
	fflush(stdout);
}


void log_hexdump2(const char *p, int len, int petscii, const char *prefix) {
	int tot = 0;
	int line = 0;
	int x = 0;
	const char *spaceprefix = spaces + strlen(spaces) - strlen(prefix);

	newline = lastlog_anything;
	color_log_debug();

	if(len) {
		while(tot < len) {
			if (tot == 0) {
				printf(LOG_PREFIX "%s%04X  ", prefix, tot);
			} else {
				printf(LOG_PREFIX "%s%04X  ", spaceprefix , tot);
			}
			for(x=0; x<16; x++) {
				if(line+x < len) {
					tot++;
					printf("%02X ", 255&p[line+x]);
				}
				else printf("   ");
				if(x == 7) putchar(' ');
			}
			printf(" |");
			for(x=0; x<16; x++) {
				if(line+x < len) {
					int c = p[line+x];
					if (petscii) c = petscii_to_ascii(c);
					if(isprint(c)) putchar(c); else putchar(' ');
				} else putchar(' ');
			}
			printf("|\n");
			line = tot;
		}

	}
	color_default();
}

void log_hexdump(const char *p, int len, int petscii) {
	log_hexdump2(p, len, petscii, "");
}


const char* dump_indent(int n) {
	return spaces + strlen(spaces) - n * 2;
}



