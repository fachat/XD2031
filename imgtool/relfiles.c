/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2013 Andre Fachat

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

//
// utility to dump REL files from a disk image, and to check their integrity
//

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "diskimgs.h"
#include "petscii.h"
#include "log.h"
#include "terminal.h"
#include "imgtool.h"
#include "relfiles.h"

static bool bad_relfile; // used by read_sector() and process_relfile()

static void read_sector(di_t *img, int track, int sector, uint8_t *buf) {
	memmove(buf, img->image + img->di.LBA(track, sector) * 256, 256);
	if (img->error_table) if (is_bad_block(img->error_table[img->di.LBA(track, sector)])) {
		bad_relfile = true;
	}
}

// Read in a file 
//
// Note: a value different from 0 in *dblklen defines the maximum number of blocks to be read,
// despite what the link chains says. At least the 1001 DOS seems to be able to mangle the last
// link entry.
static void read_file(di_t *img, uint8_t track, uint8_t sector, 
		uint8_t **data, uint8_t **dtracks, uint8_t **dsectors, int *dlength, int *dblklen) {

	log_debug("reading in sector chain file starting %d/%d\n", track, sector);

	int len = 0, blockno = 0;
	int max_len = 2000000;

	uint8_t *mem = malloc(max_len);
	uint8_t *tracks = malloc(1 + max_len / 254);
	uint8_t *sectors = malloc(1 + max_len / 254);

	while (track != 0 && (*dblklen == 0 || blockno < *dblklen)) {

		if (len + 254 > max_len) {
			max_len = max_len * 1.5;
			mem = realloc(mem, max_len);
			tracks = realloc(tracks, 1 + max_len / 254);
			sectors = realloc(sectors, 1 + max_len / 254);
		}
	
		tracks[blockno] = track;
		sectors[blockno] = sector;
		blockno ++;

		read_sector(img, track, sector, mem+len);

		// link to next block
		track = mem[len];
		sector = mem[len + 1];

		// remove link address
		memmove(mem+len, mem+len+2, 254);
		
		if (track == 0) {
			len += sector - 1;
		} else {
			len += 254;
		}
	}

	*data = mem;
	*dtracks = tracks;
	*dsectors = sectors;
	*dlength = len;
	*dblklen = blockno;

}

// Read in a block chain raw (including links) 
//
// Note: a value different from 0 in *dblklen defines the maximum number of blocks to be read,
// despite what the link chains says. At least the 1001 DOS seems to be able to mangle the last
// link entry.
static void read_raw(di_t *img, uint8_t track, uint8_t sector, 
		uint8_t **data, uint8_t **dtracks, uint8_t **dsectors, int *dlength, int *dblklen) {

	log_debug("reading in sector chain raw starting %d/%d\n", track, sector);

	int len = 0, blockno = 0;
	int max_len = 2000000;

	uint8_t *mem = malloc(max_len);
	uint8_t *tracks = malloc(1 + max_len / 254);
	uint8_t *sectors = malloc(1 + max_len / 254);

	while (track != 0 && (*dblklen == 0 || blockno < *dblklen)) {

		if (len + 256 > max_len) {
			max_len = max_len * 1.5;
			mem = realloc(mem, max_len);
			tracks = realloc(tracks, 1 + max_len / 256);
			sectors = realloc(sectors, 1 + max_len / 256);
		}
	
		tracks[blockno] = track;
		sectors[blockno] = sector;
		blockno ++;

		read_sector(img, track, sector, mem+len);

		// link to next block
		track = mem[len];
		sector = mem[len + 1];
		
		if (track == 0) {
			len += sector - 1;
		} else {
			len += 256;
		}
	}

	*data = mem;
	*dtracks = tracks;
	*dsectors = sectors;
	*dlength = len;
	*dblklen = blockno;

}

// handle a full side sector group
// 1) check its consistency
// 2) append its blocks to the given *stracks/*ssectors[sblklen] arrays
// return true if group is full, false if groups not full
static int append_ssg(di_t *img, uint8_t track, uint8_t sector, 
		uint8_t **stracks, uint8_t **ssectors, int *sblklen, int reclen, 
		uint8_t **ssbtracks, uint8_t **ssbsectors, int ddblklen) {

	log_debug("Reading side sector group starting from %d/%d\n", track, sector);

	// first read in the whole group into memory
	uint8_t *data = NULL;
	uint8_t *dtracks = NULL;
	uint8_t *dsectors = NULL;
	int dlength = 0, dblklen = 0;
	read_raw(img, track, sector, &data, &dtracks, &dsectors, &dlength, &dblklen);

	int lastbyte = 255;
	// dtracks, dsectors contains the list of side sectors of the group
	
	// check consistency of each of the side sector blocks
	for (int blk = 0; blk < dblklen; blk++) {

		int bp = blk << 8;
		// the whole side sector file is one file, so we need to check when a new group starts
		int grpno = blk / SSG_SIDE_SECTORS_MAX;
		int blkingrp = blk % SSG_SIDE_SECTORS_MAX;

		lastbyte = 255;

		// side sector block number
		if (data[bp + SSB_OFFSET_SECTOR_NUM] != blkingrp) {
			log_error("Side sector number in block %d/%d (%d) not correct, should be %d, is %d\n",
				dtracks[blk], dsectors[blk], bp, blkingrp, data[bp + SSB_OFFSET_SECTOR_NUM]);
		}

		// check record length 
		if (data[bp + SSB_OFFSET_RECORD_LEN] != reclen) {
			log_error("Side sector record len in block %d/%d (%d) not correct, "
				"should be %d, is %d\n",
				dtracks[blk], dsectors[blk], bp, reclen, data[bp + SSB_OFFSET_RECORD_LEN]);
		}

		// check link (probably superflous, as that link chain is how the file is loaded
		if (blk < dblklen-1) {
			// not last block
			if (data[bp + BLK_OFFSET_NEXT_TRACK] != dtracks[blk+1]
				|| data[bp + BLK_OFFSET_NEXT_SECTOR] != dsectors[blk+1]) {
				log_error("Side sector link in block %d/%d not correct, "
					"should be %d/%d, is %d/%d\n",
					dtracks[blk], dsectors[blk],
					255 & data[bp + BLK_OFFSET_NEXT_TRACK], 
					255 & data[bp + BLK_OFFSET_NEXT_SECTOR],
					dtracks[blk+1], dsectors[blk+1]);
			}
		} else {
			// last block
			unsigned int seclen = SSB_OFFSET_SECTOR + 2 * (ddblklen % SSB_INDEX_SECTOR_MAX) - 1;
			if (data[bp + BLK_OFFSET_NEXT_TRACK] != 0
				|| data[bp + BLK_OFFSET_NEXT_SECTOR] != seclen) {
				log_error("Side sector link in block %d/%d not correct, "
					"should be %d/%d, is %d/%d\n",
					dtracks[blk], dsectors[blk],
					0, seclen,
					255 & data[bp + BLK_OFFSET_NEXT_TRACK], 
					255 & data[bp + BLK_OFFSET_NEXT_SECTOR]
					);
			}
			lastbyte = data[bp + BLK_OFFSET_NEXT_SECTOR];
		}

		// check consistency of side sector cross links
		for (int i = 0; 
			i < dblklen - (grpno * SSG_SIDE_SECTORS_MAX) 
			&& i < SSG_SIDE_SECTORS_MAX; i++) {
			if (data[bp + SSB_OFFSET_SSG + (i<<1)] != dtracks[grpno * SSG_SIDE_SECTORS_MAX + i]
				|| data[bp + SSB_OFFSET_SSG + (i<<1) + 1] 
					!= dsectors[grpno * SSG_SIDE_SECTORS_MAX + i]) {
				log_error("Side sector cross link in block %d/%d pos %d not correct, "
					"should be %d/%d, is %d/%d\n",
					dtracks[blk], dsectors[blk],
					i,
					data[bp + BLK_OFFSET_NEXT_TRACK], 
					data[bp + BLK_OFFSET_NEXT_SECTOR],
					255 & dtracks[grpno * SSG_SIDE_SECTORS_MAX + i], 
					255 & dsectors[grpno * SSG_SIDE_SECTORS_MAX + i]);
			}
		}

		// now add all the blocks pointed to by the side sector to the given
		// external array
		for (int i = SSB_OFFSET_SECTOR; i < lastbyte; i += 2) {
			(*stracks)[*sblklen] = data[bp + i];
			(*ssectors)[*sblklen] = data[bp + i + 1];
			(*sblklen)++;
		}
	}

	log_debug("read side sector chain gives %d bytes of data in %d blocks\n", dlength, dblklen);

	memcpy(*ssbtracks, dtracks, dblklen);
	memcpy(*ssbsectors, dsectors, dblklen);

	return (dblklen >= 6 && lastbyte >= 255);
}



int process_relfile(
	di_t *img, 
	uint8_t start_track, 
	uint8_t start_sector, 
	uint8_t ss_track, 
	uint8_t ss_sector,
	uint8_t reclen,
	bool testing,
        bool dump,
	bool *weak,
	char *filename) 
{
	bool faulty = false;
	bad_relfile = false;

        if (dump) log_info("Dump of \"%s\":\n", filename);

	// we compare the file as file chain with the information as stored in the side sectors
	
	// first read the whole file, taking note of the sector addresses
	uint8_t *data = NULL;
	uint8_t *dtracks = NULL;
	uint8_t *dsectors = NULL;
	int dlength = 0, dblklen = 0;
	read_file(img, start_track, start_sector, &data, &dtracks, &dsectors, &dlength, &dblklen);
	log_debug("read file gives %d bytes of data in %d blocks\n", dlength, dblklen);

	// keep track of file blocks from side sectors
	// allocate as much memory as a single super side sector with its side sectors can address
	uint8_t *stracks = malloc(SSS_INDEX_SSB_MAX * SSG_SIDE_SECTORS_MAX * SSB_INDEX_SECTOR_MAX);
	uint8_t *ssectors = malloc(SSS_INDEX_SSB_MAX * SSG_SIDE_SECTORS_MAX * SSB_INDEX_SECTOR_MAX);
	// the list of side sector block addresses
	uint8_t *ssbtracks = malloc(SSS_INDEX_SSB_MAX * SSG_SIDE_SECTORS_MAX);
	uint8_t *ssbsectors = malloc(SSS_INDEX_SSB_MAX * SSG_SIDE_SECTORS_MAX);
	int sblklen = 0;
	// second, read the side sector structure
	uint8_t super[256];
	if (img->di.HasSSB) {
		read_sector(img, ss_track, ss_sector, super);
	
		if (super[SSB_OFFSET_SUPER_254] != 254) {
			faulty = true;
			log_error("Super side sector at %d/%d does not have super side sector "
				"marker 254, has %d instead\n", 
				ss_track, ss_sector, super[SSB_OFFSET_SUPER_254]);
		}

		// the link address is the address of the first side sector group
		if (super[BLK_OFFSET_NEXT_TRACK] != super[SSS_OFFSET_SSB_POINTER]
			|| super[BLK_OFFSET_NEXT_SECTOR] != super[SSS_OFFSET_SSB_POINTER+1]) {
			faulty = true;
			log_error("Super side sector link chain is broken: link is %d/%d,"
				" but first side sector group is %d/%d\n",
				super[BLK_OFFSET_NEXT_TRACK], super[BLK_OFFSET_NEXT_SECTOR],
				super[SSS_OFFSET_SSB_POINTER], super[SSS_OFFSET_SSB_POINTER+1]);
		}

		// note: this reads in the complete side sector block chain (i.e. all side sectors)
		append_ssg(img, super[SSS_OFFSET_SSB_POINTER], super[SSS_OFFSET_SSB_POINTER+1], 
				&stracks, &ssectors, &sblklen, reclen, &ssbtracks, &ssbsectors,
				dblklen);

		// now compare the entry points in that file each six side sectors with the values
		// in the super side sector
		int numssg = 1 + sblklen / SSB_INDEX_SECTOR_MAX;
		int ssp = SSS_OFFSET_SSB_POINTER;
		for (int i = 0; i < numssg; i+=SSG_SIDE_SECTORS_MAX) {
			//log_debug("checking ssp %d\n", i);
			if (super[ssp] != ssbtracks[i] || super[ssp+1] != ssbsectors[i]) {
				faulty = true;
				log_error("Super side sector pointer #%d is wrong, "
					"should be %d/%d (@%d), but is %d/%d (@%d)\n",
					(ssp - SSS_OFFSET_SSB_POINTER) >> 1,
					255 & ssbtracks[i], 255 & ssbsectors[i], i,
					255 & super[ssp], 255 & super[ssp+1], ssp);
			}
			ssp += 2;
		}
	} else {
		append_ssg(img, ss_track, ss_sector, &stracks, &ssectors, &sblklen, reclen, 
				&ssbtracks, &ssbsectors, dblklen);
	}

	// now compare the block lists between file chain and side sectors
	int i = 0;
	do {
		if (stracks[i] == dtracks[i] 
			&& ssectors[i] == dsectors[i]) {
			log_debug("%04d: %3d %2d - %3d %2d ok\n",
				i, dtracks[i], dsectors[i], stracks[i], ssectors[i]);
		} else {
			faulty = true;
			log_error("%04d: %3d %2d - %3d %2d ERROR\n",
				i, dtracks[i], dsectors[i], stracks[i], ssectors[i]);
		}
		i++;
	} while (i < dblklen && i < sblklen);
	
	if (dblklen != sblklen) {
		faulty = true;
		log_error("Error in file lengths: file is %d blocks, side sectors have %d blocks\n",
			dblklen, sblklen);
	}

	if(dump) {
		// dump each record in the file
		int recno = 1;
		uint8_t *p;
		int j;
		for (int i = 0; i < dlength; recno++, i += reclen) {
		
			p = data + i;

			for (j = reclen-1; j >= 0; j--) {
				if (p[j] != 0) {
					break;
				}
			}
			
			if (j < 0) {
				// empty record
				log_info("rec# %d: empty\n", recno);
			} else {
				j++;
				log_info("rec# %d: %d bytes\n", recno, j);
				log_hexdump((char*)p, j, 1);
			}
		}
	}

	if(!testing && (faulty || bad_relfile)) {
		color_log_error();
		printf("BAD ");
		color_default();
	}

	return (faulty || bad_relfile ? -1 : 0);
}
