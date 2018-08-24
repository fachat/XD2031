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

#undef	DEBUG_NAME

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h>

#include "name.h"
#include "cmdnames.h"
#include "archcompat.h"

#ifdef SERVER
#include "openpars.h"
#endif

#ifdef FIRMWARE
#include "debug.h"
#endif

#if defined(DEBUG_NAME) || defined(PCTEST)
static void dump_result(nameinfo_t *result) {
	printf("CMD=%s\n", command_to_name(result->cmd));
	printf("DRIVE=%c\n", result->trg.drive == NAMEINFO_UNUSED_DRIVE ? '-' :
				(result->trg.drive == NAMEINFO_UNDEF_DRIVE ? '*' :
				(result->trg.drive == NAMEINFO_LAST_DRIVE ? 'L' :
				result->trg.drive + 0x30)));
	printf("DRIVENAME='%s'\n", result->trg.drivename ? (char*) result->trg.drivename : nullstring);
	printf("NAME='%s' (%d)\n", result->trg.name ? (char*)result->trg.name : nullstring, result->trg.namelen);
	printf("ACCESS=%c\n", result->access ? result->access : '-');
	printf("TYPE=%c", result->pars.filetype ? result->pars.filetype : '-'); debug_putcrlf();
	printf("DRIVE2=%c\n", result->file[0].drive == NAMEINFO_UNUSED_DRIVE ? '-' :
				(result->file[0].drive == NAMEINFO_UNDEF_DRIVE ? '*' :
				(result->file[0].drive == NAMEINFO_LAST_DRIVE ? 'L' :
				result->file[0].drive + 0x30)));
	printf("DRIVENAME2='%s'\n", result->file[0].drivename ? (char*) result->file[0].drivename : nullstring);
	printf("NAME2='%s' (%d)\n", result->file[0].name ? (char*)result->file[0].name : nullstring, result->file[0].namelen);
	printf("RECLEN=%d\n", result->pars.recordlen);
	debug_flush();
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
 * @param[out] filename   Filename without preceding drive
 * @param[out] drivename  Name of drive (e.g. FTP), otherwise NULL
 * @returns    drive      Number or NAMEINFO_*_DRIVE defined in wireformat.h
 */
static uint8_t parse_drive (uint8_t *in, uint8_t **filename, uint8_t *namelen, uint8_t **drivename) {
	uint8_t *p = (uint8_t *)strchr((char*) in, ':');
	uint8_t *c = (uint8_t *)strchr((char*) in, ',');
	uint8_t   r = NAMEINFO_UNUSED_DRIVE;    // default if no colon found
	*drivename = NULL;
	*filename  = in;
	uint8_t len;

	if (p && (c == NULL || c > p)) { // there is a colon (not after a comma)
		*p = 0;             // zero-terminate drive name
		*filename = p + 1;  // filename with drive stripped

		if (p == in) r = NAMEINFO_LAST_DRIVE;          // return filename without ':'
		else {
			r = strtol((char *)in,(char **)&p,10);
			if (*p || r > 9) {            // named drive
				*drivename = in;
				r = NAMEINFO_UNDEF_DRIVE;
			}
		}
	}
	// Drop CR if appended to filename
	len = strlen((char *)*filename);
	if (*(*filename + len - 1) == 13) *(*filename + --len) = 0;
	*namelen  = len;
	return r;
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
static void parse_cmd (uint8_t *cmdstr, uint8_t len, nameinfo_t *result) {
	uint8_t cmdlen;

	result->num_files = 0; // Initialize # of secondary files
	result->cmd = command_find(cmdstr, &cmdlen);
	if (result->cmd == CMD_SYNTAX) return;

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
		result->trg.drive         = cmdstr[cmdlen  ] & 15; // target drive
		result->file[0].drive = cmdstr[cmdlen+2] & 15; // source drive
		return;
	}
	if (result->cmd == CMD_ASSIGN || result->cmd == CMD_RENAME || result->cmd == CMD_COPY) {
		// Split cmdstr at '=' for file[0].name
		// and at ',' for more file names
      uint8_t i = 0;
		uint8_t *sep = (uint8_t*) strchr((char*) cmdstr, '=');
		while (sep && i < MAX_NAMEINFO_FILES) {
			*sep = 0;
			result->file[i].drive = parse_drive (sep+1, &result->file[i].name,
			&result->file[i].namelen, &result->file[i].drivename);
			sep = (uint8_t*) strchr((char*) result->file[i].name, ',');
			// Correct length of filename if comma separator found
			if (sep) result->file[i].namelen = sep - result->file[i].name;
			++i;
		}
		result->num_files = i;
	}
	result->trg.drive = parse_drive (cmdstr+cmdlen, &result->trg.name, &result->trg.namelen, &result->trg.drivename);
	// set source drives to target drive if not specified
	for (uint8_t i=0 ; i < result->num_files ; ++i) {
		if (result->file[i].drive > 9) result->file[i].drive = result->trg.drive;
	}
}


static void parse_open (uint8_t *filename, uint8_t load, uint8_t len, nameinfo_t *result) {
	uint8_t *p;

	if (load && *filename == '$') { // load directory e.g. '$' '$:' '$d' '$d:' ...
        	if (isdigit(filename[1]) && !filename[2]) {
			strcat((char *)filename,":*");
		}
		result->cmd  = CMD_DIR;
		result->trg.drive = parse_drive (filename+1, &result->trg.name, &result->trg.namelen, &result->trg.drivename);
		return;
	}

	if (filename[0] == '@') {			// SAVE with replace
		result->cmd = CMD_OVERWRITE;
		filename++;
	}

	result->trg.drive = parse_drive (filename, &result->trg.name, &result->trg.namelen, &result->trg.drivename);

	// Process options that may follow comma separated after the filename
	// In general, options can be used in any order, but each type only once.
	// Exception: L has to be the last option
	// Long form (e.g. SEQ,WRITE instead of S,W) checks only first character.
	const char file_options[] = "PSULRWAXN";
	char opt[3] = ",?";

	for (uint8_t i=0 ; i < sizeof(file_options)-1 ;  ++i) {
		opt[1] = file_options[i];  // modify search string (",R" ",P" etc.)
		if ((p = (uint8_t *)strstr((char *)result->trg.name,opt))) {
			if (p[1] == 'N') result->options |= NAMEOPT_NONBLOCKING;  // N
			else if (i < 4)  result->pars.filetype     = file_options[i];      // PSUL
			else             result->access   = file_options[i];      // RWAX
			if (p[1] == 'L') {
				// CBM is single byte format, but we "integrate" the closing null-byte
				// to allow two byte record lengths
				result->pars.recordlen = p[3] + (p[4] << 8);
			}
		}
	}

	if (!result->access) {
		// if access is not set on a REL file, it's read & write
		if (result->pars.filetype == 'L') {
			result->access = 'X';
		}
	}
	if ((p = (uint8_t *)strchr((char *)result->trg.name,','))) {
		*p = 0; // cut off options
	}
	result->trg.namelen = strlen((char *)result->trg.name);
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
		parse_cmd(p, len, result);
	} else {
		result->trg.name = p;	// full name
		result->trg.namelen = len;	// default
		parse_open(p, parsehint & PARSEHINT_LOAD, len, result);
	}
	dump_result(result);
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
	uint8_t len;
	uint8_t i;


	if ((nameinfo->cmd == CMD_DUPLICATE || nameinfo->cmd == CMD_COPY) &&
		nameinfo->trg.name == NULL) { // disk copy
		*p++ = nameinfo->trg.drive;
		*p++ = '*'; *p++ = 0;
      		*p++ = nameinfo->file[0].drive;
		*p++ = '*'; *p   = 0;
		return 6; // target,"*",source,"*"
	}

	p = assemble_filename(p, &nameinfo->trg);

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
	// terminate even if no options
	*p++ = 0;

	// Secondary filename(s). RENAME has one source filename. SCRATCH accepts more
	// than the original file pattern.
	// COPY accepts up to 4 comma separated source file names for merging
	for (i=0 ; i < nameinfo->num_files ; ++i) {
		// two or more file names
		p = assemble_filename(p, &nameinfo->file[i]);
	}

	return p-trg;
}

#ifdef SERVER

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
cbm_errno_t parse_filename_packet(uint8_t * src, uint8_t len, nameinfo_t * nameinfo) {

	uint8_t l = len;

	nameinfo->num_files = 0;
	for (int i = 0; i < MAX_NAMEINFO_FILES; i++) {
		nameinfo->file[i].name = NULL;
		nameinfo->file[i].drivename = NULL;
		nameinfo->file[i].drive = NAMEINFO_UNDEF_DRIVE;
	}

	openpars_init_options(&nameinfo->pars);

	if (filename_from_packet(src, &l, &nameinfo->trg) == CBM_ERROR_OK) {
		src += l;
		len -= l;
	} else {
		return CBM_ERROR_FAULT;
	}

	// allow for filename without options
	if (len > 0) {
		// got some options
		l = strnlen(src, len);
		if (l >= len) {
			return CBM_ERROR_FAULT;
		}

		// parse options
		openpars_process_options(src, &nameinfo->pars);

		// parse potential other filenames
		src += l+1;
		len -= l+1;
	}

	int i;
	for (i = 0; len > 0 && i < MAX_NAMEINFO_FILES; i++) {
		l = len;
		if (filename_from_packet(src, &l, &nameinfo->file[i]) != CBM_ERROR_OK) {
			return CBM_ERROR_FAULT;
		}
	}
	nameinfo->num_files = i;

	if (len != 0) {
		return CBM_ERROR_FAULT;
	}

	return CBM_ERROR_OK;
}


#endif

