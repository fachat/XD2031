
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


/** parse a string */
const char* parse_string(const char *inp, char *out, int *outlen);

/** parse a hex byte */
const char* parse_hexbyte(const char *inp, char *out);

// scriptlet (within a parsed line)

typedef struct _scriptlet scriptlet_t;

struct _scriptlet {
	int 		type;
	int		pos;
	int		(*exec)(line_t *line, scriptlet_t *scr);
};

// parse a buffer line (i.e. a series of hex numbers and strings, possibly with scriplets)
// return length of out bytes
int scr_len(line_t *line, scriptlet_t *scr);

int scr_dsb(char *trg, int trglen, const char **parseptr);


typedef struct {
	const char *name;
	const int namelen;
	const int outlen;
	int (*parse)(char *trg, int trglen, const char **parseptr);
	int (*exec)(line_t *line, scriptlet_t *scr);
} scriptlet_tab_t;

extern scriptlet_tab_t scriptlets[];

/*
// scriplets
static const struct {
	const char *name;
	const int namelen;
	const int outlen;
	int (*parse)(char *trg, int trglen, const char **parseptr);
	int (*exec)(line_t *line, scriptlet_t *scr);
} scriptlets[] = {
	{ "len", 3, 1, NULL, scr_len },
	{ "dsb", 3, 0, scr_dsb, NULL },
	{ "talk", 4, 1, scr_talk, NULL },
	{ "listen", 6, 1, scr_listen, NULL },
	{ "secondary", 9, 1, scr_secondary, NULL },
	{ "untalk", 6, 1, scr_untalk, NULL },
	{ "unlisten", 8, 1, scr_unlisten, NULL },
	{ NULL, 0, 0, NULL, NULL }
};
*/

int parse_buf(line_t *line, const char *in, char **outbuf, int *outlen);

int parse_msg(line_t *line, const char *in, char **outbuf, int *outlen);

typedef struct {
	const char *name;
	const int namelen;
	int (*parser)(line_t *line, const char *in, char **outbuf, int *outlen);
} cmd_tab_t;

extern cmd_tab_t cmds[];

#if 0
static const struct {
	const char *name;
	const int namelen;
	int (*parser)(line_t *line, const char *in, char **outbuf, int *outlen);
} cmds[] = {
	{ "atn", 3, parse_buf },
	{ "send", 4, parse_buf },
	{ "recv", 4, parse_buf },
	{ "errmsg", 6, parse_msg },
	{ "message", 7, parse_msg },
	{ "expect", 6, parse_buf },
	{ "sendnoeof", 9, parse_buf },
	{ NULL, 0, NULL }
};

#endif

registry_t* load_script_from_file(const char *filename);
 
#endif

