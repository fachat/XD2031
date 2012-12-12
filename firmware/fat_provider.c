/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat
    Copyright (C) 2012 Nils Eilers

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

/* TODO:
 * - FS_DELETE
 * - directory listing aborts sometimes. Why?
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "version.h"
#include "packet.h"
#include "provider.h"
#include "wireformat.h"
#include "petscii.h"
#include "main.h"
#include "debug.h"
#include "led.h"
#include "fatfs/integer.h"
#include "fatfs/ff.h"
#include "sdcard.h"
#include "petscii.h"
#include "fat_provider.h"
#include "dir.h"


#define  DEBUG_FAT

/* ----- Glue to firmware -------------------------------------------------------------------- */

static void *prov_assign(const char *name);
static void prov_free(void *epdata);
static void fat_submit(void *epdata, packet_t *buf);
static void fat_submit_call(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                uint8_t (*callback)(int8_t channelno, int8_t errnum));
static int8_t directory_converter(packet_t *p, uint8_t drive);
static int8_t to_provider(packet_t *p);

provider_t fat_provider  = {
	prov_assign,
	prov_free,
	fat_submit,
	fat_submit_call,
	directory_converter,
	to_provider
};

/* ----- Private provider data --------------------------------------------------------------- */

static FATFS Fatfs[_VOLUMES];	/* File system object for each logical drive */

#define AVAILABLE -1
enum enum_dir_state { DIR_INACTIVE, DIR_HEAD, DIR_FILES, DIR_FOOTER };
struct {
	int8_t chan;		// entry used by channel # or AVAILABLE
	int8_t dir_state;	// DIR_INACTIVE for files or DIR_* when reading directories
	FIL f;			// file data
} tbl[FAT_MAX_FILES];

#ifndef FAT_MAX_ASSIGNS
#	define FAT_MAX_ASSIGNS 2	/* Each drive has a current directory */
#endif
uint8_t no_of_assigns = 0;		/* Number of assigns/drives */

// TODO: fix directory stuff for multiple assigns
static DIR dir;
static uint8_t dir_drive;
static char dir_mask[_MAX_LFN+1];
static char dir_headline[_MAX_LFN+1];
static FILINFO Finfo;

/* ----- Prototypes -------------------------------------------------------------------------- */

// helper functions
static int8_t fs_read_dir(void *epdata, int8_t channelno, packet_t *packet);
static int8_t fs_rename(char *buf);

// debug functions
static void dump_packet(packet_t *p);

/* ----- File / Channel table ---------------------------------------------------------------- */

static void tbl_init(void) {
	for(uint8_t i=0; i < FAT_MAX_FILES; i++) {
		tbl[i].chan = AVAILABLE;
		tbl[i].dir_state = DIR_INACTIVE;
	}
}

static int8_t tbl_chpos(uint8_t chan) {
	for(uint8_t i=0; i < FAT_MAX_FILES; i++) if(tbl[i].chan == chan) return i;
	return -1;
}

static int8_t get_dir_state(uint8_t chan) {
	int8_t pos = tbl_chpos(chan);
	if(pos < 0 ) return pos;
	return tbl[pos].dir_state;
}

static FIL *tbl_ins_file(uint8_t chan) {
	for(uint8_t i=0; i < FAT_MAX_FILES; i++) {
		if(tbl[i].chan == chan) {
			debug_printf("#%d already exists @%d", chan, i); debug_putcrlf();
			tbl[i].dir_state = DIR_INACTIVE;
			return &tbl[i].f;
		}
		if(tbl[i].chan == AVAILABLE) {
			tbl[i].chan = chan;
			tbl[i].dir_state = DIR_INACTIVE;
			debug_printf("#%d found in file table @ %d", chan, i); debug_putcrlf();
			return &tbl[i].f;
		}
	}
	debug_printf("tbl_ins_file: out of memory", chan); debug_putcrlf();
	return NULL;
}

static int8_t tbl_ins_dir(int8_t chan) {
	for(uint8_t i=0; i < FAT_MAX_FILES; i++) {
		if(tbl[i].chan == chan) {
			debug_printf("dir_state #%d := DIR_HEAD", i); debug_putcrlf();
			tbl[i].dir_state = DIR_HEAD;
			return 0;
		}
		if(tbl[i].chan == AVAILABLE) {
			debug_printf("@%d initialized with DIR_HEAD", i); debug_putcrlf();
			tbl[i].chan = chan;
			tbl[i].dir_state = DIR_HEAD;
			return 0;
		}
	}
	return 1;
}
static FIL *tbl_find_file(uint8_t chan) {
	uint8_t pos;

	if((pos = tbl_chpos(chan)) < 0) {
		debug_printf("tbl_find_file: #%d not found!", chan); debug_putcrlf();
		return NULL;
	}
	debug_printf("#%d found @%d", chan, pos); debug_putcrlf();
	return &tbl[pos].f;
}

static FRESULT tbl_close_file(uint8_t chan) {
	uint8_t pos;
	FRESULT res = ERROR_OK;

	if((pos = tbl_chpos(chan)) != AVAILABLE) {
		FRESULT res = f_close(&tbl[pos].f);
		debug_printf("f_close (#%d @%d): %d", chan, pos, res); debug_putcrlf();
		tbl[pos].chan = AVAILABLE;
	}
	return res;
}

/* ----- Provider routines ------------------------------------------------------------------- */

static void *prov_assign(const char *name) {
	/* name = FAT:<parameter> */

	debug_printf("fat_prov_assign name=%s\n", name);
	/* mount (but don't remount) volume, this will always succeed regardless of the drive status */
	if(disk_status(0) & STA_NOINIT) f_mount(0, &Fatfs[0]);

	tbl_init();			// TODO: move this to fat_provider_init();

	return NULL;
}

static void prov_free(void *epdata) {
	// free the ASSIGN-related data structure
	// dummy
}

static void fat_submit(void *epdata, packet_t *buf) {
	// submit a fire-and-forget packet (log_*, terminal)
	// not applicable for storage ==> dummy
}

static void fat_submit_call(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
	uint8_t (*callback)(int8_t channelno, int8_t errnum))
{
	// submit a request/response packet; call the callback function when the
	// response is received; If callback returns != 0 then the call is kept open,
	// and further responses can be received

	int8_t res = ERROR_FAULT;
	int8_t res2 = ERROR_FAULT;
	UINT transferred = 0;
	uint8_t len;
	FIL *fp;
	int8_t ds;
	char *path = (char *) (txbuf->buffer + 1);

	debug_puts("fat_submit_call"); debug_putcrlf();
	// dump_packet(txbuf);

#	ifdef _USE_LFN
		Finfo.lfname = Lfname;
		Finfo.lfsize = sizeof Lfname;
#	endif

	switch(txbuf->type) {
		case FS_CHDIR:
			debug_printf("CHDIR into '%s'\n", path);
			res = f_chdir(path);
			packet_write_char(rxbuf, res);
			break;

		case FS_MKDIR:
			debug_printf("MKDIR '%s'\n", path);
			res = f_mkdir(path);
			packet_write_char(rxbuf, res);
			break;

		case FS_RMDIR:
			// will unlink files as well. Should I test first, if "path" is really a directory?
			debug_printf("RMDIR '%s'\n", path);
			res = f_unlink(path);
			packet_write_char(rxbuf, res);
			break;

		case FS_OPEN_RD:
			/* open file for reading (only) */
			fp = tbl_ins_file(channelno);
			if(fp) {
				res = f_open(fp, path, FA_READ | FA_OPEN_EXISTING);
				debug_printf("FS_OPEN_RD '%s' #%d, res=%d\n", path, channelno, res);
				packet_write_char(rxbuf, res);
			} else {
				// too many files!
				packet_write_char(rxbuf, ERROR_NO_CHANNEL);
			}
			break;

		case FS_OPEN_WR:
			/* open file for writing (only); error if file exists */
			fp = tbl_ins_file(channelno);
			if(fp) {
				res = f_open(fp, path, FA_WRITE | FA_CREATE_NEW);
				debug_printf("FS_OPEN_WR '%s' #%d, res=%d\n", path, channelno, res);
				packet_write_char(rxbuf, res);
			} else {
				// too many files!
				packet_write_char(rxbuf, ERROR_NO_CHANNEL);
			}
			break;

		case FS_OPEN_RW:
			/* open file for read/write access */
			fp = tbl_ins_file(channelno);
			if(fp) {
				res = f_open(fp, path, FA_READ | FA_WRITE);
				debug_printf("FS_OPEN_RW '%s' #%d, res=%d\n", path, channelno, res);
				packet_write_char(rxbuf, res);
			} else {
				// too many files!
				packet_write_char(rxbuf, ERROR_NO_CHANNEL);
			}
			break;

		case FS_OPEN_OW:
			/* open file for write only, overwriting. If the file exists it is truncated
			 * and writing starts at the beginning. If it does not exist, create the file */
			fp = tbl_ins_file(channelno);
			if(fp) {
				res = f_open(fp, path, FA_WRITE | FA_CREATE_ALWAYS);
				debug_printf("FS_OPEN_OW '%s' #%d, res=%d\n", path, channelno, res);
				packet_write_char(rxbuf, res);
			} else {
				// too many files!
				packet_write_char(rxbuf, ERROR_NO_CHANNEL);
			}
			break;

		case FS_OPEN_AP:
			/* open an existing file for appending data to it. Returns an error if it
			 * does not exist */
			fp = tbl_ins_file(channelno);
			if(fp) {
				res = f_open(fp, path, FA_WRITE | FA_OPEN_EXISTING);
				debug_printf("FS_OPEN_AP '%s' #%d, res=%d\n", path, channelno, res);
				/* move to end of file to append data */
				res = f_lseek(fp, f_size(fp));
				debug_printf("Move to EOF to append data: %d\n", res);
				packet_write_char(rxbuf, res);
			} else {
				// too many files!
				packet_write_char(rxbuf, ERROR_NO_CHANNEL);
			}
			break;

		case FS_OPEN_DR:
			/* open a directory for reading */
			debug_printf("FS_OPEN_DIR for drive %d, ", txbuf->buffer[0]);
			char *b, *d;
			if (txbuf->len > 1) {
				debug_printf("dirmask '%s'\n", txbuf->buffer + 1);
				// If path is a directory, list its contents
				if(f_stat(path, &Finfo) == FR_OK) {
					if(Finfo.fattrib & AM_DIR) {
						debug_printf("'%s' is a directory\n", path);
						res = f_opendir(&dir, path);
						b = splitpath(path, &d);
						strcpy(dir_headline, b);
						strcpy(dir_mask, "*");
					}
				} else {
					b = splitpath(path, &d);
					strcpy(dir_headline, b);
					debug_printf("DIR: %s NAME: %s\n", d, b);
					res = f_opendir(&dir, d);
					strncpy(dir_mask, b, _MAX_LFN);
					dir_mask[_MAX_LFN] = 0;
				}
			} else {
				debug_puts("no dirmask\n");
				strcpy(dir_mask, "*");
				f_getcwd(dir_headline, sizeof(dir_headline));
				// Remove drive string "0:"
				memmove(dir_headline, dir_headline + 2, sizeof(dir_headline) - 2);
				res = f_opendir(&dir, ".");
			}

			dir_drive = txbuf->buffer[0];
			if(tbl_ins_dir(channelno)) {
				res = ERROR_NO_CHANNEL;
				debug_puts("No channel for FS_OPEN_DR"); debug_putcrlf();
			}
			debug_printf("f_opendir: %d", res); debug_putcrlf();
			packet_write_char(rxbuf, res);
			break;

		case FS_CLOSE:
			/* close a file, ignored when not opened first */
			debug_printf("FS_CLOSE #%d", channelno); debug_putcrlf();
			packet_write_char(rxbuf, res);
			break;

		case FS_RENAME:
			/* rename / move a file */
			dump_packet(txbuf);
			packet_write_char(rxbuf, fs_rename(path));
			break;

		case (uint8_t) (FS_READ & 0xFF):
			ds = get_dir_state(channelno);
			if(ds < 0 ) {
				debug_printf("No channel found for FS_READ #%d", channelno); debug_putcrlf();
				res = ERROR_NO_CHANNEL;
			} else if(ds) {
				// Read directory
				res = fs_read_dir(epdata, channelno, rxbuf);
			} else {
				// Read file
				fp = tbl_find_file(channelno);
				// FIXME: EOF handling
				res = f_read(fp, rxbuf->buffer, rxbuf->len, &transferred);
				debug_printf("%d/%d bytes read from #%d, res=%d\n", transferred, rxbuf->len, channelno, res);
				rxbuf->wp = transferred;
				if(transferred < rxbuf->len) rxbuf->type = FS_EOF;
				else rxbuf->type = FS_WRITE;
				if(res) rxbuf->type = FS_EOF;
				// TODO: add FS_REPLY when allowed (not yet)
			}
			break;

		case (uint8_t) (FS_WRITE & 0xFF):
		case (uint8_t) (FS_EOF & 0xFF):
			fp = tbl_find_file(channelno);
			if(fp) {
				if(txbuf->rp < txbuf->wp) {
					len = txbuf->wp - txbuf->rp;
					res = f_write(fp, txbuf->buffer, len, &transferred);
					debug_printf("%d/%d bytes written to #%d, res=%d\n", transferred, len, channelno, res);
				}

				if(txbuf->type == ((uint8_t) (FS_EOF & 0xFF))) {
					res2 = tbl_close_file(channelno);
					debug_printf("f_close channel %d, res=%d", channelno, res2); debug_putcrlf();
					if(!res) res=res2; // Return first error if any
				}
			} else {
				debug_printf("No channel found for FS_WRITE/FS_EOF #%d", channelno); debug_putcrlf();
			}

			break;

		case FS_FORMAT:
		case FS_CHKDSK:
			debug_printf("Command %d unsupported", txbuf->type);
			packet_write_char(rxbuf, ERROR_SYNTAX_INVAL);
			break;

		default:
			debug_puts("### UNKNOWN CMD ###"); debug_putcrlf();
			dump_packet(txbuf);
			break;
	}
	callback(channelno, res);
}


/* ----- Directories ------------------------------------------------------------------------- */

int8_t fs_read_dir(void *epdata, int8_t channelno, packet_t *packet) {
	int8_t res;
	int8_t tblpos = tbl_chpos(channelno);
	char *p = (char *) packet->buffer;

	switch(get_dir_state(channelno)) {
		case -1:
			/* no channel */
			debug_puts("fs_read_dir: no channel!"); debug_putcrlf();
			packet->type = FS_EOF;
			return -ERROR_NO_CHANNEL;
			break;

		case DIR_HEAD:
			/* Disk name */
			debug_puts("fs_read_dir/DIR_HEAD"); debug_putcrlf();
			p[FS_DIR_LEN+0] = dir_drive;
			p[FS_DIR_LEN+1] = 0;
			p[FS_DIR_LEN+2] = 0;
			p[FS_DIR_LEN+3] = 0;
			// TODO: add date
			p[FS_DIR_MODE]  = FS_DIR_MOD_NAM;
			strncpy(p+FS_DIR_NAME, dir_headline, 16);
			p[FS_DIR_NAME + 16] = 0;

			tbl[tblpos].dir_state = DIR_FILES;
			packet_update_wp(packet, FS_DIR_NAME + strlen(p+FS_DIR_NAME));
			return 0;

		case DIR_FILES:
			/* Files and directories */
			debug_puts("fs_read_dir/DIR_FILES"); debug_putcrlf();
			for(;;) {
				res = f_readdir(&dir, &Finfo);
				if(res != FR_OK || !Finfo.fname[0]) {
					tbl[tblpos].dir_state = DIR_FOOTER;
					return 0;
				}
				char *filename;
				filename = Finfo.fname;
#				if _USE_LFN
					if(Lfname[0]) filename = Lfname;
#				endif
				if(compare_pattern(filename, dir_mask)) break;
			}

			// TODO: skip or skip not hidden files
			debug_printf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  '%s'",
					(Finfo.fattrib & AM_DIR) ? 'D' : '-',
					(Finfo.fattrib & AM_RDO) ? 'R' : '-',
					(Finfo.fattrib & AM_HID) ? 'H' : '-',
					(Finfo.fattrib & AM_SYS) ? 'S' : '-',
					(Finfo.fattrib & AM_ARC) ? 'A' : '-',
					(Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,
					(Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63,
					Finfo.fsize, &(Finfo.fname[0]));
#			if _USE_LFN
				for (uint8_t i = strlen(Finfo.fname); i < 14; i++) debug_putc(' ');
				debug_printf("'%s'", Lfname);
#			endif
			debug_putcrlf();

			p[FS_DIR_LEN] = Finfo.fsize & 255;
			p[FS_DIR_LEN+1] = (Finfo.fsize >> 8) & 255;
			p[FS_DIR_LEN+2] = (Finfo.fsize >> 16) & 255;
			p[FS_DIR_LEN+3] = (Finfo.fsize >> 24) & 255;

			p[FS_DIR_YEAR] = (Finfo.fdate >> 9) + 80;
			p[FS_DIR_MONTH] = (Finfo.fdate >> 5) & 15;
			p[FS_DIR_DAY] = Finfo.fdate & 31;
			p[FS_DIR_HOUR] = Finfo.ftime >> 11;
			p[FS_DIR_MIN] = (Finfo.ftime >> 5) & 63;

			p[FS_DIR_MODE] = Finfo.fattrib & AM_DIR ? FS_DIR_MOD_DIR : FS_DIR_MOD_FIL;

#			ifdef _USE_LFN
				if((strlen(Lfname) > 16 ) || (!Lfname[0])) {
					// no LFN or too long LFN ==> use short name
					strcpy(p + FS_DIR_NAME, Finfo.fname);
				} else strcpy(p + FS_DIR_NAME, Lfname);
#			else
				strcpy(p + FS_DIR_NAME, Finfo.fname);
#			endif

			packet_update_wp(packet, FS_DIR_NAME + strlen(p+FS_DIR_NAME));
			return 0;

		case DIR_FOOTER:
		default:
			/* number of free bytes / end of directory */
			debug_puts("fs_read_dir/DIR_FOOTER"); debug_putcrlf();
			FATFS *fs = &Fatfs[0];
			DWORD free_clusters;
			DWORD free_bytes = 0;	/* fallback default size */
			int8_t res = f_getfree("0:/", &free_clusters, &fs);
			if(res == FR_OK) {
				// assuming 512 bytes/sector ==> * 512 ==> << 9
				free_bytes = (free_clusters * fs->csize) << 9;
			} else
			{
				debug_printf("f_getfree: %d\n", res);
			}
			p[FS_DIR_LEN] = free_bytes & 255;
			p[FS_DIR_LEN+1] = (free_bytes >> 8) & 255;
			p[FS_DIR_LEN+2] = (free_bytes >> 16) & 255;
			p[FS_DIR_LEN+3] = (free_bytes >> 24) & 255;
			p[FS_DIR_MODE]  = FS_DIR_MOD_FRE;
			p[FS_DIR_NAME] = 0;
			tbl[tblpos].dir_state = DIR_INACTIVE;
			tbl[tblpos].chan = AVAILABLE;
			packet_update_wp(packet, FS_DIR_NAME + strlen(p+FS_DIR_NAME));
			packet->type = FS_EOF;
			return 0;
	}
}

/* ----- Rename a file or directory ---------------------------------------------------------- */

static int8_t fs_rename(char *buf) {
	/* Rename/move a file or directory
	 * DO NOT RENAME/MOVE OPEN OBJECTS!
	 */
	int8_t er = ERROR_FAULT;
	uint8_t p = 0;
	char *from, *to;
	FILINFO fileinfo;

	// first find the two names separated by "="
	while (buf[p] != 0 && buf[p] != '=') p++;
	if (!buf[p]) return ERROR_SYNTAX_NONAME;

	buf[p] = 0;
	from = buf + p + 1;
	to = buf;

	debug_printf("FS_RENAME '%s' to '%s'", from, to); debug_putcrlf();

	if((er = f_stat(to, &fileinfo)) == ERROR_OK) return ERROR_FILE_EXISTS;
	if(er != FR_NO_FILE) return er;

	return f_rename(from, to);
}

/* ----- Debug routines ---------------------------------------------------------------------- */

static void dump_packet(packet_t *p)
{
	uint16_t tot = 0;
	uint8_t line = 0;
	uint8_t x = 0;

	debug_puts("--- dump packet ---"); debug_putcrlf();
	debug_printf("ptr: %p ", p);
	debug_printf("type: %d ", p->type);
	debug_printf("chan: %d   ", p->chan);
	debug_printf("rp: %d wp: %d len: %d ", p->rp, p->wp, p->len);
	debug_putcrlf();
	if(p->len) {
		while(tot < p->len) {
			debug_printf("%04X  ", tot);
			for(x=0; x<16; x++) {
				if(line+x < p->len) {
					tot++;
					debug_printf("%02X ", p->buffer[line+x]);
				}
				else debug_puts("   ");
				if(x == 7) debug_putc(' ');
			}
			debug_puts(" |");
			for(x=0; x<16; x++) {
				if(line+x < p->len) {
					uint8_t c = p->buffer[line+x];
					if(isprint(c)) debug_putc(c); else debug_putc(' ');
				} else debug_putc(' ');
			}
			debug_putc('|');
			debug_putcrlf();
			line = tot;
		}

	}
	debug_puts("--- end of dump ---"); debug_putcrlf();
}

/*****************************************************************************
 * conversion routines between wire format and packet format
 * shameless copied from serial.c
 * TODO: don't waste flash space, let serial.c and fat_provider.c share these routines
 */

/*
 * helper for conversion of ASCII to PETSCII
 */

static uint8_t *append(uint8_t *outp, const char *to_append) {
	while(*to_append != 0) {
		*outp = ascii_to_petscii(*to_append);
		outp++;
		to_append++;
	}
	*outp = 0;
	outp++;
	return outp;
}

/*
 * each packet from the UART contains one directory entry.
 * The format is defined as FS_DIR_* offset definitions
 *
 * This method converts it into a Commodore BASIC line definition
 *
 * Note that it currently relies on an entry FS_DIR_MOD_NAME as first
 * and FS_DIR_MOD_FRE as last entry, to add the PET BASIC header and
 * footer!
 */
static uint8_t out[64];

static int8_t directory_converter(packet_t *p, uint8_t drive) {
	uint8_t *inp = NULL;
	uint8_t *outp = &(out[0]);

	if (p == NULL) {
		debug_puts("P IS NULL!");
		return -1;
	}

	inp = packet_get_buffer(p);
	uint8_t type = inp[FS_DIR_MODE];

	//packet_update_wp(p, 2);

	if (type == FS_DIR_MOD_NAM) {
		*outp = 1; outp++;	// load address low
		*outp = 4; outp++;	// load address high
	}

	*outp = 1; outp++;		// link address low; will be overwritten on LOAD
	*outp = 1; outp++;		// link address high

	uint16_t lineno = 0;
	if (type == FS_DIR_MOD_NAM) {
		lineno = drive;
	} else {
		// line number, derived from file length
		if (inp[FS_DIR_LEN+3] != 0
			|| inp[FS_DIR_LEN+2] > 0xf9
			|| (inp[FS_DIR_LEN+2] == 0xf9 && inp[FS_DIR_LEN+1] == 0xff && inp[FS_DIR_LEN] != 0)) {
			// more than limit of 63999 blocks
			lineno = 63999;
		} else {
			lineno = inp[FS_DIR_LEN+1] | (inp[FS_DIR_LEN+2] << 8);
			if (inp[FS_DIR_LEN] != 0) {
				lineno++;
			}
		}
	}
	*outp = lineno & 255; outp++;
	*outp = (lineno>>8) & 255; outp++;

	//snprintf(outp, 5, "%hd", (unsigned short)lineno);
	//outp++;
	if (lineno < 10) { *outp = ' '; outp++; }
	if (type == FS_DIR_MOD_NAM) {
		*outp = 0x12;	// reverse for disk name
		outp++;
	} else {
		if (type != FS_DIR_MOD_FRE) {
			if (lineno < 100) { *outp = ' '; outp++; }
			if (lineno < 1000) { *outp = ' '; outp++; }
			if (lineno < 10000) { *outp = ' '; outp++; }
		}
	}

	if (type != FS_DIR_MOD_FRE) {
		*outp = '"'; outp++;
		uint8_t i = FS_DIR_NAME;
		// note the check i<16 - this is buffer overflow protection
		// file names longer than 16 chars are not displayed
		while ((inp[i] != 0) && (i < (FS_DIR_NAME + 16))) {
			*outp = ascii_to_petscii(inp[i]);
			outp++;
			i++;
		}
		// note: not counted in i
		*outp = '"'; outp++;

		// fill up with spaces, at least one space behind filename
		while (i < FS_DIR_NAME + 16 + 1) {
			*outp = ' '; outp++;
			i++;
		}
	}

	// add file type
	if (type == FS_DIR_MOD_NAM) {
		// file name entry
		outp = append(outp, SW_NAME_LOWER);
		//strcpy(outp, SW_NAME_LOWER);
		//outp += strlen(SW_NAME_LOWER)+1;	// includes ending 0-byte
	} else
	if (type == FS_DIR_MOD_DIR) {
		outp = append(outp, "dir");
		//strcpy(outp, "dir");
		//outp += 4;	// includes ending 0-byte
	} else
	if (type == FS_DIR_MOD_FIL) {
		outp = append(outp, "prg");
		//strcpy(outp, "prg");
		//outp += 4;	// includes ending 0-byte
	} else
	if (type == FS_DIR_MOD_FRE) {
		outp = append(outp, "blocks free");
		//strcpy(outp, "bytes free");
		//outp += 11;	// includes ending 0-byte

		*outp = 0; outp++;	// BASIC end marker (zero link address)
		*outp = 0; outp++;	// BASIC end marker (zero link address)
	}

	uint8_t len = outp - out;
	if (len > packet_get_capacity(p)) {
		debug_puts("CONVERSION NOT POSSIBLE!"); debug_puthex(len); debug_putcrlf();
		return -1;	// conversion not possible
	}

#if DEBUG_SERIAL
	debug_puts("CONVERTED TO: LEN="); debug_puthex(len);
	for (uint8_t j = 0; j < len; j++) {
		debug_putc(' '); debug_puthex(out[j]);
	}
	debug_putcrlf();
#endif
	// this should probably be combined
	memcpy(packet_get_buffer(p), &out, len);
	packet_update_wp(p, len);

	return 0;
}

/**
 * convert PETSCII names to ASCII names
 */
static int8_t to_provider(packet_t *p) {
	uint8_t *buf = packet_get_buffer(p);
	uint8_t len = packet_get_contentlen(p);
//debug_printf("CONVERT: len=%d, b=%s\n", len, buf);
	while (len > 0) {
//debug_puts("CONVERT: ");
//debug_putc(*buf); //debug_puthex(*buf);debug_puts("->");
		*buf = petscii_to_ascii(*buf);
//debug_putc(*buf);debug_puthex(*buf);debug_putcrlf();
		buf++;
		len--;
	}
	return 0;
}
