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
#define	FSFD_TERM	0xfe	// terminal output from device to server
#define	FSFD_SETOPT	0xfd	// send options from server to device

/** 
 * filesystem commands 
 *
 * Note that the positive values are types of GeckOS/A65 internal message
 * types, and are reused here for compatibility.
 * The zero and the negative numbers below are added as needed here.
 */
#define	  FS_SYNC	 127	/* sync character, ignored until real buffer comes */

#define	  FS_TERM	 0	/* print out */

#define   FS_OPEN_RD     1	/* open file for reading (only) */
#define   FS_OPEN_WR     2	/* open file for writing (only); error if exists */
#define   FS_OPEN_RW     3	/* open file for read/write access */
#define   FS_OPEN_OW     4	/* open file for write-only, overwriting */
#define   FS_OPEN_AP     5	/* open file for appending data to it */
#define   FS_OPEN_DR     6	/* open a directory for reading */
#define   FS_RENAME      7	/* rename a file */
#define   FS_DELETE      8	/* delete a file */
#define   FS_FORMAT      9	/* format a disk */
#define   FS_CHKDSK      10	/* check disk for consistency */
#define   FS_CLOSE       11	/* close a file */
#define   FS_RMDIR       12	/* remove a subdirectory */
#define   FS_MKDIR       13	/* create a subdirectory */
#define   FS_CHDIR       14	/* change into another directory */
#define   FS_ASSIGN      15	/* assign a drive number to a directory */
#define   FS_SETOPT      16	/* set an option using an X-command string as payload */

/*
 * additional FS commands to be sent between fstcp client & server
 * in addition to normal FS_* codes
 */
#define   FS_READ        -1     /* pull data */
#define   FS_WRITE       -2     /* push data */
#define   FS_REPLY       -3     /* return value */
#define   FS_EOF         -4     /* as FS_WRITE, but signal EOF with */
                                /* last byte sent                    */


/* structure of a directory entry when reading a directory */

#define   FS_DIR_LEN     0    	/* file length in bytes, four bytes, low byte first */
#define   FS_DIR_YEAR    4    	/* last modification date, year-1900 */
#define   FS_DIR_MONTH   5    	/* -"- month */
#define   FS_DIR_DAY     6    	/* -"- day */
#define   FS_DIR_HOUR    7    	/* -"- hour */
#define   FS_DIR_MIN     8    	/* -"- minute */
#define   FS_DIR_SEC     9    	/* -"- second */
#define   FS_DIR_MODE    10   	/* type of directory entry, see FS_DIR_MOD_* below */
#define   FS_DIR_NAME    11   	/* zero-terminated file name */

/* type of directory entries */

#define   FS_DIR_MOD_FIL 0    	/* file */
#define   FS_DIR_MOD_NAM 1    	/* disk name */
#define   FS_DIR_MOD_FRE 2    	/* number of free bytes on disk in DIR_LEN */
#define   FS_DIR_MOD_DIR 3    	/* subdirectory */

#endif

