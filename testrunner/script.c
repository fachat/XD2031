
/****************************************************************************

    xd2031 filesystem server - socket test runner script parser
    Copyright (C) 2014 Andre Fachat

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
 * script.c
 *
 * Part of the xd2031 file system server
 * 
 * this code connects to a Unix socket and uses a script to send
 * commands, resp. receive replies and check whether they are correct
 */

#include <ctype.h>
#include <string.h>

#include "mem.h"
#include "log.h"
#include "registry.h"
#include "script.h"
#include "cmdnames.h"


// -----------------------------------------------------------------------
// script handling

/** 
 * read script, and build up memory structure
 */

// line of a script


static void line_init(const type_t *type, void *obj) {
	(void) type; // silence
	line_t *line = (line_t*) obj;

	line->buffer = NULL;
	line->mask = NULL;
	line->length = 0;

	reg_init(&line->scriptlets, "line_scriptlets", 2);
}

static type_t line_type = {
	"script_line",
	sizeof(line_t),
	line_init
};


/** parse a string */
const char* parse_string(const char *inp, char *out, int *outlen) {

	char delim = *inp;
	const char *p = inp + 1;
	int l = 0;

	while ((*p) != 0 && (*p) != delim) {
		out[l] = *p;
		l++;
		p++;
	}

	if ((*p) == 0) {
		// string not delimited
		log_error("String not delimited: %s\n", inp);
		return NULL;
	}
	p++;
	*outlen = l;
	return p;
}


/** parse a hex byte */
const char* parse_hexint(const char *inp, int *out) {
	
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

	*out = val;
	return p;
}

/** parse a hex byte */
const char* parse_hexbyte(const char *inp, char *out) {
	
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

// scriptlet (within a parsed line)

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
// return length of out bytes

int exec_len(line_t *line, scriptlet_t *scr) {

	line->buffer[scr->pos] = line->length;

	return 1;
}

int exec_ign(line_t *line, scriptlet_t *scr) {

	line->mask[scr->pos] = 0;

	return 1;
}

int scr_dsb(char *trg, int trglen, const char **parseptr, int *outparam) {

        (void) outparam; // silence warning

	int len = 0;
	char fill = 0;
	const char *p = *parseptr;

	while (isspace(*p)) { p++; }

	p = parse_hexint(p, &len);
	if (p != NULL) {	

		while (isspace(*p)) { p++; }

		if (*p == ',') {
			// fillbyte is given
			p++;

			while (isspace(*p)) { p++; }

			p = parse_hexbyte(p, &fill);
		}
	}
	*parseptr = p;

	if (trglen < len) {
		log_error("dsb has fill %d larger than remaining buffer %d\n", len, trglen);
		return -1;
	}

	memset(trg, fill, len);

	return len;
}

int scr_ignore(char *trg, int trglen, const char **parseptr, int *outparam) {

	int len = 0;
	const char *p = *parseptr;

	while (isspace(*p)) { p++; }

	p = parse_hexint(p, &len);
	*parseptr = p;

	if (trglen < len) {
		log_error("dsb has fill %d larger than remaining buffer %d\n", len, trglen);
		return -1;
	}

	memset(trg, 0, len);

	*outparam = len;

	return len;
}

int exec_ignore(line_t *line, scriptlet_t *scr) {
	(void)line; // silence

	return scr->param;
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


int parse_buf(line_t *line, const char *in, char **outbuf, int *outlen) {

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
		if ((*p) == '\"' || (*p) == '\'') {
			int outlen = 0;
			p = parse_string(p, buffer + outp, &outlen);
			if (p == NULL) {
				log_error("Error parsing line %d\n", line->num);
				return -1;
			}
			outp += outlen;
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
			if (scriptlets[cmd].parse != NULL) {
				int l = scriptlets[cmd].parse(buffer + outp, sizeof(buffer)-outp, &p, &scr->param);
				if (l < 0) {
					break;
				}
				outp += l;
			} else {
				outp += scriptlets[cmd].outlen;
			}
		} else
		if ((*p) == ':') {
			// find constant value
			p++;
			if (p[0] == 'F' && p[1] == 'S' && p[2] == '_') {
				int l = 3; while (isalnum(p[l]) || p[l]=='_') { l++; };
				const char *constname = mem_alloc_strn(p, l);
				int cmdno = numofcmd(constname+3);
				p += l;
				if (cmdno < 0) {
					log_error("Could not parse constant ('%s') as command!\n", constname);
				} else {
					buffer[outp++] = (char)cmdno;	
					
				}
				mem_free(constname);
			} else {
				log_error("Could not parse constant ('%s')!\n", p);
			}
		} else
		if ((*p) == 0) {
			break;
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

int parse_msg(line_t *line, const char *in, char **outbuf, int *outlen) {
	(void) line; // silence
	*outlen = strlen(in);
	*outbuf = mem_alloc_str(in);

	return 0;
}


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
		// comment
		line_t *line = mem_alloc(&line_type);
		line->cmd = CMD_COMMENT;
		line->num = num;
		line->length = strlen(p);
		line->buffer = mem_alloc_str(p);
		*linep = line;
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

		// remove CR/LF at end of line
		while (len > 0 && (lineptr[len-1] == '\n' || lineptr[len-1] == '\r')) {
			lineptr[--len] = 0;
		}

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
 

