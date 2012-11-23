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

#define FAT_PROVIDER_C
#include "fat_provider.h"


#define  DEBUG_FAT

/* ----- Glue to firmware -------------------------------------------------------------------- */

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

struct {
	int8_t chan;
	int8_t dir_state;
	FIL f;
} tbl[FAT_MAX_FILES];

// TODO: fix directory stuff for multiple assigns

#define FAT_MAX_ASSIGNS 2	/* Each ASSIGN keeps its own directory */
uint8_t no_of_assigns = 0;	/* Number of assigns */

static DIR dir;
static FILINFO Finfo;
#if _USE_LFN
	static char Lfname[_MAX_LFN+1];
#endif

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
			debug_printf("@% initialized with DIR_HEAD", i); debug_putcrlf();
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

	if((pos = tbl_chpos(chan)) < 0) {
		debug_printf("tbl_close_file: #%d not found!", chan); debug_putcrlf();
		return ERROR_FAULT;
	}

	FRESULT res = f_close(&tbl[pos].f);
	debug_printf("f_close (#%d @%d): %d", chan, pos, res); debug_putcrlf();
	tbl[pos].chan = AVAILABLE;
	return res;
}

/* ----- Provider routines ------------------------------------------------------------------- */

static void *prov_assign(const char *name) {
	/* name = FAT:<parameter> */

	debug_printf("fat_prov_assign name=%s\n", name);
	/* mount (but don't remount) volume, this will always succeed regardless of the drive status */
	if(disk_status(0) & STA_NOINIT) f_mount(0, &Fatfs[0]);

	tbl_init();			// TODO: move this to fat_provider_init();

	show_root_directory();		/* Show directory to proof SD card's alive */

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
			/* open file for write only, overwriting */
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
			/* open file for appending data to it */
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
			res = f_opendir(&dir, ".");
			packet_write_char(rxbuf, res);
			if(tbl_ins_dir(channelno)) {
				debug_puts("No channel for FS_OPEN_DR"); debug_putcrlf();
			}
			debug_printf("f_opendir: %d", res); debug_putcrlf();
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
				// debug_puts("FS_READ delivers: "); debug_putcrlf(); dump_packet(rxbuf);
				// TODO: add FS_REPLY some day
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

		default:
			debug_puts("### UNKNOWN CMD ###"); debug_putcrlf();
			fat_submit_dump(channelno, txbuf, rxbuf);
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
			debug_puts("DIR_HEAD"); debug_putcrlf();
			p[FS_DIR_LEN+0] = 9;		// driveno & 255;
		        p[FS_DIR_LEN+1] = 0;
		        p[FS_DIR_LEN+2] = 0;
		        p[FS_DIR_LEN+3] = 0;
		        // don't set date for now
		        p[FS_DIR_MODE]  = FS_DIR_MOD_NAM;
			// no pattern,  simple default
			strncpy(p+FS_DIR_NAME, ".               ", 16);

		        p[FS_DIR_NAME + 16] = 0;

		        tbl[tblpos].dir_state = DIR_FILES;
			packet->type = FS_EOF;
			return FS_DIR_NAME + 17;


		case DIR_FILES:
			/* Files and directories */
			debug_puts("DIR_FILES"); debug_putcrlf();
			res = f_readdir(&dir, &Finfo);
			if ((res == FR_OK) && Finfo.fname[0]) {
				debug_printf("file '%s'", Finfo.fname); debug_putcrlf();

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

				strcpy(p + FS_DIR_NAME, Finfo.fname);
		/*
#				ifdef _USELFN
					if(Lfname[0]) strncpy(p[FS_DIR_NAME], Lfname, 16);
					p[FS_DIR_NAME + 16] = 0;
#				endif
		*/
				packet->type = FS_EOF;
				return FS_DIR_NAME + strlen(p+FS_DIR_NAME) + 1;
			} else {
				tbl[tblpos].dir_state = DIR_FOOTER;
				// fall through to end of directory
			}

		case DIR_FOOTER:
		default:
			/* number of free bytes / end of directory */
			debug_puts("DIR_FOOTER"); debug_putcrlf();
		        p[FS_DIR_LEN] = 1;
		        p[FS_DIR_LEN+1] = 0;
		        p[FS_DIR_LEN+2] = 0;
		        p[FS_DIR_LEN+3] = 0;
		        p[FS_DIR_MODE]  = FS_DIR_MOD_FRE;
		        p[FS_DIR_NAME] = 0;
		        tbl[tblpos].dir_state = DIR_INACTIVE;
		        tbl[tblpos].chan = AVAILABLE;
			packet->type = FS_EOF;
			return FS_DIR_NAME + 1;
	}
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

static void fat_submit_dump(int8_t channelno, packet_t *txbuf, packet_t *rxbuf)
{
	debug_puts("*** FAT_SUBMIT_CALL ***"); debug_putcrlf();
	debug_printf("chan: %d\n", channelno);
	debug_puts("--- txbuf ---\n"); dump_packet(txbuf);
	debug_puts("--- rxbuf ---\n"); dump_packet(rxbuf);
}

static void show_root_directory(void) {
	/* Show directory to proof SD card's alive */
	UINT s1, s2;
	FRESULT res;
	char *size_unit_char = "KM";

#	ifdef _USE_LFN
		Finfo.lfname = Lfname;
		Finfo.lfsize = sizeof Lfname;
#	endif
	DWORD free_clusters;
	uint32_t free_kb, tot_kb;
	uint8_t free_size_unit, tot_size_unit;

	res = f_opendir(&dir, "/");
	if (res) {
		debug_printf("f_opendir failed, res:%d\n", res);
	} else {
		s1 = s2 = 0;
		for(;;) {
			res = f_readdir(&dir, &Finfo);
			if ((res != FR_OK) || !Finfo.fname[0]) break;
			if (Finfo.fattrib & AM_DIR) {
				s2++;
			} else {
				s1++;
			}
			debug_printf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %s",
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
				debug_printf("%s\n", Lfname);
#			else
				debug_putcrlf();
#			endif
		}
		debug_printf("%4u File(s),%4u Dir(s)", s1, s2);
		FATFS *fs = &Fatfs[0];
		if (f_getfree("0:/", &free_clusters, &fs) == FR_OK) {
			tot_kb = ((fs->n_fatent - 2) * fs->csize ) / 2;
			tot_size_unit = 0;
			if(tot_kb > 1024) { tot_kb /= 1024; tot_size_unit++; }

			free_kb = (free_clusters * fs->csize) / 2;
			free_size_unit = 0;
			if(free_kb > 1024) { free_kb /= 1024; free_size_unit++; }

			debug_printf(", %lu %cB / %lu %cB free\n", free_kb, size_unit_char[free_size_unit],
					tot_kb, size_unit_char[tot_size_unit]);
		}
	}
}

/*****************************************************************************
 * conversion routines between wire format and packet format
 * shameless copied from serial.c
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

	debug_puts("directory_converter"); debug_putcrlf();

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
