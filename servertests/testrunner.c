
/****************************************************************************

    xd2031 filesystem server
    Copyright (C) 2012,2014 Andre Fachat

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

/**
 * testrunner.c
 *
 * Part of the xd2031 file system server
 * 
 * this code connects to a Unix socket and uses a script to send
 * commands, resp. receive replies and check whether they are correct
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "log.h"
#include "terminal.h"
#include "types.h"
#include "registry.h"
#include "mem.h"

#define	FS_SYNC		127

void usage(int rv) {
        printf("Usage: fsser [options] run_directory\n"
                " options=\n"
                "   -d <device> define serial device to use\n"
                "   -?          gives you this help text\n"
        );
        exit(rv);
}

// Assert switch is a single character
// If somebody tries to combine options (e.g. -vD) or
// encloses the parameter in quotes (e.g. fsser "-d COM5")
// this function will throw an error
void assert_single_char(char *argv) {
        if (strlen(argv) != 2) {
                log_error("Unexpected trailing garbage character%s '%s' (%s)\n",
                                strlen(argv) > 3 ? "s" : "", argv + 2, argv);
                exit (1);
        }
}

int socket_open(const char *socketname) {

   	int sockfd, servlen;
   	struct sockaddr_un  server_addr;

   	log_info("Connecting to socket %s\n", socketname);

   	memset((char *)&server_addr, 0, sizeof(server_addr));
   	server_addr.sun_family = AF_UNIX;
   	strcpy(server_addr.sun_path, socketname);
   	servlen = strlen(server_addr.sun_path) + 
                 sizeof(server_addr.sun_family);
   	if ((sockfd = socket(AF_UNIX, SOCK_STREAM,0)) < 0) {
       		log_error("Creating socket");
		return -1;
   	}
   	if (connect(sockfd, (struct sockaddr *) 
                         &server_addr, servlen) < 0) {
       		log_error("Connecting");
   		return -1;
   	}
   	return sockfd;
}


// -----------------------------------------------------------------------
// script handling

/** 
 * read script, and build up memory structure
 */

// line of a script

typedef struct {
	int		cmd;		// command for the line
	int		num;
	char 		*buffer;	// pointer to malloc'd line buffer
	int		length;		// length of buffer (amount of data in it, NOT capacity)
	registry_t	scriptlets;
} line_t;

static void line_init(const type_t *type, void *obj) {
	(void) type; // silence
	line_t *line = (line_t*) obj;

	line->buffer = NULL;
	line->length = 0;

	reg_init(&line->scriptlets, "line_scriptlets", 2);
}

static type_t line_type = {
	"script_line",
	sizeof(line_t),
	line_init
};


// scriptlet (within a parsed line)

typedef struct _scriptlet scriptlet_t;

struct _scriptlet {
	int 		type;
	int		pos;
	int		(*exec)(line_t *line, scriptlet_t *scr);
};

static void scriptlet_init(const type_t *type, void *obj) {
	(void) type; // silence
	scriptlet_t *scr = (scriptlet_t*) obj;

	scr->pos = 0;
}

static type_t scriptlet_type = {
	"line_scriptlet",
	sizeof(scriptlet_t),
	scriptlet_init
};

// parse a buffer line (i.e. a series of hex numbers and strings, possibly with scriplets)

int scr_len(line_t *line, scriptlet_t *scr) {

	line->buffer[scr->pos] = line->length;

	return 0;
}

// scriplets
static const struct {
	const char *name;
	const int namelen;
	const int outlen;
	int (*exec)(line_t *line, scriptlet_t *scr);
} scriptlets[] = {
	{ "len", 3, 1, scr_len },
	{ NULL, 0, 0, NULL }
};

// parse a hex byte
static const char* parse_hexbyte(const char *inp, char *out) {
	
	const char *p = inp;
	int val = 0;
	char v = 0;

	do {
		v = *(p++);
		if (v >= 0x30 && v <= 0x39) {
			v -= 0x30;
		} else {
			v &= 0x1f;
			v += 9;
		}
		val = (val << 4) + v;

	} while (isxdigit(*p));

	if (val & (~0xff)) {
		log_error("Overflow error parsing hex '%s'\n", inp);
		return NULL;
	}

	*out = val;
	return p;
}

static void free_scriptlet(registry_t *reg, void *obj) {
	(void) reg;	// silence
	mem_free(obj);
}

static void free_line(registry_t *reg, void *obj) {
	(void) reg;	// silence
	line_t* line = (line_t*) obj;

	reg_free(&line->scriptlets, free_scriptlet);

	mem_free(obj);
}

static int parse_buf(line_t *line, const char *in, char **outbuf, int *outlen) {

	// output buffer - TODO fix size checks
	char buffer[8192];
	int outp = 0;
	const char *p = in;

	log_debug("parse_buf(%s)\n", in);

	do {

		while (isspace(*p)) { p++; }

		if (isxdigit(*p)) {
			p = parse_hexbyte(p, buffer+outp);
			outp++;
		} else
		if ((*p) == '.') {
			// find scriptlet
			p++;
			int cmd = 0;
			int ch;
			while (scriptlets[cmd].name != NULL) {
				ch = 0;
				while ((scriptlets[cmd].name[ch] != 0) 
					&& (scriptlets[cmd].name[ch] == p[ch])) {
					ch++;
				}
				if ((scriptlets[cmd].name[ch] == 0) && ((p[ch] == 0) || isspace(p[ch]))) {
					// found
					break;
				}
				cmd++;
			}
			if (scriptlets[cmd].name == NULL) {
				log_error("Could not parse line scriptlet ('%s')!\n", p);
				return -1;
			}
			p += scriptlets[cmd].namelen;

			scriptlet_t *scr = mem_alloc(&scriptlet_type);
			scr->pos = outp;
			scr->exec = scriptlets[cmd].exec;
			reg_append(&line->scriptlets, scr);
			outp += scriptlets[cmd].outlen;
		} else {
			log_error("Could not parse buffer at line %d: '%s'\n", line->num, p);
			return -1;
		}

	} while (*p && ((*p) != '\n'));

	*outbuf = mem_alloc_c(outp, "parse_buf_outbuf");
	*outlen = outp;	
	memcpy(*outbuf, buffer, outp);
	return 0;
}

static int parse_msg(line_t *line, const char *in, char **outbuf, int *outlen) {
	(void) line; // silence
	*outlen = strlen(in);
	*outbuf = mem_alloc_str(in);

	return 0;
}

static const struct {
	const char *name;
	const int namelen;
	int (*parser)(line_t *line, const char *in, char **outbuf, int *outlen);
} cmds[] = {
	{ "expect", 6, parse_buf },
	{ "send", 4, parse_buf },
	{ "message", 7, parse_msg },
	{ "errmsg", 6, parse_msg },
	{ "init", 4, NULL },
	{ NULL, 0, NULL }
};

#define CMD_EXPECT	0
#define CMD_SEND	1
#define CMD_MESSAGE	2
#define CMD_ERRMSG	3
#define CMD_INIT	4


/**
 * parse the null-terminated input buffer into a line_t struct
 */
int parse_line(const char *buffer, int n, line_t **linep, int num) {
	*linep = NULL;

	int rv = 0;
	const char *p = buffer;
	char *outbuf = NULL;
	int outlen = 0;

	log_debug("parse_line(%s)\n", buffer);

	// leading space
	while (isspace(*p)) { p++; }

	if (*p == '#') {
		// ignore, comment
		return 0;
	}

	if (!(*p)) {
		return 0;
	}

	// find command
	int cmd = 0;
	int ch;
	while (cmds[cmd].name != NULL) {
		ch = 0;
		while ((cmds[cmd].name[ch] != 0) && (cmds[cmd].name[ch] == p[ch])) {
			ch++;
		}
		if ((cmds[cmd].name[ch] == 0) && ((p[ch] == 0) || isspace(p[ch]))) {
			// found
			break;
		}
		cmd++;
	}
	if (cmds[cmd].name == NULL) {
		log_error("Could not parse line %d ('%s') - did not find command!\n", n, buffer);
		return -1;
	}
	p += cmds[cmd].namelen;

	while (isspace(*p)) p++;
	
	line_t *line = mem_alloc(&line_type);
	line->cmd = cmd;
	line->num = num;

	if (cmds[cmd].parser != NULL) {
		rv = cmds[cmd].parser(line, p, &outbuf, &outlen);
		if (rv < 0) {
			mem_free(line);
			return -1;
		}
	}

	line->buffer = outbuf;
	line->length = outlen;
	
	*linep = line;
	return 0;
}

registry_t* load_script(FILE *fp) {

	registry_t *script = mem_alloc_c(sizeof(registry_t), "script");

	int rv = 0;
	int num = 1;
	ssize_t len = 0;
	size_t n;
	char *lineptr = NULL;
	line_t *line =NULL;

	reg_init(script, "script", 20);

	while ( (len = getline(&lineptr, &n, fp)) != -1) {

		rv = parse_line(lineptr, len, &line, num);

		if (rv < 0) {
			break;
		}
		if (line != NULL) {
			reg_append(script, line);
		}
		lineptr = NULL;
		num++;
	}

	if (feof(fp)) {
		return script;
	}

	log_errno("Error loading script");

	reg_free(script, free_line);
	mem_free(script);
	return NULL;
}

registry_t* load_script_from_file(const char *filename) {

	FILE *fp = NULL;

	if (!strcmp("-", filename)) {
		fp = stdin;
	} else {
		fp = fopen(filename, "r");
	}
	if (fp == NULL) {
		log_errno("Could not open script file '%s'", filename);
		return NULL;
	}

	registry_t* script = load_script(fp);

	if (fp != stdin) {
		fclose(fp);
	}

	return script;
}
 
// -----------------------------------------------------------------------

void execute_script(int sockfd, registry_t *script) {

	// current "pc" pointer to script line
	int curpos = 0;	
	line_t *line = NULL;

	int err = 0;
	ssize_t size = 0;
	ssize_t cnt = 0;
	line_t *errmsg = NULL;
	scriptlet_t *scr = NULL;
	char *buffer = NULL;
	char fs_sync = FS_SYNC;
	char inbyte = 0;

	while ( (err == 0) && (line = reg_get(script, curpos)) != NULL) {

		switch (line->cmd) {
		case CMD_MESSAGE:
			log_info("> %s\n", line->buffer);
			curpos++;
			break;
		case CMD_ERRMSG:
			errmsg = line;
			curpos++;
			break;
		case CMD_SEND:
			for (int i = 0; (scr = reg_get(&line->scriptlets, i)) != NULL; i++) {
				scr->exec(line, scr);
			}
			size = write(sockfd, line->buffer, line->length);
			if (size < 0) {
				log_errno("Error writing to socket at line %d\n", curpos);
				err = -1;
			}
			curpos++;
			break;
		case CMD_EXPECT:
			for (int i = 0; (scr = reg_get(&line->scriptlets, i)) != NULL; i++) {
				scr->exec(line, scr);
			}
			buffer = mem_alloc_c(line->length, "expect_buffer");
			size = 0;
			cnt = 0;
			while ((cnt = read(sockfd, buffer + size, line->length - size)) > 0) {
				size += cnt;
			}
			if (cnt < 0) {
				log_errno("Error reading from socket at line %d\n", curpos);
				err = -1;
			} else
			if (memcmp(line->buffer, buffer, line->length)) {
				if (errmsg != NULL) {
					log_error("> %d: %s\n", curpos, errmsg->buffer);
				} else {
					log_error("Detected mismatch at line %d\n", curpos);
				}
			}
			curpos++;
		case CMD_INIT:
			for (int i = 0; i < 128; i++) {
				size = write(sockfd, &fs_sync, 1);
			}
			do {
				size = read(sockfd, &inbyte, 1);
			} while ((size == 1) && (inbyte == FS_SYNC));
			curpos++;
			break;
		}
	}
}

// -----------------------------------------------------------------------

int main(int argc, char *argv[]) {

	const char *device = NULL;
	const char *scriptname = NULL;
	terminal_init();


        int i=1;
        while(i<argc && argv[i][0]=='-' && argv[i][1] != 0) {
          	switch(argv[i][1]) {
            	case '?':
                	assert_single_char(argv[i]);
                	usage(EXIT_SUCCESS);    /* usage() exits already */
                	break;
            	case 'd':
                	assert_single_char(argv[i]);
                	if (i < argc-1) {
                  		i++;
                  		device = argv[i];
                  		log_info("main: device = %s\n", device);
                	} else {
                  		log_error("-d requires <device> parameter\n");
                  		exit(1);
                	}
                	break;
		case 'D':
			set_verbose();
			break;
            	default:
                	log_error("Unknown command line option %s\n", argv[i]);
                	usage(1);
                	break;
          	}
          	i++;
        }

	// next parameter is the script name
	if (i >= argc) {
		log_error("Script name parameter missing!\n");
		return -1;
	}

	scriptname = argv[i];
	i++;

	registry_t *script = load_script_from_file(scriptname);

	if (script != NULL) {

		int sockfd = socket_open(device);
	
		if (sockfd >= 0) {

			execute_script(sockfd, script);
		}
	}

	
	return 0;
}

