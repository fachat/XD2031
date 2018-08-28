/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat, Nils Eilers

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301, USA.

****************************************************************************/

/**
 * This file contains the file name parser
 */

#define	DEBUG_NAME

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h>

#include "name.h"
#include "cmdnames.h"
#include "archcompat.h"

#ifdef FIRMWARE
#include "debug.h"
#endif

#ifdef SERVER
#include "openpars.h"
#endif

#if defined(DEBUG_NAME)


static void dump_result(nameinfo_t *result) {
	printf("CMD      =%s\n", command_to_name(result->cmd));
	printf("DRIVE    =%c\n", result->trg.drive == NAMEINFO_UNUSED_DRIVE ? '-' :
				(result->trg.drive == NAMEINFO_UNDEF_DRIVE ? '*' :
				(result->trg.drive == NAMEINFO_LAST_DRIVE ? 'L' :
				result->trg.drive + 0x30)));
	printf("DRIVENAME='%s'\n", result->trg.drivename ? (char*) result->trg.drivename : "<>");
	printf("NAME     ='%s' (%d)\n", result->trg.name ? (char*)result->trg.name : "<>", result->trg.namelen);
	printf("ACCESS   =%c\n", result->access ? result->access : '-');
	printf("TYPE     =%c\n", result->pars.filetype ? result->pars.filetype : '-'); 
	printf("RECLEN   =%d\n", result->pars.recordlen);
	for (int i = 0; i < MAX_NAMEINFO_FILES; i++) {
		printf("%d.DRIVE    =%c\n", i, result->file[i].drive == NAMEINFO_UNUSED_DRIVE ? '-' :
				(result->file[i].drive == NAMEINFO_UNDEF_DRIVE ? '*' :
				(result->file[i].drive == NAMEINFO_LAST_DRIVE ? 'L' :
				result->file[i].drive + 0x30)));
		printf("%d.DRIVENAME='%s'\n", i, result->file[i].drivename ? (char*) result->file[i].drivename : "<>");
		printf("%d.NAME     ='%s' (%d)\n", i, result->file[i].name ? (char*)result->file[i].name : "<>", 
				result->file[i].namelen);
	}
	//debug_flush();
}
#else
#define dump_result(x) do {} while (0)
#endif

// shared global variable to be used in parse_filename, as long as it's threadsafe
//nameinfo_t nameinfo;

/** @brief Extract drive from filename
 *
 * Function changes the in-string to zero-terminate the elements.
 * A provider name must not end with a digit to be able to detect numeric drives.
 *
 * @param[in]  in         Single filename with may be preceded by a (named) drive
 * @param[out] out        drive_and_name_t struct filled
 * @returns    commap     pointer to next comma if exists, overwritten by zero
 */
static uint8_t* parse_drive (uint8_t *in, drive_and_name_t *out) {
	uint8_t *p = (uint8_t *)strchr((char*) in, ':');
	uint8_t *c = (uint8_t *)strchr((char*) in, ',');
	uint8_t   r = NAMEINFO_UNUSED_DRIVE;    // default if no colon found
	out->drivename = NULL;
	out->name  = in;
	uint8_t len;

	if (p && (c == NULL || c > p)) { // there is a colon (not after a comma)
		*p = 0;             // zero-terminate drive name
		out->name = p + 1;  // filename with drive stripped

		if (p == in) r = NAMEINFO_LAST_DRIVE;          // return filename without ':'
		else {
			r = strtol((char *)in,(char **)&p,10);
			if (*p || r > 9) {            // named drive
				out->drivename = in;
				r = NAMEINFO_UNDEF_DRIVE;
			}
		}
	}
	if (c) {
		// comma is end of filename, overwrite comma with string terminator
		*c = 0;
		// point to char after comma
		c++;
	}
	// Drop CR if appended to filename
	len = strlen((char *)out->name);
	if (out->name[len - 1] == 13) {
		out->name[--len] = 0;
	}
	out->drive = r;
	out->namelen  = len;
	return c;
}

/** @brief Parse command and fill result in a way the specific command expects it
 *
 * - Some commands parse the raw string on their own
 * - Some commands expect every parameter in result.name
 * - Some commands use result.name and result.file[0].name (ASSIGN, RENAME)
 * - COPY may use 1 target file name and up to 4 source file names
 *
 * @param[in]  cmdstr Raw command string
 * @param[in]  len    Length of command string
 * @param[out] result Filled with zero, one to two names
 */
void parse_cmd_pars (uint8_t *cmdstr, uint8_t len, nameinfo_t *result) {
	uint8_t cmdlen = 0;

	int driveno = NAMEINFO_UNUSED_DRIVE;

	result->num_files = 0; // Initialize # of secondary files

	// skip whitespace if any
	while(isspace(cmdstr[cmdlen])) ++cmdlen;

	// the position command is fully binary
	if (result->cmd == CMD_POSITION || result->cmd == CMD_TIME) {
		result->trg.name = cmdstr + cmdlen;
		result->trg.namelen = len - cmdlen;
		return;
	}
	// check for disk copy "Dt=s" or "Ct=s"
	if ((result->cmd == CMD_DUPLICATE || result->cmd == CMD_COPY) &&
			cmdstr[cmdlen+1] == '='  && cmdstr[cmdlen+3] == 0 &&
			isdigit(cmdstr[cmdlen])  && isdigit(cmdstr[cmdlen+2])) {
		result->trg.drive = cmdstr[cmdlen  ] & 15; 	// target drive
		result->file[0].drive = cmdstr[cmdlen+2] & 15; 	// source drive
		result->num_files = 1;
		return;
	}

	uint8_t *sep = cmdstr + cmdlen;
	if (result->cmd == CMD_ASSIGN || result->cmd == CMD_RENAME || result->cmd == CMD_COPY) {
		// Split cmdstr at '=' for file[0].name
		// and at ',' for more file names
		sep = (uint8_t*) strchr((char*) cmdstr, '=');
		if (sep) {
			*sep = 0;
			sep++;
			// note: if there is a comma, it should probably be a syntax error
			// but we currently just ignore it (we could add options here!)
			parse_drive (cmdstr+cmdlen, &result->trg);
			if (result->trg.drive != NAMEINFO_UNDEF_DRIVE) {
				driveno = result->trg.drive;
			}
		} else {
			// no "=" for assign, rename or copy means syntax error 
			result->cmd = CMD_SYNTAX;
			return;
		}
	}

      	uint8_t i = 0;
	while (sep && i < MAX_NAMEINFO_FILES) {
		sep = parse_drive (sep, &result->file[i]);
		if (result->file[i].drive == NAMEINFO_UNUSED_DRIVE 
			|| result->file[i].drive == NAMEINFO_LAST_DRIVE) {
			result->file[i].drive = driveno;
		}
		++i;
	}
	result->num_files = i;

	if (sep) {
		// too many file names
		result->cmd = CMD_SYNTAX;
		return;
	}
#if 0
	// set source drives to target drive if not specified
	for (uint8_t i=0 ; i < result->num_files ; ++i) {
		if (result->file[i].drive > 9) result->file[i].drive = result->trg.drive;
	}
#endif
	dump_result(result);
}


static void parse_open (uint8_t *filename, uint8_t load, uint8_t len, nameinfo_t *result) {
	uint8_t *p;

	if (load && *filename == '$') { // load directory e.g. '$' '$:' '$d' '$d:' ...
        	if (isdigit(filename[1]) && !filename[2]) {
			strcat((char *)filename,":*");
		}
		result->cmd  = CMD_DIR;
		uint8_t *c = parse_drive (filename+1, &result->trg);
		if (c) {
			// here we could add parsing of options, e.g. filtering a directory for file types
		}
		return;
	}

	if (filename[0] == '@') {			// SAVE with replace
		result->cmd = CMD_OVERWRITE;
		filename++;
	}

	p = parse_drive (filename, &result->trg);

	// Process options that may follow comma separated after the filename
	// In general, options can be used in any order, but each type only once.
	// Exception: L has to be the last option
	// Long form (e.g. SEQ,WRITE instead of S,W) checks only first character.

	// loop as long as we have options and not yet received the L option
	while (p && *p && result->pars.filetype != 'L') {
		switch(*p) {
		// file type options
		case 'L':
			// CBM is single byte format, but we "integrate" the closing null-byte
			// to allow two byte record lengths
			result->pars.recordlen = p[1] + (p[2] << 8);
			// falls through
		case 'P':
			// falls through
		case 'S':
			// falls through
		case 'U':
			if (result->pars.filetype) {
				result->cmd = CMD_SYNTAX;
				return;
			}
			result->pars.filetype = *p;
			break;
		// file access options
		case 'R':
			// falls through
		case 'W':
			// falls through
		case 'A':
			// falls through
		case 'X':
			if (result->access) {
				result->cmd = CMD_SYNTAX;
				return;
			}
			result->access = *p;
			break;
		// other options
		case 'N':
			result->options |= NAMEOPT_NONBLOCKING;
			break;
		default:
			result->cmd = CMD_SYNTAX;
			return;
		}
		p++;
		if (*p && *p != ',') {
			result->cmd = CMD_SYNTAX;
			return;
		}
		p++;
	}

	if (!result->access) {
		// if access is not set on a REL file, it's read & write
		if (result->pars.filetype == 'L') {
			result->access = 'X';
		}
	}
}

/*
 * This function does the initialization common for
 * - parse_cmd  (commands)
 * - parse_open (file names for OPEN and DIRECTORY)
 * Afterwards the appropriate function is called.
 *
 * Note that some functionality relies on that the resulting name pointer is
 * within the cmd_t command buffer! So the result has to be taken from "in place"
 * of what has been given
 *
 * To distinguish numeric drive numbers from unassigned (undefined) drives like "ftp:",
 * the provider name must not end with a digit.
 *
 */
void parse_filename(uint8_t *in, uint8_t dlen, uint8_t inlen, nameinfo_t *result, uint8_t parsehint) {

	// does not include a trailing zero-byte
	uint8_t len = dlen;

	result->access = 0;
	result->pars.filetype = 0;

	// copy over command to the end of the buffer, so we can
	// construct it from the parts at the beginning after parsing it
	// (because we may need to insert bytes at some places, which would
	// be difficult)
	// Note that assembling takes place in assemble_filename_packet below.
	uint8_t diff = inlen - len;
	memmove(in + diff, in, len);

	// runtime vars (uint e.g. to avoid sign extension on REL file record len)
	uint8_t *p = in + diff;

	// init output
	memset(result, 0, sizeof(*result));
	result->trg.drive  = NAMEINFO_UNUSED_DRIVE;
	for (uint8_t i=0; i < MAX_NAMEINFO_FILES ; ++i) {
		result->file[i].drive = NAMEINFO_UNUSED_DRIVE;
	}
	result->cmd    = CMD_NONE;	// no command

	if (parsehint & PARSEHINT_COMMAND) {
		uint8_t cmdlen = 0;
		result->cmd = command_find(p, &cmdlen);
		if (result->cmd == CMD_SYNTAX) {
			return;
		}
		parse_cmd_pars(p+cmdlen, len-cmdlen, result);
	} else {
		result->trg.name = p;	// full name
		result->trg.namelen = len;	// default
		parse_open(p, parsehint & PARSEHINT_LOAD, len, result);
		dump_result(result);
	}
}


static void* assemble_filename(uint8_t *p, drive_and_name_t *name) {

	// drive
	*p++ = name->drive;
	// drive provider name (if NAMEINFO_UNDEF_DRIVE)
	if (name->drivename != NULL && *name->drivename) {
		int len = strlen((char*)name->drivename);
		memmove (p, name->drivename, len);
		p += len;
		*p++ = ':';	// ':' instead of \0, so parseable
	}
	// copy filename including terminating null
	memmove (p, name->name, name->namelen + 1);
	p += name->namelen + 1;

	return p;
}

/**
 * assembles the filename packet from nameinfo into the target buffer.
 * For this it is essential, that nameinfo content does not overlap
 * (at least in the order of assembly) with the target buffer.
 * That is why the command_buffer content is moved to the end of the
 * command_buffer in parse_filename - so it can be re-assembled in the
 * beginning of the command_buffer.
 *
 * it returns the number of bytes in the buffer
 *
 * TODO: check for buffer overflow
 */
uint8_t assemble_filename_packet(uint8_t *trg, nameinfo_t *nameinfo) {

	uint8_t *p = trg;
	uint8_t i;


	if ((nameinfo->cmd == CMD_DUPLICATE || nameinfo->cmd == CMD_COPY) &&
		nameinfo->trg.name == NULL) { // disk copy
		*p++ = nameinfo->trg.drive;
		*p++ = '*'; *p++ = 0;
      		*p++ = nameinfo->file[0].drive;
		*p++ = '*'; *p   = 0;
		return 6; // target,"*",source,"*"
	}

	// option string first
	if (nameinfo->pars.filetype) {
		// parameters are comma-separated lists of "<name>'='<values>", e.g. "T=S"
		*p++ = 'T';
		*p++ = '=';
		*p++ = nameinfo->pars.filetype;
		if (nameinfo->pars.recordlen > 0) {
			sprintf((char*)p, "%d", nameinfo->pars.recordlen);
			p += strlen((char*)p);
		}
	}
	// terminate string even if no options
	*p++ = 0;

	// do we have a target string?
	if (nameinfo->trg.name) {
		p = assemble_filename(p, &nameinfo->trg);
	}

	// Secondary filename(s). RENAME has one source filename. SCRATCH accepts more
	// than the original file pattern.
	// COPY accepts up to 4 comma separated source file names for merging
	for (i = 0; i < nameinfo->num_files ; ++i) {
		// two or more file names
		p = assemble_filename(p, &nameinfo->file[i]);
	}

	return p-trg;
}

#ifdef SERVER

/**
 * src is the pointer to the drive/filename data.
 * len is in/out: in is the rest length of the packet, out is the number of 
 * consumed bytes
 */
static cbm_errno_t filename_from_packet(uint8_t *src, uint8_t *len, drive_and_name_t *name) {

	uint8_t p = 1;
	uint8_t l = *len;

	if (l < 2) {
		// at least drive and terminating zero
		return CBM_ERROR_FAULT;
	}

	name->drivename = NULL;
	name->name = NULL;
	name->drive = *src++;

	p += strnlen(src, l-p);
	if(p >= l) {
		// no terminating zero within buffer
		return CBM_ERROR_FAULT;
	}

	if (name->drive == NAMEINFO_UNDEF_DRIVE) {
		char *cp = strchr(src, ':');
		if (cp) {
			*cp = 0;
			name->drivename = src;
			// after the inserted zero
			cp++;
			src = cp;
		}
	}

	name->name = src;
	name->namelen = strlen(name->name);

	// points to the byte after the terminating zero
	*len = p+1;
	return CBM_ERROR_OK;
}

/**
 * dis-assembles a filename packet back into a nameinfo struct.
 * (Note: only available on the server).
 *
 * cmd, access, and options are not set.
 *
 * Returns error code, most specifically SYNTAX codes if the packet
 * cannot be parsed.
 */
cbm_errno_t parse_filename_packet(uint8_t * src, uint8_t len, openpars_t *pars, drive_and_name_t *names, int *num_files) {

	uint8_t l = len;

	openpars_init_options(pars);

	for (int i = 0; i < *num_files; i++) {
		names[i].name = NULL;
		names[i].drivename = NULL;
		names[i].drive = NAMEINFO_UNDEF_DRIVE;
	}

	// parse options
	l = strnlen(src, len);
	if (l >= len) {
		return CBM_ERROR_FAULT;
	}
	// parse options
	openpars_process_options(src, pars);
	// move on to other filenames
	src += l+1;
	len -= l+1;

	// parse the files
	int i;
	for (i = 0; len > 0 && i < *num_files; i++) {
		l = len;
		if (filename_from_packet(src, &l, &names[i]) != CBM_ERROR_OK) {
			return CBM_ERROR_FAULT;
		}
		src += l;
		len -= l;
	}
	*num_files = i;

	if (len != 0) {
		return CBM_ERROR_FAULT;
	}

	return CBM_ERROR_OK;
}


#endif

