/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012,2014 Andre Fachat, Nils Eilers

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

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

/*
 * translate the command numbers into names, and vice versa;
 * Note: NOT speed optimized!
 */

#include <string.h>

#include "wireformat.h"

const char *nameofcmd(int cmdno) {
	switch (cmdno) {
	case FS_TERM:		return "TERM";
	case FS_OPEN_RD:	return "OPEN_RD";
	case FS_OPEN_WR:	return "OPEN_WR";
	case FS_OPEN_RW:	return "OPEN_RW";
	case FS_OPEN_AP:	return "OPEN_AP";
	case FS_OPEN_OW:	return "OPEN_OW";
	case FS_OPEN_DR:	return "OPEN_DR";
	case FS_READ:		return "READ";
	case FS_WRITE:		return "WRITE";
	case FS_WRITE_EOF:	return "WRITE_EOF";
	case FS_REPLY:		return "REPLY";
	case FS_DATA:		return "DATA";
	case FS_DATA_EOF:	return "DATA_EOF";
	case FS_SEEK:		return "SEEK";
	case FS_CLOSE:		return "CLOSE";
	case FS_MOVE:		return "MOVE";
	case FS_DELETE:		return "DELETE";
	case FS_FORMAT:		return "FORMAT";
	case FS_CHKDSK:		return "CHKDSK";
	case FS_RMDIR:		return "RMDIR";
	case FS_MKDIR:		return "MKDIR";
	case FS_CHDIR:		return "CHDIR";
	case FS_ASSIGN:		return "ASSIGN";
	case FS_SETOPT:		return "SETOPT";
	case FS_RESET:		return "RESET";
	case FS_BLOCK:		return "BLOCK";
	case FS_POSITION:	return "POSITION";
	case FS_GETDATIM:	return "GETDATIM";
	case FS_CHARSET:	return "CHARSET";
	case FS_COPY:    	return "COPY";
	case FS_DUPLICATE: 	return "DUPLICATE";
	case FS_INITIALIZE: 	return "INITIALIZE";
	default:		return "???";
	}
}

int numofcmd(const char *name) {

	if (!strcmp("TERM", name)) 	return FS_TERM;
	if (!strcmp("OPEN_RD", name)) 	return FS_OPEN_RD;
	if (!strcmp("OPEN_WR", name)) 	return FS_OPEN_WR;
	if (!strcmp("OPEN_RW", name)) 	return FS_OPEN_RW;
	if (!strcmp("OPEN_AP", name)) 	return FS_OPEN_AP;
	if (!strcmp("OPEN_OW", name)) 	return FS_OPEN_OW;
	if (!strcmp("OPEN_DR", name)) 	return FS_OPEN_DR;
	if (!strcmp("READ", name)) 	return FS_READ;
	if (!strcmp("WRITE", name)) 	return FS_WRITE;
	if (!strcmp("WRITE_EOF", name)) return FS_WRITE_EOF;
	if (!strcmp("REPLY", name)) 	return FS_REPLY;
	if (!strcmp("DATA", name)) 	return FS_DATA;
	if (!strcmp("DATA_EOF", name)) 	return FS_DATA_EOF;
	if (!strcmp("SEEK", name)) 	return FS_SEEK;
	if (!strcmp("CLOSE", name)) 	return FS_CLOSE;
	if (!strcmp("MOVE", name)) 	return FS_MOVE;
	if (!strcmp("DELETE", name)) 	return FS_DELETE;
	if (!strcmp("FORMAT", name)) 	return FS_FORMAT;
	if (!strcmp("CHKDSK", name)) 	return FS_CHKDSK;
	if (!strcmp("RMDIR", name)) 	return FS_RMDIR;
	if (!strcmp("MKDIR", name)) 	return FS_MKDIR;
	if (!strcmp("CHDIR", name)) 	return FS_CHDIR;
	if (!strcmp("ASSIGN", name)) 	return FS_ASSIGN;
	if (!strcmp("SETOPT", name)) 	return FS_SETOPT;
	if (!strcmp("RESET", name)) 	return FS_RESET;
	if (!strcmp("BLOCK", name)) 	return FS_BLOCK;
	if (!strcmp("POSITION", name)) 	return FS_POSITION;
	if (!strcmp("GETDATIM", name)) 	return FS_GETDATIM;
	if (!strcmp("CHARSET", name)) 	return FS_CHARSET;
	if (!strcmp("COPY", name)) 	return FS_COPY;
	if (!strcmp("DUPLICATE", name)) return FS_DUPLICATE;
	if (!strcmp("INITIALIZE", name)) return FS_INITIALIZE;

	return -1;
}


