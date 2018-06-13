
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

#ifndef _SCRIPT_H
#define _SCRIPT_H

// -----------------------------------------------------------------------
// script handling

#define	CMD_COMMENT	(-1)

/** 
 * read script, and build up memory structure
 */

// line of a script

typedef struct {
	int cmd;		// command for the line
	int num;
	char *buffer;		// pointer to malloc'd line buffer
	char *mask;		// mask for compare (if set)
	int length;		// length of buffer (amount of data in it, NOT capacity)
	registry_t scriptlets;
} line_t;

/** parse a string */
const char *parse_string(const char *inp, char *out, int *outlen);

/** parse a hex byte */
const char *parse_hexbyte(const char *inp, char *out);
/** parse a hex integer */
const char *parse_hexint(const char *inp, int *out);

// scriptlet (within a parsed line)

typedef struct _scriptlet scriptlet_t;

struct _scriptlet {
	int type;
	int pos;
	int (*exec) (line_t * line, scriptlet_t * scr);
	int param;		// depends on command
};

// parse a buffer line (i.e. a series of hex numbers and strings, possibly with scriplets)
// return length of out bytes
int exec_len(line_t * line, scriptlet_t * scr);
int exec_ign(line_t * line, scriptlet_t * scr);

int scr_dsb(char *trg, int trglen, const char **parseptr, int *outparam);

int scr_ignore(char *trg, int trglen, const char **parseptr, int *outparam);
int exec_ignore(line_t * line, scriptlet_t * scr);

typedef struct {
	const char *name;
	const int namelen;
	const int outlen;
	int (*parse) (char *trg, int trglen, const char **parseptr,
		      int *outparam);
	int (*exec) (line_t * line, scriptlet_t * scr);
} scriptlet_tab_t;

extern scriptlet_tab_t scriptlets[];

int parse_buf(line_t * line, const char *in, char **outbuf, int *outlen);

int parse_msg(line_t * line, const char *in, char **outbuf, int *outlen);

typedef struct {
	const char *name;
	const int namelen;
	int (*parser) (line_t * line, const char *in, char **outbuf,
		       int *outlen);
} cmd_tab_t;

extern cmd_tab_t cmds[];

registry_t *load_script_from_file(const char *filename);

#endif
