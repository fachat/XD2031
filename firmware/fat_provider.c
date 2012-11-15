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

#include "version.h"
#include "packet.h"
#include "provider.h"
#include "wireformat.h"
#include "fat_provider.h"
#include "petscii.h"
#include "main.h"
#include "debug.h"
#include "led.h"
#include "fatfs/integer.h"
#include "fatfs/ff.h"
#include "sdcard.h"
#include "petscii.h"

#define  DEBUG_FAT

static void *prov_assign(const char *name);
static void prov_free(void *epdata);
void fat_submit(void *epdata, packet_t *buf);
void fat_submit_call(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                uint8_t (*callback)(int8_t channelno, int8_t errnum));

static int8_t directory_converter(packet_t *p, uint8_t drive);
static int8_t to_provider(packet_t *p);

FATFS Fatfs[_VOLUMES];      /* File system object for each logical drive */

static char *size_unit_char = "KM";

void dump_packet(packet_t *p)
{
	debug_printf("type: %i\n", (int8_t) p->type);
	debug_printf("rp: %d wp: %d len: %d chan: %d\n", p->rp, p->wp, p->len, p->chan);
	debug_printf("buffer: %p\n", p->buffer);
	if(p->len) debug_printf("buffer: '%s'\n", p->buffer);
}


static void *prov_assign(const char *name) {
	/* name = FAT:<parameter> */

	debug_printf("fat_prov_assign name=%s\n", name); 
	/* mount (but don't remount) volume, this will always succeed regardless of the drive status */
	if(disk_status(0)) f_mount(0, &Fatfs[0]);

/* ------------------------------------------------------------------------------------------- */
	/* Show directory to proof SD card's alive */
	FILINFO Finfo;
	DIR dir;
	UINT s1, s2;
	FRESULT res;
	DWORD free_clusters;
	uint32_t free_kb, tot_kb;
	uint8_t free_size_unit, tot_size_unit;
#if _USE_LFN
	char Lfname[_MAX_LFN+1];
	Finfo.lfname = Lfname;
	Finfo.lfsize = sizeof Lfname;
#endif


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
#if _USE_LFN
                    for (uint8_t i = strlen(Finfo.fname); i < 14; i++) debug_putc(' ');
                    debug_printf("%s\n", Lfname);
#else
                    debug_putcrlf();
#endif
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
/* ------------------------------------------------------------------------------------------- */
	return NULL;
}

// dummy
static void prov_free(void *epdata) {
	debug_puts("fat_prov_free\n"); 
	return;
}

void fat_submit(void *epdata, packet_t *buf) {
	debug_puts("fat_submit\n");
	dump_packet(buf);
}

void fat_submit_call(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                uint8_t (*callback)(int8_t channelno, int8_t errnum)) 
{
	uint8_t res = ERROR_FAULT;

	debug_puts("fat_submit_call\n"); debug_putcrlf();
	debug_printf("chan: %d\n", channelno);
	debug_puts("--- txbuf ---\n"); dump_packet(txbuf);
	debug_puts("--- rxbuf ---\n"); dump_packet(rxbuf);

	if(channelno == 15) {
		switch(txbuf->type) {
			case FS_CHDIR:
				petscii_to_ascii_str(txbuf->buffer + 1);
				debug_printf("CHDIR into '%s'\n", txbuf->buffer + 1);
				res = f_chdir(txbuf->buffer + 1);
				break;
			default:
				break;
		}
	}

	// do something
	// then callback
	packet_write_char(rxbuf, res);
	callback(channelno, res);
}

static int8_t directory_converter(packet_t *p, uint8_t drive)
{
	debug_printf("fat_directory_converter, drive: %d\n", drive);
	dump_packet(p);
	return 0;
}

static int8_t to_provider(packet_t *p)
{
	/* OPEN lands here. First byte of p->buffer is drive, open string follows */
	/* packet_type contains FS_ commands like 1=FS_OPEN_RD, 2=FS_OPEN_WR rtc. */
	debug_puts("fat_to_provider\n");
	dump_packet(p);
	return 0;
}

provider_t fat_provider  = {
	prov_assign,
	prov_free,
        fat_submit,
        fat_submit_call,
	directory_converter,
	to_provider
};
