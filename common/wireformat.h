/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

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

#ifndef WIREFORMAT_H
#define WIREFORMAT_H

#include "errors.h"		// pull in the error numbers

/* data struct exchanged between client and server */
#define FSP_CMD         0	/* command, see the FS_* defines below */
#define FSP_LEN         1	/* total packet length, i.e. including CMD and LEN */
#define FSP_FD          2	/* channel that packet is sent for */
#define FSP_DATA        3	/* first payload data byte */

// reserved file descriptors 
// Note: -1 = 0xff is reserved
#define	FSFD_TERM	126	// terminal output from device to server
#define	FSFD_SETOPT	125	// send options from server to device
#define	FSFD_CMD	124	// send commands from device to server (FS_CHARSET)

// the first byte of the payload is the (binary) drive number, or one of those two
#define NAMEINFO_UNUSED_DRIVE   0xff    // unspecified like: LOAD"file",8
#define NAMEINFO_UNDEF_DRIVE    0xfe    // non-numeric drive like: LOAD"ftp:file",8
#define NAMEINFO_LAST_DRIVE     0xfd    // colon without drive means last drive: LOAD":file",8

/** 
 * filesystem commands 
 */
#define	  FS_SYNC	 127	/* sync character, ignored until real buffer comes */

#define	  FS_TERM	 0	/* print out to the log on the server */

#define   FS_OPEN_RD     1	/* open file for reading (only) */
#define   FS_OPEN_WR     2	/* open file for writing (only); error if exists */
#define   FS_OPEN_RW     3	/* open file for read/write access */
#define   FS_OPEN_OW     4	/* open file for write-only, overwriting */
#define   FS_OPEN_AP     5	/* open file for appending data to it */
#define   FS_OPEN_DR     6	/* open a directory for reading */

#define   FS_READ        7      /* pull data */
#define   FS_WRITE       8      /* push data */
#define   FS_REPLY       9      /* return value */
#define   FS_EOF         10     /* as FS_WRITE, but signal EOF with */
#define	 FS_SEEK        11	/* seek within a file */
#define   FS_CLOSE       12	/* close a channel */

#define   FS_MOVE      	 13	/* rename a file */
#define   FS_DELETE      14	/* delete a file */
#define   FS_FORMAT      15	/* format a disk */
#define   FS_CHKDSK      16	/* check disk for consistency */
#define   FS_RMDIR       17	/* remove a subdirectory */
#define   FS_MKDIR       18	/* create a subdirectory */
#define   FS_CHDIR       19	/* change into another directory */

#define   FS_ASSIGN      20	/* assign a drive number to a directory */
#define   FS_SETOPT      21	/* set an option using an X-command string as payload */
#define   FS_RESET       22	/* device sends this to notify it has reset */

#define   FS_BLOCK       23	/* summary for block commands */
#define	  FS_GETDATIM	 24	/* request an FS_DATE_* struct with the current date/time as FS_REPLY */

#define	  FS_POSITION	 25	/* position a read/write pointer onto a rel file record; zero-based */

#define   FS_OPEN_DIRECT 26	/* open a direct file (firmware-internal) */

#define   FS_CHARSET 	 27	/* send to the server the name of the requested character set for
				   file names and directory entries */
#define   FS_COPY        28   /* copy a file or merge files */
#define   FS_DUPLICATE   29   /* duplicate a disk image or copy a directory */
#define   FS_INITIALIZE  30   /* initialize (e.g. free buffers and remove file locks) */

/*
 * BLOCK and DIRECT commands
 *
 * Those are used as sub-commands in the FS_BLOCK or FS_DIRECT filesystem command
 */

#define	FS_BLOCK_U1	0
#define	FS_BLOCK_U2	1
#define	FS_BLOCK_BR	2
#define	FS_BLOCK_BW	3
#define	FS_BLOCK_BP	4
#define	FS_BLOCK_BA	5	/* Block allocate */
#define	FS_BLOCK_BF	6	/* Block free */

/*
 * Structure sent for U1/U2
 */
#define	FS_BLOCK_PAR_DRIVE	0	/* drive (to get endpoint, as for open */
#define	FS_BLOCK_PAR_CMD	1	/* actual command, FS_BLOCK_* */
#define FS_BLOCK_PAR_TRACK	2	/* two byte track number */
#define	FS_BLOCK_PAR_SECTOR	4	/* two byte sector number */
#define FS_BLOCK_PAR_CHANNEL	6	/* channel number to use for transfer */
#define	FS_BLOCK_PAR_LEN	7	/* number of bytes in block cmd parameters */

/* 
 * time and date struct, each entry is a byte
 * Used in reading directories as well as FS_GETDATIM
 *
 * Note: I don't expect that to be around in year 2155 :-)
 * In that case the year=255 could be used as extension marker (yuck, y2k all over ;-)
 */

#define   FS_DATE_YEAR    0    	/* last modification date, year-1900 */
#define   FS_DATE_MONTH   1    	/* -"- month */
#define   FS_DATE_DAY     2    	/* -"- day */
#define   FS_DATE_HOUR    3    	/* -"- hour */
#define   FS_DATE_MIN     4    	/* -"- minute */
#define   FS_DATE_SEC     5    	/* -"- second */

#define	  FS_DATE_LEN	  6	/* length of struct */

/* structure of a directory entry when reading a directory */

#define   FS_DIR_LEN     0    	/* file length in bytes, four bytes, low byte first */
#define	  FS_DIR_ATTR	 4	/* file entry attribute bits, see below */
#define   FS_DIR_YEAR    FS_DATE_YEAR + 5    	/* =4;    last modification date, year-1900 */
#define   FS_DIR_MONTH   FS_DATE_MONTH + 5    	/* =5;    -"- month */
#define   FS_DIR_DAY     FS_DATE_DAY + 5    	/* =6;    -"- day */
#define   FS_DIR_HOUR    FS_DATE_HOUR + 5    	/* =7;    -"- hour */
#define   FS_DIR_MIN     FS_DATE_MIN + 5    	/* =8;    -"- minute */
#define   FS_DIR_SEC     FS_DATE_SEC + 5    	/* =9;    -"- second */
#define   FS_DIR_MODE    FS_DATE_LEN + 5   	/* =10;   type of directory entry, see FS_DIR_MOD_* below */
#define   FS_DIR_NAME    FS_DIR_MODE + 1   	/* =11;   zero-terminated file name */

/* type of directory entries */

#define   FS_DIR_MOD_FIL 0    	/* file */
#define   FS_DIR_MOD_NAM 1    	/* disk name */
#define   FS_DIR_MOD_FRE 2    	/* number of free bytes on disk in DIR_LEN */
#define   FS_DIR_MOD_DIR 3    	/* subdirectory */
#define   FS_DIR_MOD_NAS 4       /* disk name  [Suppress LOAD address] */
#define   FS_DIR_MOD_FRS 5       /* free bytes [Suppress BASIC end bytes] */

/* file attribute bits - note they are like the CBM directory entry type bits,
   except for $80, which indicates a splat file, which is inverted.
   For details see also: http://www.baltissen.org/newhtm/1541c.htm
*/
#define	  FS_DIR_ATTR_SPLAT	0x80	/* when set file is splat - i.e. open, display "*" */
#define	  FS_DIR_ATTR_LOCKED	0x40	/* write-protected, show "<" */
#define	  FS_DIR_ATTR_TRANS	0x20	/* transient - will (be?) @-replace(d by) other file */
#define	  FS_DIR_ATTR_ESTIMATE	0x10	/* file size is an estimate only (may require lengthy computation) */
#define	  FS_DIR_ATTR_TYPEMASK	0x07	/* file type mask - see below */

/* represents a (logical) CBM file type - providers may use them or ignore them
   All are simple sequential files, with REL types (in CBM DOS) being a record-oriented
   format (see "side sector" containing the list of blocks for direct access. A provider 
   may choose to allow record-oriented access on all types (with 
   appropriate open with record length) */
#define	  FS_DIR_TYPE_DEL	0
#define	  FS_DIR_TYPE_SEQ	1
#define	  FS_DIR_TYPE_PRG	2
#define	  FS_DIR_TYPE_USR	3
#define	  FS_DIR_TYPE_REL	4
#define	  FS_DIR_TYPE_UNKNOWN	255

#endif

