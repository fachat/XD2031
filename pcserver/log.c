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
#include <inttypes.h>

#include "petscii.h"

static int verbose = 0;

static enum lastlog {
	lastlog_anything, lastlog_warn, lastlog_error, lastlog_info, lastlog_debug
} newline = lastlog_anything;

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
	newline = lastlog_anything;
	fflush(stdout);
}

void log_warn(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

	if (newline != lastlog_warn) {
       		printf("WRN:");
	}
	newline = lastlog_anything;
	if (msg[strlen(msg)-1]!='\n') {
		newline = lastlog_warn;
	}
        vprintf(msg, args);
	fflush(stdout);
}

void log_error(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

	if (newline != lastlog_error) {
       		printf("ERR:");
	}
	newline = lastlog_anything;
	if (msg[strlen(msg)-1]!='\n') {
		newline = lastlog_error;
	}
        vprintf(msg, args);
	fflush(stdout);
}

void log_info(const char *msg, ...) {
       va_list args;
       va_start(args, msg);

	if (newline != lastlog_info) {
		printf("INF:");
	}
	newline = lastlog_anything;
	if (msg[strlen(msg)-1]!='\n') {
		newline = lastlog_info;
	}
        vprintf(msg, args);
	fflush(stdout);
}

void log_debug(const char *msg, ...) {
	if (verbose) {
       		va_list args;
       		va_start(args, msg);

		if (newline != lastlog_debug) {
			printf("DBG:");
		}
		newline = lastlog_anything;
		if (msg[strlen(msg)-1]!='\n') {
			newline = lastlog_debug;
		}
        	vprintf(msg, args);
	}
	fflush(stdout);
}

void log_hexdump(char *p, int len, int petscii) {
	int tot = 0;
	int line = 0;
	int x = 0;

	newline = lastlog_anything;

	if(len) {
		while(tot < len) {
			printf("%04X  ", tot);
			for(x=0; x<16; x++) {
				if(line+x < len) {
					tot++;
					printf("%02X ", p[line+x]);
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
}

#if 0
void test_log(void) {
	log_term("log_term ");
	log_term("without CR\n");
	log_term("log_term with CR\n");
	log_term("log_term with CR\n");

	log_info("log_info ");
	log_info("without CR\n");
	log_info("log_info with CR\n");
	log_info("log_info with CR\n");

	log_warn("log_warn ");
	log_warn("without CR\n");
	log_warn("log_warn with CR\n");
	log_warn("log_warn with CR\n");

	log_error("log_error ");
	log_error("without CR\n");
	log_error("log_error with CR\n");
	log_error("log_error with CR\n");

	// log_errno supports only single lines without CR
	log_errno("log_errno without CR");

	printf("--> log_debug() messages appear only with -v option set\n");
	log_debug("log_debug ");
	log_debug("without CR\n");
	log_debug("log_debug with CR\n");
	log_debug("log_debug with CR\n");
}
#endif
