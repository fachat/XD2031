/****************************************************************************

    Commodore disk image Serial line server
    Copyright (C) 2013 Edilbert Kirk, Andre Fachat

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
 * This file is a disk image provider implementation
 *
 * In this file the actual command work is done for
 * Commodore disk images of type d64, d71, d80, d81, d82
 */

#include "os.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <assert.h>
#include <stdbool.h>

#include "provider.h"
#include "dir.h"
#include "handler.h"
#include "errors.h"
#include "mem.h"
#include "wireformat.h"
#include "channel.h"
#include "wildcard.h"
#include "openpars.h"

#include "diskimgs.h"

#include "log.h"

#undef DEBUG_READ
#define DEBUG_CMD

#define	ALLOC_LINEAR		0
#define	ALLOC_INTERLEAVE	1
#define	ALLOC_FIRST_BLOCK	2
#define	ALLOC_SIDE_SECTOR	3

// structure for directory slot handling

typedef struct {
	unsigned int number;	// current slot number
	unsigned int pos;	// file position
	unsigned int size;	// file size in (254 byte) blocks
	uint8_t dir_sector;	// sector in which the slot resides
	uint8_t next_track;	// next directory track
	uint8_t next_sector;	// next directory sector
	uint8_t filename[20];	// filename (C string zero terminated)
	uint8_t type;		// file type
	uint8_t start_track;	// first track
	uint8_t start_sector;	// first sector
	uint8_t ss_track;	// side sector track
	uint8_t ss_sector;	// side sector sector
	uint8_t recordlen;	// REL file record length
	uint8_t eod;		// end of directory
} slot_t;

typedef struct {
	file_t file;
	slot_t Slot;		// 
	uint8_t *buf;		// direct channel block buffer
	uint8_t CBM_file[20];	// filename with CBM charset
	const char *dospattern;	// directory match pattern in PETSCII
	uint8_t is_first;	// is first directory entry?
	uint8_t next_track;
	uint8_t next_sector;
	uint8_t cht;		// chain track
	uint8_t chs;		// chain sector
	uint8_t chp;		// chain pointer
	uint8_t access_mode;
	uint16_t lastpos;	// last P record number + 1, to expand to on write if > 0
	uint16_t maxrecord;	// the last record number available in the file
} File;

typedef struct {		// derived from endpoint_t
	endpoint_t base;	// payload
	file_t *Ip;		// Image file pointer
	//char         *curpath;             // malloc'd current path
	Disk_Image_t DI;	// mounted disk image
	uint8_t *BAM[4];	// Block Availability Maps
	int BAMpos[4];		// File position of BAMs
	uint8_t *buf[5];	// direct channel block buffer
	uint8_t chan[5];	// channel #
	uint8_t bp[5];		// buffer pointer
	uint8_t CurrentTrack;	// start track for scannning of BAM
	uint8_t U2_track;	// track  for U2 command
	uint8_t U2_sector;	// sector for U2 command
	slot_t Slot;		// directory slot - should be deprecated!
} di_endpoint_t;

extern provider_t di_provider;

static registry_t di_endpoint_registry;

handler_t di_file_handler;

// prototypes
static void di_read_BAM(di_endpoint_t * diep);
static void di_write_slot(di_endpoint_t * diep, slot_t * slot);
static void di_dump_file(file_t * fp, int recurse, int indent);

// ------------------------------------------------------------------
// management of endpoints

static void endpoint_init(const type_t * t, void *obj)
{
	(void)t;		// silence unused warning
	di_endpoint_t *fsep = (di_endpoint_t *) obj;
	reg_init(&(fsep->base.files), "di_endpoint_files", 16);
	fsep->base.ptype = &di_provider;
	fsep->base.is_assigned = 0;
	fsep->base.is_temporary = 0;
}

static type_t endpoint_type = {
	"di_endpoint",
	sizeof(di_endpoint_t),
	endpoint_init
};

// **********
// di_init_fp
// **********

static void di_init_fp(const type_t * t, void *obj)
{
	(void)t;
	File *fp = (File *) obj;

	fp->buf = NULL;
	fp->is_first = 0;
	fp->cht = 0;
	fp->chs = 0;
	fp->chp = 0;
	fp->file.handler = &di_file_handler;
	fp->dospattern = NULL;
	fp->lastpos = 0;
	fp->maxrecord = 0;
}

static type_t file_type = {
	"di_file",
	sizeof(File),
	di_init_fp
};

// ***************
// di_reserve_file
// ***************

static File *di_reserve_file(di_endpoint_t * diep)
{
	File *file = mem_alloc(&file_type);
	file->file.endpoint = (endpoint_t *) diep;

	log_debug("di_reserve_file %p\n", file);

	reg_append(&diep->base.files, file);

	return file;
}

// *************
// di_load_image
// *************

static cbm_errno_t di_load_image(di_endpoint_t * diep, file_t * file)
{

	log_debug("image size = %d\n", diep->Ip->filesize);

	size_t filelen = file->handler->realsize(file);

	if (diskimg_identify(&(diep->DI), filelen)) {
		int numbamblocks = diep->DI.BAMBlocks;
		for (int i = 0; i < numbamblocks; i++) {
			diep->BAMpos[i] =
			    256 * diep->DI.LBA(diep->DI.bamts[(i << 1)],
					       diep->DI.bamts[(i << 1) + 1]);
		}
	} else {
		log_error("Invalid/unsupported disk image\n");
		return CBM_ERROR_FILE_TYPE_MISMATCH;	// not an image file
	}

	di_read_BAM(diep);
	log_debug("di_load_image(%s) as d%d\n", file->filename, diep->DI.ID);
	return CBM_ERROR_OK;	// success
}

// ********
// di_newep
// ********

static endpoint_t *di_newep(const char *path)
{
	di_endpoint_t *diep = mem_alloc(&endpoint_type);

	// register with list of endpoints
	// (note: early registration, but we are single-threaded...)
	reg_append(&di_endpoint_registry, diep);

	log_debug("di_newep(%s) = %p\n", path, diep);

	return (endpoint_t *) diep;
}

// *********
// di_freeep
// *********

static void di_freeep(endpoint_t * ep)
{
	log_debug("di_freeep(%p)\n", ep);

	di_endpoint_t *cep = (di_endpoint_t *) ep;
	if (reg_size(&ep->files)) {
		log_warn
		    ("di_freeep(): trying to close endpoint %p with %d open files!\n",
		     ep, reg_size(&ep->files));
		return;
	}
	if (ep->is_assigned > 0) {
		log_warn("Endpoint %p is still assigned\n", ep);
		return;
	}
	// remove from list of endpoints
	reg_remove(&di_endpoint_registry, cep);

	// close/free resources
	if (cep->Ip != NULL) {
		cep->Ip->handler->close(cep->Ip, 1);
		cep->Ip = NULL;
	}

	mem_free(ep);
}

static void di_ep_free(endpoint_t * ep)
{

	if (ep->is_assigned > 0) {
		ep->is_assigned--;
	}

	if (ep->is_assigned == 0) {
		di_freeep(ep);
	}
}

// ------------------------------------------------------------------
// adapter methods to handle indirection via file_t instead of FILE*
// note: this provider reads/writes single bytes in many cases
// always(?) preceeded by a seek, so there is room for improvements

// OLD Style (deprectated)

static inline cbm_errno_t di_fseek(file_t * file, long pos, int whence)
{
	return file->handler->seek(file, pos, whence);
}

static inline void di_fread(void *ptr, size_t size, size_t nmemb, file_t * file)
{
	int readfl;
	file->handler->readfile(file, (char *)ptr, size * nmemb, &readfl);
}

static inline void
di_fwrite(void *ptr, size_t size, size_t nmemb, file_t * file)
{
	file->handler->writefile(file, (char *)ptr, size * nmemb, 0);
}

static inline int di_fflush(file_t * file)
{
	di_endpoint_t *diep = (di_endpoint_t *) file->endpoint;

	return diep->Ip->handler->flush(diep->Ip);
}

static inline void di_fsync(file_t * file)
{
	// TODO
	// if(res) log_error("os_fsync failed: (%d) %s\n", os_errno(), os_strerror(os_errno()));

	file->handler->flush(file);
}

// NEW style

// the new style centers around a buffer for a disk block. A buf_t struct describes
// the buffer. The API assumes that a buffer is reserved for a specific track/sector(!)
// The API uses (buf_t **) in/out params, so you get the error number as return code, and the
// buffer in the out parameter. If you input a buf_t, it will be reused.
// The idea is that the shell can reuse and existing in-memory buffer that way.
// A buffer must be returned via FREBUF when not needed anymore

typedef struct {
	di_endpoint_t *diep;
	uint8_t *buf;
	uint8_t dirty;
	uint8_t track;
	uint8_t sector;
} buf_t;

static void buf_init(const type_t * t, void *obj)
{
	(void)t;		// silence unused warning
	buf_t *p = (buf_t *) obj;
	p->dirty = 0;
	p->track = 0;
	p->sector = 0;
	p->buf = NULL;
}

static type_t buf_type = {
	"di_buf_t",
	sizeof(buf_t),
	buf_init
};

/*
 * get a detached buffer; can be set (with SETBUF) to any t/s address and written to
 */
static cbm_errno_t di_GETBUF(buf_t ** bufp, di_endpoint_t * diep)
{

	buf_t *p = *bufp;

	if (p == NULL) {
		p = mem_alloc(&buf_type);
		p->buf = mem_alloc_c(256, "di_buf_block");
		log_debug("GETBUF() -> new buffer at %p\n", p);
		*bufp = p;
	} else {
		log_debug("GETBUF() -> reuse buffer at %p\n", p);
	}

	p->diep = diep;
	p->dirty = 0;
	p->track = 0;
	p->sector = 0;

	return CBM_ERROR_OK;
}

/*
 * prepare a buffer for writing to the given t/s
 */
static void di_SETBUF(buf_t * bufp, uint8_t track, uint8_t sector)
{

	bufp->track = track;
	bufp->sector = sector;
	bufp->dirty = 0;
}

static cbm_errno_t di_RDBUF(buf_t * bufp)
{

	cbm_errno_t err;
	int readfl;
	di_endpoint_t *diep = bufp->diep;
	file_t *file = diep->Ip;

	long seekpos = 256 * diep->DI.LBA(bufp->track, bufp->sector);
	err = file->handler->seek(file, seekpos, SEEKFLAG_ABS);
	if (err == CBM_ERROR_OK) {
		// TODO: error on read?
		file->handler->readfile(file, (char *)(bufp->buf), 256,
					&readfl);
	}

	bufp->dirty = 0;

	log_debug("RDBUF(%d,%d (%p)) -> %d\n", bufp->track, bufp->sector, bufp,
		  err);

	return err;
}

static cbm_errno_t di_WRBUF(buf_t * p)
{

	cbm_errno_t err;
	di_endpoint_t *diep = p->diep;
	file_t *file = diep->Ip;

	long seekpos = 256 * diep->DI.LBA(p->track, p->sector);
	err = file->handler->seek(file, seekpos, SEEKFLAG_ABS);
	if (err == CBM_ERROR_OK) {
		// TODO: error on read?
		file->handler->writefile(file, (char *)(p->buf), 256, 0);
	}

	p->dirty = 0;

	log_debug("WRBUF(%d,%d (%p)) -> %d\n", p->track, p->sector, p, err);

	return err;
}


/* 
 * get a (possibly) mapped buffer. Can only be used to read
 * t/s address. Although mapped buffer can be set dirty and written later
 */
static cbm_errno_t di_FLUSH(buf_t * bufp)
{
	cbm_errno_t err = CBM_ERROR_OK;

	if (bufp->dirty) {
		err = di_WRBUF(bufp);
	}

	return err;
}

/* 
 * get a (possibly) mapped buffer. Can only be used to read
 * t/s address. Although mapped buffer can be set dirty and written later
 */
static inline void di_DIRTY(buf_t * bufp)
{
	bufp->dirty = 1;
}

/* 
 * reuse a buffer if t/s match. Otherwise flush (write) dirty buffers and
 * map the new one t/s
 */
static cbm_errno_t di_REUSEFLUSHMAP(buf_t * bufp, uint8_t track, uint8_t sector)
{
	cbm_errno_t err = CBM_ERROR_OK;

	if (bufp->track != track || bufp->sector != sector) {

		if (bufp->dirty) {
			err = di_WRBUF(bufp);
		}
		if (err == CBM_ERROR_OK) {
			
			bufp->track = track;
			bufp->sector = sector;

			err = di_RDBUF(bufp);
		}
	}
	return err;
}

/* 
 * get a (possibly) mapped buffer. Can only be used to read
 * t/s address. Although mapped buffer can be set dirty and written later
 */
static cbm_errno_t di_MAPBUF(buf_t * bufp, uint8_t track, uint8_t sector)
{

	bufp->track = track;
	bufp->sector = sector;

	return di_RDBUF(bufp);
}

static void di_FREBUF(buf_t ** bufp)
{

	buf_t *p = *bufp;

	log_debug("FREBUF(%d,%d (%p))\n", p == NULL ? 0 : p->track,
		  p == NULL ? 0 : p->sector, p);

	if (p != NULL) {
		p->diep = NULL;
		if (p->buf != NULL) {
			mem_free(p->buf);
			p->buf = NULL;
		}
		mem_free(p);
	}
	*bufp = NULL;
}

// ------------------------------------------------------------------
// Track/sector calculations and checks

// ************
// di_assert_ts
// ************

static int di_assert_ts(di_endpoint_t * diep, uint8_t track, uint8_t sector)
{
	if (diep->DI.LBA(track, sector) < 0)
		return CBM_ERROR_ILLEGAL_T_OR_S;
	return CBM_ERROR_OK;
}

// ************
// di_fseek_tsp
// ************

static void
di_fseek_tsp(di_endpoint_t * diep, uint8_t track, uint8_t sector, uint8_t ptr)
{
	long seekpos = ptr + 256 * diep->DI.LBA(track, sector);
	log_debug
	    ("di_fseek_tsp: diep=%p, seeking to position %ld (0x%lx) for t/s/p=%d/%d/%d\n",
	     diep, seekpos, seekpos, track, sector, ptr);
	di_fseek(diep->Ip, seekpos, SEEKFLAG_ABS);
}

// ************
// di_fseek_pos
// ************

static void di_fseek_pos(di_endpoint_t * diep, int pos)
{
	log_debug("di_fseek_pos: diep=%p, pos=%d (0x%x)\n", diep, pos, pos);

	di_fseek(diep->Ip, pos, SEEKFLAG_ABS);
}

// ************
// check position - make sure cht/chs are valid
// ************

static int di_update_chx(di_endpoint_t * diep, File * f, int seek)
{
	if (f->chp > 253) {
		f->chp = 0;
		if (f->next_track == 0) {
			return READFLAG_EOF;	// EOF
		}
		di_fseek_tsp(diep, f->next_track, f->next_sector, 0);
		f->cht = f->next_track;
		f->chs = f->next_sector;
		di_fread(&f->next_track, 1, 1, diep->Ip);
		di_fread(&f->next_sector, 1, 1, diep->Ip);
		// log_debug("this block: (%d/%d)\n",f->cht,f->chs);
		// log_debug("next block: (%d/%d)\n",f->next_track,f->next_sector);
	} else {
		if (seek) {
			di_fseek_tsp(diep, f->cht, f->chs, 2 + f->chp);
		}
	}
	return 0;
}

// ------------------------------------------------------------------
// BAM handling

// ***********
// di_sync_BAM
// ***********

static void di_sync_BAM(di_endpoint_t * diep)
{
	int i;

	for (i = 0; i < diep->DI.BAMBlocks; ++i) {
		di_fseek_pos(diep, diep->BAMpos[i]);
		di_fwrite(diep->BAM[i], 1, 256, diep->Ip);
	}
}

// ***********
// di_read_BAM
// ***********

static void di_read_BAM(di_endpoint_t * diep)
{
	int i;

	for (i = 0; i < diep->DI.BAMBlocks; ++i) {
		if (diep->BAM[i]) {
			// free memory
			free(diep->BAM[i]);
		}
		diep->BAM[i] = (uint8_t *) malloc(256);
		di_fseek_pos(diep, diep->BAMpos[i]);
		di_fread(diep->BAM[i], 1, 256, diep->Ip);
	}
}

// ***********
// di_scan_BAM
// ***********

// linearly scan to find a new sector for a file, for B-A
// do NOT allocate the block
// mimic GETSEC (8250 DOS at $FA35)
// note that it seems the last sector of a track can not be allocated here...
static int
di_scan_BAM_GETSEC(Disk_Image_t * di, uint8_t * bam, uint8_t track,
		   uint8_t firstSector)
{
	uint8_t s = firstSector;	// sector index
	int lastsector = di->LSEC(track);

	while (s <= lastsector) {
		if (bam[s >> 3] & (1 << (s & 7))) {
			return s;
		}
		s = s + 1;
	}
	return -1;		// no free sector
}

// do allocate a block
static void di_alloc_BAM(uint8_t * bam, uint8_t sector)
{
	bam[sector >> 3] &= ~(1 << (sector & 7));
}

// calculate the position of the BAM entry for a given track
static void
di_calculate_BAM(di_endpoint_t * diep, uint8_t Track, uint8_t ** outBAM,
		 uint8_t ** outFbl)
{
	int BAM_Number;		// BAM block for current track
	int BAM_Offset;
	int BAM_Increment;
	Disk_Image_t *di = &diep->DI;
	uint8_t *fbl;		// pointer to track free blocks
	uint8_t *bam;		// pointer to BAM bit field

	BAM_Number = (Track - 1) / di->TracksPerBAM;
	BAM_Offset = di->BAMOffset;	// d64=4  d80=6  d81=16
	BAM_Increment = 1 + ((di->Sectors + 7) >> 3);
	fbl = diep->BAM[BAM_Number] + BAM_Offset
	    + ((Track - 1) % di->TracksPerBAM) * BAM_Increment;
	bam = fbl + 1;		// except 1571 2nd. side
	if (di->ID == 71 && Track > di->Tracks) {
		fbl = diep->BAM[0] + 221 + (Track - 36);
		bam = diep->BAM[1] + 3 * (Track - 36);
	}
	*outFbl = fbl;
	*outBAM = bam;
}

// **************
// di_block_alloc
// **************
// try to allocate a given block (T/S). If this block is
// already allocated, return a 65 error with the first free
// t/s following the requested t/s
// Basically this is B-A
//
// find a free block, starting from the given t/s
// searching linearly in each track. Reserve only if the given t/s is
// free, otherwise return the found ones with error 65
//
static int
di_block_alloc(di_endpoint_t * diep, uint8_t * req_track, uint8_t * req_sector)
{
	int sector;		// sector of next free block
	int sectorfound;
	int track;
	int lasttrack;
	Disk_Image_t *di = &diep->DI;
	uint8_t *fbl;		// pointer to track free blocks
	uint8_t *bam;		// pointer to track BAM

	lasttrack = di->Tracks * di->Sides;
	sector = *req_sector;
	track = *req_track;
	sectorfound = -1;

	if (track == 0 || track > lasttrack) {
		return CBM_ERROR_ILLEGAL_T_OR_S;
	}

	while (track <= lasttrack) {
		di_calculate_BAM(diep, track, &bam, &fbl);
		if (fbl[0]) {
			// number of free blocks in track is not null
			sectorfound =
			    di_scan_BAM_GETSEC(di, bam, track, sector);
			// but the free sector may well be below the start sector 
			// as GETSEC only searches up, GETSEC may thus still fail
			// note: we can overwrite sector, as on failure, it will
			// still be set to 0 below
			if (sectorfound >= 0) {
				break;
			}
		}
		sector = 0;
		track++;
	}

	if (sectorfound >= 0) {
		if (*req_track == track && *req_sector == sectorfound) {
			// found the requested one, allocate it
			fbl[0]--;	// decrease free block counter
			di_alloc_BAM(bam, sectorfound);
			di_sync_BAM(diep);
			log_debug
			    ("di_block_alloc: found %d/%d to alloc, BAM at pos %d\n",
			     track, sectorfound, bam - diep->BAM[0]);
			return CBM_ERROR_OK;
		}
		// we found another block
		*req_track = track;
		*req_sector = sectorfound;
		return CBM_ERROR_NO_BLOCK;
	}
	*req_sector = 0;
	*req_track = 0;

	return CBM_ERROR_NO_BLOCK;
}

// ******************
// di_find_free_block
// ******************

// simulates the INTTS algorithm of CBM DOS, 
// i.e. allocating the first data sector of a file
// return CBM_ERROR_*
static int di_find_free_block_INTTS(di_endpoint_t * diep, uint8_t * out_track,
				    uint8_t * out_sector)
{
	int track;
	int lasttrack;
	int sector;
	int counter;
	uint8_t *bam;
	uint8_t *fbl;

	Disk_Image_t *di = &diep->DI;

	lasttrack = di->Tracks * di->Sides;
	sector = -1;
	counter = 1;

	// alternating search first "below" then "above" dir track
	do {
		track = di->DirTrack - counter;

		if (track > 0) {
			// below dir track
			di_calculate_BAM(diep, track, &bam, &fbl);
			if (fbl[0]) {
				// number of free blocks in track not null
				break;
			}
		}

		track = di->DirTrack + counter;

		if (track <= lasttrack) {
			di_calculate_BAM(diep, track, &bam, &fbl);
			if (fbl[0]) {
				// number of free blocks in track not null
				break;
			}
		}

		counter++;

	}
	while (track <= lasttrack);

	if (track <= lasttrack) {
		// found one
		sector = di_scan_BAM_GETSEC(di, bam, track, 0);
		fbl[0]--;	// decrease free block counter
		di_alloc_BAM(bam, sector);
		diep->CurrentTrack = track;
		di_sync_BAM(diep);
		*out_track = track;
		*out_sector = sector;

		log_debug("di_find_free_block_INTTS () -> (%d,%d)\n", track,
			  sector);

		return CBM_ERROR_OK;
	}
	return CBM_ERROR_DISK_FULL;	// No free block -> DISK FULL
}

// simulates the NXTTS algorithm of CBM DOS, 
// i.e. allocating the "next" data sector of a file
static int
di_find_free_block_NXTTS(di_endpoint_t * diep, uint8_t * inout_track,
			 uint8_t * inout_sector)
{
	int track;
	int dirtrack;
	int lasttrack;
	int lastsector;
	int sector;
	int interleave;
	int counter;
	uint8_t *bam;
	uint8_t *fbl;

	Disk_Image_t *di = &diep->DI;

	dirtrack = di->DirTrack;
	lasttrack = di->Tracks * di->Sides;

	track = *inout_track;
	sector = *inout_sector;

	if (track == 0) {
		log_error("NXTTS: track is 0!\n");
	}

	if (track == di->DirTrack) {
		interleave = di->DirInterleave;
		counter = 1;
	} else {
		interleave = di->DatInterleave;
		counter = 3;
	}

	// search from current position out, then from dir into other direction, then from
	// dir in original direction
	do {
		di_calculate_BAM(diep, track, &bam, &fbl);
		if (fbl[0]) {
			// number of free blocks in track not null
			break;
		}

		if (track == dirtrack) {
			// we are done
			counter = 0;
			break;
		}
		if (track < dirtrack) {
			// below dir track
			track--;
			if (track == 0) {
				sector = 0;
				track = dirtrack + 1;
				counter--;
				continue;
			}
		} else {
			track++;
			if (track > lasttrack) {
				sector = 0;
				track = dirtrack - 1;
				counter--;
				continue;
			}
		}
	}
	while (counter > 0);

	if (counter > 0) {
		// found one
		// number of free blocks in track not null
		sector += interleave;

		lastsector = di->LSEC(track);
		if (sector > lastsector) {
			sector -= lastsector;
			if (sector > 0) {
				sector--;
			}
		}
		sector = di_scan_BAM_GETSEC(di, bam, track, sector);
		if (sector < 0) {
			sector = di_scan_BAM_GETSEC(di, bam, track, 0);
		}

		fbl[0]--;	// decrease free block counter
		di_alloc_BAM(bam, sector);
		di_sync_BAM(diep);
		diep->CurrentTrack = track;

		log_debug
		    ("di_find_free_block_NXTTS (diep=%p, %d/%d, intrlv=%d, lstsec=%d, bam=%02x %02x %02x %02x) -> (%d,%d)\n",
		     diep, *inout_track, *inout_sector, interleave, lastsector,
		     bam[0], bam[1], bam[2], bam[3], track, sector);

		*inout_track = track;
		*inout_sector = sector;

		return CBM_ERROR_OK;
	}
	return CBM_ERROR_DISK_FULL;	// No free block -> DISK FULL
}

// ------------------------------------------------------------------
// direct/block code

// ***************
// di_alloc_buffer
// ***************

static int di_alloc_buffer(di_endpoint_t * diep)
{
	log_debug("di_alloc_buffer 0\n");
	if (!diep->buf[0])
		diep->buf[0] = (uint8_t *) calloc(256, 1);
	if (!diep->buf[0])
		return 0;	// OOM
	diep->bp[0] = 0;
	return 1;		// success
}

// **************
// di_load_buffer
// **************

static int di_load_buffer(di_endpoint_t * diep, uint8_t track, uint8_t sector)
{
	if (!di_alloc_buffer(diep))
		return 0;	// OOM
	log_debug("di_load_buffer %p->%p U1(%d/%d)\n", diep, diep->buf[0],
		  track, sector);
	di_fseek_tsp(diep, track, sector, 0);
	di_fread(diep->buf[0], 1, 256, diep->Ip);
	// di_dump_block(diep->buf[0]);
	return 1;		// OK
}

// **************
// di_save_buffer
// **************

static int di_save_buffer(di_endpoint_t * diep)
{
	log_debug("di_save_buffer U2(%d/%d)\n", diep->U2_track,
		  diep->U2_sector);
	di_fseek_tsp(diep, diep->U2_track, diep->U2_sector, 0);
	di_fwrite(diep->buf[0], 1, 256, diep->Ip);
	di_fsync(diep->Ip);
	diep->U2_track = 0;
	// di_dump_block(diep->buf[0]);
	return 1;		// OK
}

// **************
// di_flag_buffer
// **************

static void di_flag_buffer(di_endpoint_t * diep, uint8_t track, uint8_t sector)
{
	log_debug("di_flag_buffer U2(%d/%d)\n", track, sector);
	diep->U2_track = track;
	diep->U2_sector = sector;
	diep->bp[0] = 0;
}

// *************
// di_read_block
// *************

static int
di_read_block(di_endpoint_t * diep, File * file, char *retbuf, int len,
	      int *eof)
{
	log_debug("di_read_block: chan=%p len=%d\n", file, len);

	int avail = 256 - diep->bp[0];
	int n = len;
	if (len > avail) {
		n = avail;
		*eof = READFLAG_EOF;
	}
	log_debug("di_read_block: avail=%d, n=%d\n", avail, n);
	if (n > 0) {
		log_debug("memcpy(%p,%p,%d)\n", retbuf,
			  diep->buf[0] + diep->bp[0], n);
		memcpy(retbuf, diep->buf[0] + diep->bp[0], n);
		diep->bp[0] += n;
	}
	return n;
}

// **************
// di_write_block
// **************

static int di_write_block(di_endpoint_t * diep, const char *buf, int len)
{
	log_debug("di_write_block: len=%d at ptr %d\n", len, diep->bp[0]);

	int avail = 256 - diep->bp[0];
	int n = len;
	if (len > avail)
		n = avail;
	if (n > 0) {
		memcpy(diep->buf[0] + diep->bp[0], buf, n);
		diep->bp[0] += n;
	}
	return n;
}

// *************
// di_block_free
// *************

static int di_block_free(di_endpoint_t * diep, uint8_t Track, uint8_t Sector)
{
	uint8_t *fbl;		// pointer to track free blocks
	uint8_t *bam;		// pointer to track BAM

	log_debug("di_block_free(%d,%d)\n", Track, Sector);

	di_calculate_BAM(diep, Track, &bam, &fbl);

	if (!(bam[Sector >> 3] & (1 << (Sector & 7))))	// allocated ?
	{
		++fbl[0];	// increase # of free blocks on track
		bam[Sector >> 3] |= (1 << (Sector & 7));	// mark as free (1)

		di_sync_BAM(diep);

		return CBM_ERROR_OK;
	}

	return CBM_ERROR_OK;
}

// *********
// di_direct
// *********

static int
di_direct(endpoint_t * ep, const char *buf, char *retbuf, int *retlen)
{
	int rv = CBM_ERROR_OK;

	di_endpoint_t *diep = (di_endpoint_t *) ep;
	file_t *fp = NULL;
	File *file = NULL;

	buf--;

	uint8_t cmd = (uint8_t) buf[FS_BLOCK_PAR_CMD];
	uint8_t track = (uint8_t) buf[FS_BLOCK_PAR_TRACK];	// ignoring high byte
	uint8_t sector = (uint8_t) buf[FS_BLOCK_PAR_SECTOR];	// ignoring high byte
	uint8_t chan = (uint8_t) buf[FS_BLOCK_PAR_CHANNEL];

	log_debug("di_direct(cmd=%d, tr=%d, se=%d ch=%d (ep=%p, '%s')\n", cmd,
		  track, sector, chan, ep, ep->ptype->name);
	rv = di_assert_ts(diep, track, sector);
	if (buf[FS_BLOCK_PAR_TRACK + 1] != 0
	    || buf[FS_BLOCK_PAR_SECTOR + 1] != 0 || rv != CBM_ERROR_OK) {
		retbuf[0] = track;	// low byte
		retbuf[1] = buf[FS_BLOCK_PAR_TRACK + 1];	// high byte
		retbuf[2] = sector;	// low byte
		retbuf[3] = buf[FS_BLOCK_PAR_SECTOR + 1];	// high byte
		*retlen = 4;
		return rv;	// illegal track or sector
	}

	switch (cmd) {
	case FS_BLOCK_BR:
	case FS_BLOCK_U1:
		di_load_buffer(diep, track, sector);
		diep->chan[0] = chan;	// assign channel # to buffer

		//handler_resolve_block(ep, chan, &fp);
		file = mem_alloc(&file_type);
		file->file.endpoint = ep;
		file->access_mode = FS_BLOCK;
		fp = (file_t *) file;

		channel_set(chan, fp);
		break;
	case FS_BLOCK_BW:
	case FS_BLOCK_U2:
		if (!di_alloc_buffer(diep)) {
			return CBM_ERROR_NO_CHANNEL;	// OOM
		}
		di_flag_buffer(diep, track, sector);
		diep->chan[0] = chan;	// assign channel # to buffer
		//handler_resolve_block(ep, chan, &fp);

		file = mem_alloc(&file_type);
		file->file.endpoint = ep;
		file->access_mode = FS_BLOCK;
		fp = (file_t *) file;

		channel_set(chan, fp);
		break;
	case FS_BLOCK_BA:
		rv = di_block_alloc(diep, &track, &sector);
		break;
	case FS_BLOCK_BF:
		rv = di_block_free(diep, track, sector);
		break;
	}

	retbuf[0] = track;	// low byte
	retbuf[1] = 0;		// high byte
	retbuf[2] = sector;	// low byte
	retbuf[3] = 0;		// high byte
	*retlen = 4;

	return rv;
}

// ------------------------------------------------------------------
// REL file handling

// *************
// di_pos_append
// *************

static void di_pos_append(di_endpoint_t * diep, File * f)
{
	uint8_t t, s, nt, ns;

	nt = f->next_track;
	ns = f->next_sector;
	while (nt) {
		t = nt;
		s = ns;
		di_fseek_tsp(diep, t, s, 0);
		di_fread(&nt, 1, 1, diep->Ip);
		di_fread(&ns, 1, 1, diep->Ip);
	}
	f->next_track = 0;
	f->next_sector = ns;
	f->cht = t;
	f->chs = s;
	f->chp = ns - 1;
	log_debug("di_pos_append (%d/%d) %d\n", t, s, ns);
}

//***********
// di_rel_navigate
//***********
//
// find the record_max value to know when we have to expand a REL file
//
// If the given targetrec is larger than the one found, the file is expanded
static int di_rel_navigate(di_endpoint_t * diep,
			   uint8_t * ss_track, uint8_t * ss_sector,
			   uint8_t * dt_track, uint8_t * dt_sector,
			   uint8_t recordlen,
			   unsigned int targetrec, unsigned int *wasrecord,
			   unsigned int *allocated);

static unsigned int di_rel_record_max(di_endpoint_t * diep, File * f)
{

	unsigned int numrecs = 0;
	unsigned int allocated = 0;

	// navigate the super/side sectors, find how many records are there (return in outparam numrecs)
	// then expand the file to the given number of records  
	di_rel_navigate(diep, &f->Slot.ss_track, &f->Slot.ss_sector, NULL, NULL,
			f->file.recordlen, 1, &numrecs, &allocated);

	return numrecs;
}

// navigate the super/side sectors, find how many records are there (return in outparam numrecs)
// then expand the file to the given number of records  
// combines the original di_rel_record_max and di_expand_rel methods into a single, optimized (and unified)
// code path.
//
// returns CBM_ERROR_* code
// returns super side sector (for disks that have one) or first side sector group block address in *ss_
// returns the (first) allocated data block in *dt_, for the file creation; dt_* may be NULL
//
static int di_rel_navigate(di_endpoint_t * diep,
			   uint8_t * ss_track, uint8_t * ss_sector,
			   uint8_t * dt_track, uint8_t * dt_sector,
			   uint8_t recordlen,
			   unsigned int targetrec, unsigned int *wasrecord,
			   unsigned int *blocks)
{
	int err = CBM_ERROR_OK;

	// tmp index
	uint8_t o;

	// number of side sectors in group
	uint8_t side = 0;

	// super side sector address
	uint8_t super_track = 0;
	uint8_t super_sector;
	uint8_t super_pos = 0;

	// side sector address
	uint8_t side_track = 0;
	uint8_t side_sector;
	uint8_t side_pos = 0;

	// file sector address
	uint8_t data_track = 0;
	uint8_t data_sector;
	uint8_t data_pos = 0;

	// for additional data block due to DOS bug
	uint8_t data2_track = 0;
	uint8_t data2_sector;

	// last t/s of file block chain
	uint8_t last_track = 0;
	uint8_t last_sector;

	uint8_t next_track = 0;
	uint8_t next_sector;

	// return value
	unsigned int file_size = 0;
	unsigned int numrecords = 0;
	unsigned int side_blocks = 0;
	unsigned int data_blocks = 0;

	// position in record   
	uint8_t rec_pos = 0;

	log_debug("di_navigate ss: %d/%d, reclen=%d, target=%d\n", *ss_track,
		  *ss_sector, recordlen, targetrec);

	// buffer to use
	buf_t *superp = NULL;
	buf_t *sidep = NULL;
	buf_t *datap = NULL;

	di_GETBUF(&superp, diep);
	di_GETBUF(&sidep, diep);
	di_GETBUF(&datap, diep);

	if (*ss_track == 0) {
		// not a REL file or a new one
		goto end;
	}
	// read super sector or first side sector
	if (diep->DI.HasSSB) {

		super_track = *ss_track;
		super_sector = *ss_sector;

		// read the super sector
		di_MAPBUF(superp, super_track, super_sector);

		// sector is a super side sector
		// count how many side sector groups are used -> groups
		o = SSS_OFFSET_SSB_POINTER;
		for (super_pos = 0;
		     super_pos < SSS_INDEX_SSB_MAX && superp->buf[o] != 0;
		     super_pos++, o += 2) ;
		// now super_pos points to the first entry with track == 0

		if (super_pos == 0) {
			// no side sector group yet
			goto end;
		}
		// last side sector group 
		side_track =
		    superp->buf[SSS_OFFSET_SSB_POINTER +
				((super_pos - 1) << 1)];
		side_sector =
		    superp->buf[SSS_OFFSET_SSB_POINTER +
				((super_pos - 1) << 1) + 1];

		di_MAPBUF(sidep, side_track, side_sector);
	} else {
		super_pos = 1;

		side_track = *ss_track;
		side_sector = *ss_sector;

		di_MAPBUF(sidep, side_track, side_sector);
	}

	// now sidep contains the first block of the last side sector group
	// find last sector in side sector group (guaranteed to find)
	o = SSB_OFFSET_SSG;
	for (side = 0; side < SSG_SIDE_SECTORS_MAX && sidep->buf[o] != 0;
	     side++, o += 2) ;
	// side points to the first entry with track == 0

	if (side > 1) {
		// not the first one (which is already in the buffer)
		side_track = sidep->buf[SSB_OFFSET_SSG + ((side - 1) << 1)];
		side_sector = sidep->buf[SSB_OFFSET_SSG + ((side - 1) << 1) + 1];

		di_MAPBUF(sidep, side_track, side_sector);
	}
	// here we have the last side sector of the last side sector chain in the buffer.
	// obtain the last byte of the sector according to the index 
	side_pos =
	    (sidep->buf[BLK_OFFSET_NEXT_SECTOR] + 1 - SSB_OFFSET_SECTOR) / 2;
	// side_pos points to the first entry with track == 0

	// now get the track and sector of the last block
	o = SSB_OFFSET_SECTOR + 2 * (side_pos - 1);
	data_track = sidep->buf[o];
	data_sector = sidep->buf[o + 1];

	// read the last sector of the file
	di_MAPBUF(datap, data_track, data_sector);

 end:
	// calculate the total number of blocks - because we need it for bug2 situation below, to get data_pos
	data_blocks = 0;
	side_blocks = 0;
	if (diep->DI.HasSSB && super_track != 0) {
		side_blocks += 1;			// super side sector
		if (super_pos > 0) {
			side_blocks += (super_pos - 1) * SSG_SIDE_SECTORS_MAX * 1; // side sectors in groups
			data_blocks += (super_pos - 1) * SSG_SIDE_SECTORS_MAX * SSB_INDEX_SECTOR_MAX; // data sectors
		}
	}
	if (side > 0) {
		data_blocks += (side - 1) * SSB_INDEX_SECTOR_MAX;	// data sectors
		side_blocks += (side - 1) * 1;	// side sectors
		data_blocks += side_pos;	// blocks in last side sector
		side_blocks += 1;		// last side sector
	}
	*blocks = side_blocks + data_blocks;

	// check for bug2 situation and truncate if necessary

	if (datap->track != 0) {
		if (datap->buf[0] != 0) {
			// Here datap should contain the last data block of the file.
			// But we still have a followup pointer in the data block in the buffer
			// ("bug2" situation). We don't produce it, but DOS does
			// We do the same as DOS here - we truncate it away, as DOS
			// that discards it and all data written to it
			// in the meantime
	
			log_debug("di_navigate: discarding non-side-sector data block in chain (DOS bug) at %d,%d\n",
					datap->buf[0], datap->buf[1]);

			data_pos = 254 - (data_blocks * 254) % recordlen;

			log_debug("di_navigate: setting data_pos=%d, data_blocks=%d, recordlen=%d\n",
					data_pos, data_blocks, recordlen);

			datap->buf[0] = 0;
			datap->buf[1] = data_pos + 1;

		}  else {

			// number of bytes in this last sector
			data_pos = datap->buf[BLK_OFFSET_NEXT_SECTOR] - 1;
		}
	}

	/* calculate the total bytes based on the number of super side, side
	   sectors, and last byte index */
	file_size = 0;
	if (super_pos > 0) {
		file_size = (super_pos - 1);	// side sector group
	}
	file_size *= SSG_SIDE_SECTORS_MAX;	// times size of SSG in sectors
	if (side > 0) {
		file_size += (side - 1);	// side sector in group
	}
	file_size *= SSB_INDEX_SECTOR_MAX;	// times numbers of sectors per side sector
	if (side_pos > 0) {
		file_size += (side_pos - 1);	// plus sector position in the side sector
	}
	file_size *= 254;	// times bytes per sector
	file_size += data_pos;	// plus bytes in last sector

	// divide by the record length, and get the maximum records 
	// round up to full records
	numrecords = file_size / recordlen;

	
	log_debug
	    ("di_navigate: super_pos=%d, side=%d, side_pos=%d, data_pos=%d reclen=%d -> file_size=%d, blocks=%d, numrecords=%d\n",
	     super_pos, side, side_pos, data_pos, recordlen, file_size,
	     *blocks, numrecords);

	// output
	*wasrecord = numrecords;

	// last block is in datap buffer (or data_track is null)
	last_track = data_track;
	last_sector = data_sector;

	log_debug("di_navigate: expand numrecs=%d, targetrec=%d, data=%d/%d\n",
		  numrecords, targetrec, data_track, data_sector);

	while (numrecords < targetrec) {

		// else expand the file
		log_debug
		    ("di_navigate: expand: super_pos=%d, side=%d, side_pos=%d, data_pos=%d reclen=%d -> blocks=%d, numrecords=%d\n",
		     super_pos, side, side_pos, data_pos, recordlen,
		     *blocks, numrecords);

		// check if there is space left in the super-/side sector structures
		if (side == SSG_SIDE_SECTORS_MAX
		    && side_pos == SSB_INDEX_SECTOR_MAX) {
			// reached the end of the side sector group
			if (super_pos == SSS_INDEX_SSB_MAX || !diep->DI.HasSSB) {
				// either no super side sector or end of super side sector reached
				err = CBM_ERROR_DISK_FULL;
				goto end2;
			}
			// ok, we have reached a full side sector group, we need to create a new one
			//side_track = 0;
		}

		if (numrecords == 0
		    || ((targetrec - numrecords) * recordlen) + data_pos >
		    254) {
			// a new data sector is needed

			data2_track = 0;

			log_debug("di_navigate: allocate new data sector\n");
			if (side_track == 0) {
				// file does not yet exist
				err =
				    di_find_free_block_INTTS(diep, &data_track,
							     &data_sector);
				(*blocks)++;
			} else {
				err =
			    		di_find_free_block_NXTTS(diep, &data_track,
						     &data_sector);
				(*blocks)++;
			}
			if (err != CBM_ERROR_OK)
				goto end2;

			if (side_track == 0) {
				// we need to create the first side sector group for the file

				// first side sector in group
				side = 0;
				// no data block entry yet
				side_pos = 0;

				// start values for NXTTS
				side_track = data_track;
				side_sector = data_sector;

				log_debug
				    ("di_navigate: allocate first side sector group for file\n");
				err =
				    di_find_free_block_NXTTS(diep, &side_track,
							     &side_sector);
				if (err != CBM_ERROR_OK)
					goto end2;
				(*blocks)++;

				di_REUSEFLUSHMAP(sidep, side_track, side_sector);

				// prepare side sector group
				memset(sidep->buf, 0, 256);
				sidep->buf[BLK_OFFSET_NEXT_SECTOR] = SSB_OFFSET_SECTOR - 1;	// no pointer in file
				sidep->buf[SSB_OFFSET_RECORD_LEN] = recordlen;
				// first entry in side sector addresses list in first side sector points to itself
				sidep->buf[SSB_OFFSET_SSG] = side_track;
				sidep->buf[SSB_OFFSET_SSG + 1] = side_sector;

				di_DIRTY(sidep);
			}

			if (diep->DI.HasSSB && super_track == 0) {
				// we need to create a super side sector

				super_track = data_track;
				super_sector = data_sector;
				log_debug
				    ("di_navigate: allocate super side sector\n");
				err =
				    di_find_free_block_NXTTS(diep, &super_track,
							     &super_sector);
				if (err != CBM_ERROR_OK)
					goto end2;
				(*blocks)++;

				di_REUSEFLUSHMAP(superp, super_track, super_sector);

				memset(superp->buf, 0, 256);

				// marker for super side sector
				superp->buf[SSB_OFFSET_SUPER_254] = 254;

				// update supersector - next t/s point to first side sector block
				superp->buf[BLK_OFFSET_NEXT_TRACK] =
				    sidep->buf[SSB_OFFSET_SSG];
				superp->buf[BLK_OFFSET_NEXT_SECTOR] =
				    sidep->buf[SSB_OFFSET_SSG + 1];
				// first side sector group also
				superp->buf[SSS_OFFSET_SSB_POINTER] =
				    sidep->buf[SSB_OFFSET_SSG];
				superp->buf[SSS_OFFSET_SSB_POINTER + 1] =
				    sidep->buf[SSB_OFFSET_SSG + 1];

				di_DIRTY(superp);
			}

			if (side_pos == SSB_INDEX_SECTOR_MAX) {
				// current side sector is full, need another one

				// DOS bug: allocates two blocks, but looses one - we at least don't loose it
				data2_track = data_track;
				data2_sector = data_sector;
				log_debug
				    ("di_navigate: allocate data2 block\n");
				err =
				    di_find_free_block_NXTTS(diep, &data2_track,
							     &data2_sector);
				if (err != CBM_ERROR_OK)
					goto end2;
				(*blocks)++;

				side_track = data_track;
				side_sector = data_sector;
				log_debug
				    ("di_navigate: allocate new side sector\n");
				err =
				    di_find_free_block_NXTTS(diep, &side_track,
							     &side_sector);
				if (err != CBM_ERROR_OK)
					goto end2;
				(*blocks)++;

				uint8_t side_sectors[SSG_SIDE_SECTORS_MAX * 2];

				// update link chain to point to first block of new side sector group
				sidep->buf[0] = side_track;
				sidep->buf[1] = side_sector;

				di_DIRTY(sidep);

				// do we need a new side sector group?
				if (diep->DI.HasSSB
				    && side == SSG_SIDE_SECTORS_MAX) {
					// yes, need a new side sector group
					// side_t/s contain last side sector block of last (=now previous) ss group

					side = 0;

					log_debug
					    ("di_navigate: write link to new %d/%d in last side sector of previous group\n",
					     side_track, side_sector);

					// now prepare for first side sector block in new group
					err = di_REUSEFLUSHMAP(sidep, side_track,
						  side_sector);
					if (err != CBM_ERROR_OK)
						goto end2;

					memset(side_sectors, 0,
					       SSG_SIDE_SECTORS_MAX * 2);

					if (diep->DI.HasSSB) {
						log_debug
						    ("di_navigate: update super side sector @ super_pos=%d\n",
						     super_pos);

						superp->
						    buf[SSS_OFFSET_SSB_POINTER +
							(super_pos * 2)] =
						    side_track;
						superp->
						    buf[SSS_OFFSET_SSB_POINTER +
							(super_pos * 2) + 1] =
						    side_sector;

						di_DIRTY(superp);
					}
				} else if (side < SSG_SIDE_SECTORS_MAX) {
					// current group is not full yet, create a new side sector in it

					// update pointer to new side sector
					sidep->buf[SSB_OFFSET_SSG +
						   (side * 2)] = side_track;
					sidep->buf[SSB_OFFSET_SSG + (side * 2) +
						   1] = side_sector;
					di_DIRTY(sidep);

					log_debug
					    ("di_navigate: write previous last side sector (side=%d -> %d,%d)\n",
					     side, side_track, side_sector);

					// save list of side sectors
					memcpy(side_sectors,
					       &sidep->buf[SSB_OFFSET_SSG],
					       SSG_SIDE_SECTORS_MAX * 2);

					// now update all sectors in the side sector group. We take advantage that 
					// all side sectors contain the list of sectors in that group
					for (o = 0; o + 1 < side; o++) {
						log_debug
						    ("di_navigate: update new side sector in sector (%d,%d) of the group\n",
						     side_sectors[o * 2],
						     side_sectors[o * 2 + 1]);
						err =
						    di_REUSEFLUSHMAP(sidep,
							      side_sectors[o *
									   2],
							      side_sectors[o *
									   2 +
									   1]);
						if (err != CBM_ERROR_OK)
							goto end2;

						// update pointer to new side sector
						sidep->buf[SSB_OFFSET_SSG +
							   (side * 2)] =
						    side_track;
						sidep->buf[SSB_OFFSET_SSG +
							   (side * 2) + 1] =
						    side_sector;

						di_DIRTY(sidep);
					}
					// prepare new side sector
					err = di_REUSEFLUSHMAP(sidep, side_track,
						  side_sector);
					if (err != CBM_ERROR_OK) 
						goto end2;

				} else {
					err = CBM_ERROR_DISK_FULL;
					goto end2;
				}

				// update side sector itself
				memset(sidep->buf, 0, 256);

				sidep->buf[SSB_OFFSET_SECTOR_NUM] = side;

				sidep->buf[SSB_OFFSET_RECORD_LEN] = recordlen;

				// list of side sectors
				memcpy(&sidep->buf[SSB_OFFSET_SSG],
				       side_sectors, SSG_SIDE_SECTORS_MAX * 2);
				// and pointer to self
				log_debug
				    ("di_navigate: set side sector (side=%d -> %d,%d)\n",
				     side, side_track, side_sector);
				sidep->buf[SSB_OFFSET_SSG + (side * 2)] =
				    side_track;
				sidep->buf[SSB_OFFSET_SSG + (side * 2) + 1] =
				    side_sector;

				di_DIRTY(sidep);

				side_pos = 0;
				side++;
			}
			// update side sector with data block address
			sidep->buf[SSB_OFFSET_SECTOR + (side_pos * 2)] =
			    data_track;
			sidep->buf[SSB_OFFSET_SECTOR + (side_pos * 2) + 1] =
			    data_sector;
			// number of sectors in side sector
			sidep->buf[BLK_OFFSET_NEXT_SECTOR] =
			    SSB_OFFSET_SECTOR + side_pos * 2 + 1;

			side_pos++;

			if (data2_track != 0) {
				// DOS bug: allocate another data block, link it, but do not write into side sector
				// TODO: check when the DOS has created such a file, how do we handle it?
				log_debug
				    ("di_navigate: set data2 sector (side_pos=%d -> %d,%d)\n",
				     side_pos, data2_track, data2_sector);
				sidep->buf[SSB_OFFSET_SECTOR + (side_pos * 2)] =
				    data2_track;
				sidep->buf[SSB_OFFSET_SECTOR + (side_pos * 2) +
					   1] = data2_sector;
				sidep->buf[BLK_OFFSET_NEXT_SECTOR] =
				    SSB_OFFSET_SECTOR + side_pos * 2 + 1;
				side_pos++;
			}
			di_DIRTY(sidep);
		}

		// now fill up the data sector

		if (dt_track != NULL)
			*dt_track = data_track;
		if (dt_sector != NULL)
			*dt_sector = data_sector;

		// default if last_track is 0
		next_track = last_track;
		next_sector = last_sector;
		do {
			// next_track is the one we manipulate and it's already in datap
			// if we create the file, last_track is 0, but data_track is not!
			if (next_track != 0) {

				/* Fill new sector with maximum records */
				o = datap->buf[BLK_OFFSET_NEXT_SECTOR] + 1;

				log_debug("di_navigate: writing to datap(%d,%d) at offset %d (rec_pos=%d)\n",
					datap->track, datap->sector, o, rec_pos);

				while (o != 0) {	// wraps over from 255 to 0
					if (rec_pos == 0) {
						datap->buf[o] = 0xff;
					} else {
						datap->buf[o] = 0x00;
					}
					rec_pos = (rec_pos + 1) % recordlen;
					/* increment the maximum records each time we complete a full
					   record. */
					if (rec_pos == 0)
						numrecords++;
					o++;
				}

				if (data_track == 0) {
					/* set as last sector in REL file */
					datap->buf[BLK_OFFSET_NEXT_TRACK] = 0;

					log_debug
					    ("set last sector (%d/%d) size to %d (rec_pos=%d)\n",
					     last_track, last_sector,
					     255 - rec_pos, rec_pos);

					/* Update the last byte pointer based on how much of the last record we
					   filled. */
					datap->buf[BLK_OFFSET_NEXT_SECTOR] =
					    255 - rec_pos;
					rec_pos = 0;

					last_track = next_track;
					last_sector = next_sector;

					data_pos =
					    datap->buf[BLK_OFFSET_NEXT_SECTOR] -
					    1;
				} else {
					datap->buf[BLK_OFFSET_NEXT_TRACK] =
					    data_track;
					datap->buf[BLK_OFFSET_NEXT_SECTOR] =
					    data_sector;
				}

				// write back data sector
				di_DIRTY(datap);
			}
			// shift stack...
			next_track = data_track;
			next_sector = data_sector;
			data_track = data2_track;
			data_sector = data2_sector;
			data2_track = 0;

			if (next_track != 0) {
				di_REUSEFLUSHMAP(datap, next_track, next_sector);
				memset(datap->buf, 0, 256);
				datap->buf[BLK_OFFSET_NEXT_SECTOR] = 1;
			}

		} while (next_track != 0);

		data_track = last_track;
		data_sector = last_sector;

	}			// end while target > numrecords

	// write back metadata (flush what has not yet been written)

	err = di_FLUSH(datap);
	if (err != CBM_ERROR_OK)
		goto end2;

	err = di_FLUSH(sidep);
	if (err != CBM_ERROR_OK)
		goto end2;

	if (diep->DI.HasSSB) {
		err = di_FLUSH(superp);
		if (err != CBM_ERROR_OK)
			goto end2;

		*ss_track = super_track;
		*ss_sector = super_sector;
	} else {
		*ss_track = sidep->buf[SSB_OFFSET_SSG];
		*ss_sector = sidep->buf[SSB_OFFSET_SSG + 1];
	}

 end2:
	*wasrecord = numrecords;

	di_FREBUF(&datap);
	di_FREBUF(&sidep);
	di_FREBUF(&superp);
	return CBM_ERROR_OK;
}

//***********
// di_expand_rel
//***********
//
// internal function to expand a REL file to the given record (so that the record given exists)
// The algorithm adds a sector to the rel file until we meet the required records

static int di_expand_rel(di_endpoint_t * diep, File * f, int recordno)
{

	log_debug("di_expand_rel f=%p to recordno=%d (f->maxrecord=%d)\n", f,
		  recordno, f->maxrecord);

	int err = CBM_ERROR_OK;

	unsigned int wasrecord = 0;
	unsigned int blocks = 0;
	uint8_t track = f->Slot.start_track;
	uint8_t sector = f->Slot.start_sector;
	uint8_t dirty = 0;

	err = di_rel_navigate(diep,
			      &f->Slot.ss_track, &f->Slot.ss_sector,
			      &track, &sector,
			      f->file.recordlen, recordno, &wasrecord,
			      &blocks);

	f->maxrecord = wasrecord;

	if (f->Slot.size != blocks) {
		dirty = 1;
	}
	f->Slot.size = blocks;

	if (f->Slot.start_track == 0) {
		f->Slot.start_track = track;
		f->Slot.start_sector = sector;
		dirty = 1;
	}

	if (dirty) {
		di_write_slot(diep, &f->Slot);
	}

	return err;
}

//***********
// di_position
//***********
//
// Note: algorithm partially taken from VICE's vdrive_rel_track_sector()
//
// Note: record numbers start with 0 (not with 1 as with DOS, this is taken care
// of by the firmware). record 0 does always exist - it is created when the file
// is created. That is why f->lastpos >0 can be used as flag to fill up the file

static int di_position(di_endpoint_t * diep, File * f, int recordno)
{

	uint8_t reclen = 0;

	unsigned int rec_long;	// absolute offset in file
	unsigned int rec_start;	// start of record in block
	unsigned int super, side, byt;	// super side sector index, side sector index
	unsigned int offset;	// temp for the number of file bytes pointed to by blocks in a sss or ss

	cbm_errno_t err = CBM_ERROR_OK;

	buf_t *bufp = NULL;

	log_debug("di_position: set position to record no %d\n", recordno);

	// store position for write in case we exit with error.
	// (will be cleared on success, so also used as flag; note: recordno 0 is treated as 1)
	f->lastpos = recordno + 1;

	// find the block number in file from the record number
	reclen = f->Slot.recordlen;

	// total byte offset (record number starts with 1)
	rec_long = (recordno * reclen);

	// offset in block
	rec_start = rec_long % 254;

	// compute super side sector index (0-125)
	offset = (254 * SSB_INDEX_SECTOR_MAX * SSG_SIDE_SECTORS_MAX);
	super = rec_long / offset;
	rec_long = rec_long % offset;

	// compute side sector index value (0-5)
	offset = (254 * SSB_INDEX_SECTOR_MAX);
	side = rec_long / offset;
	rec_long = rec_long % offset;

	// block number in side sector
	offset = rec_long / 254;
	byt = rec_long % 254;

	log_debug
	    ("di_position: to super=%d, side=%d, offset=%d, byte=%d, lastpos=%d\n",
	     super, side, offset, byt, f->lastpos);

	err = di_GETBUF(&bufp, diep);
	if (err != CBM_ERROR_OK) 
		goto end;

	// -----------------------------------------
	// find the position pointed to in the image
	uint8_t ss_track = f->Slot.ss_track;
	uint8_t ss_sector = f->Slot.ss_sector;

	if (ss_track == 0) {
		// not a REL file
		log_warn("di_position: not a REL file\n");
		err = CBM_ERROR_FILE_TYPE_MISMATCH;
		goto end;
	}

	// read the first side sector
	err = di_MAPBUF(bufp, ss_track, ss_sector);
	if (err != CBM_ERROR_OK) 
		goto end;

	if (bufp->buf[SSB_OFFSET_SUPER_254] == 0xfe) {
		// sector is a super side sector
		// read the address of the first block of the correct side sector chain
		ss_track = bufp->buf[SSS_OFFSET_SSB_POINTER + (super << 1)];
		ss_sector =
		    bufp->buf[SSS_OFFSET_SSB_POINTER + 1 + (super << 1)];

		if (ss_track == 0) {
			// disk image inconsistent
			log_warn("di_position: no side sector chain\n");
			err = CBM_ERROR_RECORD_NOT_PRESENT;
			goto end;
		}
		err = di_MAPBUF(bufp, ss_track, ss_sector);
		if (err != CBM_ERROR_OK) 
			goto end;
	} else {
		// no super side sectors, but sector number too large for single side sector chain
		if (super > 0) {
			log_info("di_position: sector to large for side sector chain\n");
			err = CBM_ERROR_RECORD_NOT_PRESENT;
			goto end;
		}
	}
	// here we have the first side sector of the correct side sector chain in the buffer.
	if (side > 0) {
		// need to read the correct side sector first
		// read side sector number
		ss_track = bufp->buf[SSB_OFFSET_SSG + (side << 1)];
		ss_sector = bufp->buf[SSB_OFFSET_SSG + 1 + (side << 1)];
		if (ss_track == 0) {
			log_debug("di_position: side sector not yet allocated\n");
			// sector in a part of the side sector group that isn't created yet
			err = CBM_ERROR_RECORD_NOT_PRESENT;
			goto end;
		}
		err = di_MAPBUF(bufp, ss_track, ss_sector);
		if (err != CBM_ERROR_OK) 
			goto end;
	}
	// here we have the correct side sector in the buffer.
	ss_track = bufp->buf[SSB_OFFSET_SECTOR + (offset << 1)];
	ss_sector = bufp->buf[SSB_OFFSET_SECTOR + 1 + (offset << 1)];
	if (ss_track == 0) {
		log_debug("di_position: sector not yet allocated\n");
		// sector in a part of the side sector that is not yet created
		err = CBM_ERROR_RECORD_NOT_PRESENT;
		goto end;
	}
	// here we have, in ss_track, ss_sector, and rec_start the tsp position of the
	// record as given in the parameter
	err = di_MAPBUF(bufp, ss_track, ss_sector);
	if (err != CBM_ERROR_OK) 
		goto end;

	// is there enough space on the last block?
	if (bufp->buf[0] == 0 && bufp->buf[1] < (rec_start + reclen + 1)) {
		log_debug("di_position: not enough space on the last block\n");
		err = CBM_ERROR_RECORD_NOT_PRESENT;
	}

	f->cht = ss_track;
	f->chs = ss_sector;
	f->chp = rec_start;
	f->next_track = bufp->buf[0];
	f->next_sector = bufp->buf[1];

	// clean up the "expand me" flag
	f->lastpos = 0;

end:
	log_debug("di_position -> err=%d, sector %d/%d/%d\n", err, 
		f->next_track, f->next_sector, rec_start);

	di_FREBUF(&bufp);

	return err;

}

//***********
// di_seek 
//***********

// this is a simple, stupid seek - just following the block chain
// TODO: detect loop / break after max len
// TODO: use di_position on REL files
static int di_seek(file_t * file, long position, int flag)
{

	if (flag != SEEKFLAG_ABS) {
		// unsupported
		return CBM_ERROR_FAULT;
	}

	File *f = (File *) file;
	di_endpoint_t *diep = (di_endpoint_t *) file->endpoint;

	log_debug("di_seek(diep=%p, position=%d (0x%x), flag=%d)\n", diep,
		  position, position, flag);

	f->lastpos = 0;
	if (f->file.recordlen > 0) {
		// store position value for di_position / di_expand_rel
		// because DOS returns RECORD NOT PRESENT, but subsequent
		// writes WILL use this value and expand the file and write there.
		f->lastpos = (position / f->file.recordlen) + 1;
		log_debug("setting lastpos to %d\n", f->lastpos);
	}

	uint8_t next_t = f->Slot.start_track;
	uint8_t next_s = f->Slot.start_sector;

	// each block is 254 data bytes
	do {
		di_fseek_tsp(diep, next_t, next_s, 0);
		f->cht = next_t;
		f->chs = next_s;
		di_fread(&next_t, 1, 1, diep->Ip);
		di_fread(&next_s, 1, 1, diep->Ip);
		if (next_t == 0 || position < 254) {
			// no next block
			break;
		}
		position -= 254;
	}
	while (1);

	f->next_track = next_t;
	f->next_sector = next_s;

	// when position >= 254 we passed to the end of the file
	if (position >= 254) {
		// seek behind end of file - default for POSITION
		return CBM_ERROR_RECORD_NOT_PRESENT;
	}
	if ((f->Slot.type & FS_DIR_ATTR_TYPEMASK) == FS_DIR_TYPE_REL) {
		// when relative file, make sure the full record is within the
		// block, or there is a following block
		if (position + f->file.recordlen >= 254) {
			if (f->next_track == 0) {
				return CBM_ERROR_RECORD_NOT_PRESENT;
			}
		}
	}

	f->chp = position & 0xff;

	return CBM_ERROR_OK;
}

// ------------------------------------------------------------------
// directory / slot handling

// *************
// di_write_slot
// *************

static void di_write_slot(di_endpoint_t * diep, slot_t * slot)
{
	uint8_t p[32];

	log_debug("di_write_slot %d\n", slot->number);
	// di_print_slot(slot);
	memset(p, 0, 32);	// clear slot
	memset(p + 5, 0xa0, 16);	// fill name with $A0
	memcpy(p + 5, slot->filename, strlen((char *)slot->filename));
	p[2] = slot->type;
	p[3] = slot->start_track;
	p[4] = slot->start_sector;
	p[21] = slot->ss_track;
	p[22] = slot->ss_sector;
	p[23] = slot->recordlen;
	p[30] = slot->size & 0xff;
	p[31] = slot->size >> 8;

	log_debug("di_write_slot pos %x\n", slot->pos);
	di_fseek_pos(diep, slot->pos + 2);
	di_fwrite(p + 2, 1, 30, diep->Ip);
	// di_print_block(diep,slot->pos & 0xffffff00);
}

// ************
// di_read_slot
// ************

static void di_read_slot(di_endpoint_t * diep, slot_t * slot)
{
	int i = 0;
	uint8_t p[32];
	di_fseek_pos(diep, slot->pos);
	di_fread(p, 1, 32, diep->Ip);
	memset(slot->filename, 0, 20);
	while (i < 16 && p[i + 5] != 0xa0) {
		slot->filename[i] = p[i + 5];
		++i;
	}
	slot->filename[i] = 0;
	slot->type = p[2];
	slot->start_track = p[3];
	slot->start_sector = p[4];
	slot->size = p[30] + 256 * p[31];
	if ((slot->pos & 0xff) == 0)	// first slot of block
	{
		slot->next_track = p[0];
		slot->next_sector = p[1];
	}
	slot->ss_track = p[21];
	slot->ss_sector = p[22];
	slot->recordlen = p[23];

	log_debug("di_read_slot <(%02x ...) %s>\n", slot->filename[0],
		  slot->filename);
}

// *************
// di_first_slot
// *************

// TODO: check consistency with real images, resp. DirSector in the disk image definitions
static void di_first_slot(di_endpoint_t * diep, slot_t * slot)
{
	log_debug("di_first_slot\n");
	if (diep->DI.ID == 80 || diep->DI.ID == 82) {
		slot->pos = 256 * diep->DI.LBA(39, 1);
		slot->dir_sector = 1;
	} else if (diep->DI.ID == 81) {
		slot->pos = 256 * diep->DI.LBA(40, 3);
		slot->dir_sector = 3;
	} else {
		slot->pos =
		    256 * diep->DI.LBA(diep->BAM[0][0], diep->BAM[0][1]);
		slot->dir_sector = diep->BAM[0][1];
	}
	slot->number = 0;
	slot->eod = 0;
}

// ************
// di_next_slot
// ************

static int di_next_slot(di_endpoint_t * diep, slot_t * slot)
{
	if ((++slot->number & 7) == 0)	// read next dir block
	{
		log_debug("Next Slot (%d/%d)\n", slot->next_track,
			  slot->next_sector);
		if (slot->next_track == 0) {
			slot->eod = 1;
			return 0;	// end of directory
		}
		slot->pos =
		    256 * diep->DI.LBA(slot->next_track, slot->next_sector);
		slot->dir_sector = slot->next_sector;
	} else
		slot->pos += 32;
	return 1;
}

// *************
// di_match_slot
// *************
// @deprecated
static int
di_match_slot(di_endpoint_t * diep, slot_t * slot, const uint8_t * name,
	      uint8_t type)
{
	do {
		di_read_slot(diep, slot);
		if (slot->type && ((type == FS_DIR_TYPE_UNKNOWN)
				   || ((slot->type & FS_DIR_ATTR_TYPEMASK) ==
				       type))
		    && compare_pattern((char *)slot->filename, (char *)name,
				       advanced_wildcards)) {
			return 1;	// found
		}
	}
	while (di_next_slot(diep, slot));
	return 0;		// not found
}

// **************
// di_clear_dir_block
// **************

static void di_clear_dir_block(di_endpoint_t * diep, int pos)
{
	uint8_t p[256];

	memset(p, 0, 256);
	p[1] = 0xff;
	di_fseek_pos(diep, pos);
	di_fwrite(p, 1, 256, diep->Ip);
}

// *******************
// di_update_dir_chain
// *******************

static void
di_update_dir_chain(di_endpoint_t * diep, slot_t * slot, uint8_t sector)
{
	di_fseek_pos(diep, slot->pos & 0xffff00);
	di_fwrite(&diep->DI.DirTrack, 1, 1, diep->Ip);
	di_fwrite(&sector, 1, 1, diep->Ip);
}

// *************************
// di_allocate_new_dir_block
// *************************

static int di_allocate_new_dir_block(di_endpoint_t * diep, slot_t * slot)
{
	uint8_t track = diep->DI.DirTrack;
	uint8_t sector = slot->dir_sector;

	int err = di_find_free_block_NXTTS(diep, &track, &sector);
	if (err != CBM_ERROR_OK) {
		return err;	// directory full;
	}

	di_sync_BAM(diep);
	di_update_dir_chain(diep, slot, sector);
	slot->pos = 256 * diep->DI.LBA(diep->DI.DirTrack, sector);
	slot->eod = 0;
	di_clear_dir_block(diep, slot->pos);
	slot->next_track = 0;
	slot->next_sector = 0;
	log_debug("di_allocate_new_dir_block diep=%p, (%d/%d)\n", diep,
		  diep->DI.DirTrack, sector);
	return CBM_ERROR_OK;	// OK
}

// *****************
// di_find_free_slot
// *****************

static int di_find_free_slot(di_endpoint_t * diep, slot_t * slot)
{
	di_first_slot(diep, slot);
	do {
		di_read_slot(diep, slot);
		if (slot->type == 0)
			return 0;	// found
	}
	while (di_next_slot(diep, slot));
	return di_allocate_new_dir_block(diep, slot);
}

// ***************
// di_create_entry 
// ***************

static int
di_create_entry(di_endpoint_t * diep, File * file, const char *name,
		openpars_t * pars)
{
	log_debug("di_create_entry(%s)\n", name);

	if (!file)
		return CBM_ERROR_FAULT;
	if (di_find_free_slot(diep, &file->Slot))
		return CBM_ERROR_DISK_FULL;
	strncpy((char *)file->Slot.filename, name, 16);
	file->file.type =
	    ((pars->filetype ==
	      FS_DIR_TYPE_UNKNOWN) ? FS_DIR_TYPE_PRG : pars->filetype);
	file->Slot.type = 0x80 | file->file.type;
	file->chp = 0;
	file->Slot.ss_track = 0;	// invalid, i.e. new empty file if REL
	file->Slot.ss_sector = 0;
	file->Slot.recordlen = pars->recordlen;
	if (pars->filetype != FS_DIR_TYPE_REL) {
		int err =
		    di_find_free_block_INTTS(diep, &(file->Slot.start_track),
					     &(file->Slot.start_sector));
		if (err != CBM_ERROR_OK) {
			return err;
		}
		file->Slot.size = 1;
	} else {
		file->Slot.size = 0;
		file->Slot.start_track = 0;
		file->Slot.start_sector = 0;

		// store number of actual records in file; will store 0 on new file
		file->maxrecord = 0;
		file->file.recordlen = pars->recordlen;

		// expand file to at least one record (which extends to the first block)
		di_expand_rel(diep, file, 1);

		log_debug("Setting maxrecord to %d\n", file->maxrecord);

	}
	di_write_slot(diep, &file->Slot);

	di_fflush((file_t *) file);
	// di_print_slot(&file->Slot);
	return CBM_ERROR_OK;
}

// ------------------------------------------------------------------
// OPEN / READ a directory

// ************
// di_open_dir
// ************
// open a directory read
static int di_open_dir(File * file)
{

	int er = CBM_ERROR_FILE_TYPE_MISMATCH;

	log_debug("ENTER: di_open_dr(%p (%s))\n", file,
		  (file == NULL) ? "<nil>" : file->file.filename);

	di_endpoint_t *diep = (di_endpoint_t *) file->file.endpoint;

	// crude root check
	if (strcmp(file->file.filename, "$") == 0) {
		file->file.dirstate = DIRSTATE_FIRST;

		di_first_slot(diep, &diep->Slot);

		log_exitr(CBM_ERROR_OK);
		return CBM_ERROR_OK;
	} else {
		log_error("Error opening directory\n");
		log_exitr(er);
		return er;
	}
}

char *extension[6] = { "DEL", "SEQ", "PRG", "USR", "REL", "CBM" };

// *******************
// di_directory_header
// *******************

static int di_directory_header(char *dest, di_endpoint_t * diep)
{
	memset(dest + FS_DIR_LEN, 0, 4);	// length
	memset(dest + FS_DIR_YEAR, 0, 6);	// date+time
	dest[FS_DIR_MODE] = FS_DIR_MOD_NAM;

	di_fseek_tsp(diep, diep->DI.DirTrack, diep->DI.HdrSector,
		     diep->DI.HdrOffset);
	di_fread(dest + FS_DIR_NAME, 1, 16, diep->Ip);
	di_fseek_tsp(diep, diep->DI.DirTrack, diep->DI.HdrSector,
		     diep->DI.HdrOffset + 18);
	di_fread(dest + FS_DIR_NAME + 16, 1, 5, diep->Ip);

	// fix up $a0 into $20 characters
	for (int i = FS_DIR_NAME; i < FS_DIR_NAME + 22; i++) {
		if (dest[i] == 0xa0) {
			dest[i] = 0x20;
		}
	}
	dest[FS_DIR_NAME + 22] = 0;
	log_debug("di_directory_header (%s)\n", dest + FS_DIR_NAME);
	return FS_DIR_NAME + 23;
}

// **************
// di_blocks_free
// **************

static int di_blocks_free(char *dest, di_endpoint_t * diep)
{
	int FreeBlocks;
	int BAM_Number;		// BAM block for current track
	int BAM_Increment;
	int Track;
	int i;
	uint8_t *fbl;		// pointer to track free blocks
	Disk_Image_t *di = &diep->DI;

	FreeBlocks = 0;
	BAM_Number = 0;
	BAM_Increment = 1 + ((di->Sectors + 7) >> 3);
	Track = 1;

	while (BAM_Number < 4 && diep->BAM[BAM_Number]) {
		fbl = diep->BAM[BAM_Number] + di->BAMOffset;
		if (di->ID == 71 && Track > di->Tracks) {
			fbl = diep->BAM[0] + 221;
			BAM_Increment = 1;
		}
		for (i = 0;
		     i < di->TracksPerBAM && Track <= di->Tracks * di->Sides;
		     ++i) {
			if (Track != di->DirTrack)
				FreeBlocks += *fbl;
			fbl += BAM_Increment;
			++Track;
		}
		++BAM_Number;
	}

	log_debug("di_blocks_free: %u\n", FreeBlocks);
	FreeBlocks <<= 8;

	dest[FS_DIR_ATTR] = FS_DIR_ATTR_ESTIMATE;
	dest[FS_DIR_LEN + 0] = FreeBlocks;
	dest[FS_DIR_LEN + 1] = FreeBlocks >> 8;
	dest[FS_DIR_LEN + 2] = FreeBlocks >> 16;
	dest[FS_DIR_LEN + 3] = 0;
	dest[FS_DIR_MODE] = FS_DIR_MOD_FRE;
	dest[FS_DIR_NAME] = 0;
	return FS_DIR_NAME + 1;
}

/*******************
 * get the next directory entry in the directory given as fp.
 * If isresolve is set, then the disk header and blocks free entries are skipped
 * 
 * outpattern points into a newly malloc'd string
 */
static int
di_direntry(file_t * fp, file_t ** outentry, int isresolve, int *readflag,
	    const char **outpattern)
{

	(void)isresolve;	// silence warning unused parameter

	// here we (currently) only use it in resolve, not in read_dir_entry,
	// so we don't care about isresolve and first/last entry

	log_debug("di_direntry(fp=%p)\n", fp);

	cbm_errno_t rv = CBM_ERROR_FAULT;

	if (!fp) {
		return rv;
	}

	rv = CBM_ERROR_OK;
	*outentry = NULL;

	di_endpoint_t *diep = (di_endpoint_t *) fp->endpoint;
	File *file = (File *) fp;

	if (file->dospattern == NULL) {
		char *pattern = NULL;
		if (fp->pattern == NULL) {
			pattern = mem_alloc_str("*");
		} else {
			pattern = mem_alloc_str(fp->pattern);
		}
		// pattern will be in wireformat and our reference, 
		// so we never convert the pattern, only the file name
		//provider_convto(diep->base.ptype)(pattern, strlen(pattern), pattern, strlen(pattern));
		file->dospattern = pattern;
	}

	*readflag = READFLAG_DENTRY;
	file_t *wrapfile = NULL;

	do {

		if (diep->Slot.eod) {
			*outentry = NULL;
			//fp->dirstate = DIRSTATE_END;
			// end of search
			break;
		}

		di_read_slot(diep, &diep->Slot);

		if (diep->Slot.type != 0) {
			File *entry = di_reserve_file(diep);
			entry->file.endpoint = (endpoint_t *) diep;

			// it is totally stupid having to do this...
			memcpy(&entry->Slot, &diep->Slot, sizeof(slot_t));

			entry->file.parent = fp;
			entry->file.mode = FS_DIR_MOD_FIL;
			entry->file.filesize = entry->Slot.size * 254;
			entry->file.type =
			    entry->Slot.type & FS_DIR_ATTR_TYPEMASK;
			entry->file.seekable =
			    (entry->file.type == FS_DIR_TYPE_REL);
			entry->file.attr =
			    (entry->Slot.type & (~FS_DIR_ATTR_TYPEMASK)) ^
			    FS_DIR_ATTR_SPLAT;
			// convert to external charset
			entry->file.filename =
			    conv_from_alloc((const char *)entry->Slot.filename,
					    &di_provider);

			log_debug
			    ("converted image filename %s to %s for next check\n",
			     (const char *)entry->Slot.filename,
			     entry->file.filename);

			if (!diep->Ip->writable) {
				entry->file.attr |= FS_DIR_ATTR_LOCKED;
			} else {
				entry->file.writable = 1;
			}

			if (handler_next
			    ((file_t *) entry, file->dospattern,
			     outpattern, &wrapfile) == CBM_ERROR_OK) {
				*outentry = wrapfile;
				rv = CBM_ERROR_OK;

				// .. and this state is the reason for the stupid copy above...
				di_next_slot(diep, &diep->Slot);
				break;
			}
			// cleanup to read next entry
			entry->file.handler->close((file_t *) entry, 0);
		}
		// .. and this state is the reason for the stupid copy above...
		di_next_slot(diep, &diep->Slot);
	}
	while (1);

	return rv;
}

// *****************
// di_read_dir_entry
// *****************

static int
di_read_dir_entry(di_endpoint_t * diep, File * file, char *retbuf, int len,
		  int *eof)
{
	int rv = 0;
	const char *outpattern;

	log_debug("di_read_dir_entry(%p, dospattern=(%02x ...) %s)\n", file,
		  file->dospattern[0], file->dospattern);

	if (!file)
		return -CBM_ERROR_FAULT;

	*eof = READFLAG_DENTRY;

	if (file->file.dirstate == DIRSTATE_FIRST) {
		file->file.dirstate++;
		rv = di_directory_header(retbuf, diep);
		di_first_slot(diep, &diep->Slot);
		return rv;
	}

	if (!diep->Slot.eod) {
		File *direntry = NULL;
		int readflg = 0;

		rv = -di_direntry((file_t *) file, (file_t **) & direntry, 0,
				  &readflg, &outpattern);

		if (rv == CBM_ERROR_OK && direntry != NULL) {
			rv = dir_fill_entry_from_file(retbuf,
						      (file_t *) direntry, len);
			direntry->file.handler->close((file_t *) direntry, 0);
			return rv;
		}
	}

	if (file->file.dirstate != DIRSTATE_END) {
		file->file.dirstate = DIRSTATE_END;
		rv = di_blocks_free(retbuf, diep);
	}
	*eof |= READFLAG_EOF;
	return rv;
}

// ------------------------------------------------------------------
// OPEN / READ / WRITE a file

// ************
// di_open_file
// ************

static int di_open_file(File * file, openpars_t * pars, int di_cmd)
{
	int np, rv;

	const char *filename = file->file.filename;

	log_info("OpenFile(%s,%c,%d)\n", filename, pars->filetype + 0x30,
		 pars->recordlen);

	if (pars->recordlen > 254) {
		return CBM_ERROR_OVERFLOW_IN_RECORD;
	}

	di_endpoint_t *diep = (di_endpoint_t *) (file->file.endpoint);

	int file_required = false;
	int file_must_not_exist = false;

	file->access_mode = 0;

	switch (di_cmd) {
	case FS_OPEN_AP:
	case FS_OPEN_RD:
		file_required = true;
		break;
	case FS_OPEN_WR:
		file_must_not_exist = true;
		break;
	case FS_OPEN_OW:
		break;
	case FS_OPEN_RW:
		// we defer the test until after we find the file and check its file type
		//if (pars->filetype != FS_DIR_TYPE_REL) {
		//     log_error("Read/Write currently only supported for REL files on disk images\n");
		//     return CBM_ERROR_FAULT;
		//}
		break;
	default:
		log_error("Internal error: OpenFile with di_cmd %d\n", di_cmd);
		return CBM_ERROR_FAULT;
	}
	if (*filename == '$' && di_cmd == FS_OPEN_RD) {
		// reading the directory as normal file just returns the standard
		// blocks 18/0 -> 18/1 -> and following the block chain
		Disk_Image_t *di = &diep->DI;
		file->next_track = di->DirTrack;
		file->next_sector = 0;
		np = 1;
	} else {
		di_first_slot(diep, &file->Slot);
		np = di_match_slot(diep, &file->Slot, (const uint8_t *)filename,
				   pars->filetype);
		file->next_track = file->Slot.start_track;
		file->next_sector = file->Slot.start_sector;
		if ((pars->filetype == FS_DIR_TYPE_REL)
		    || ((file->Slot.type & FS_DIR_ATTR_TYPEMASK) ==
			FS_DIR_TYPE_REL)) {
			// open a relative file

			pars->filetype = FS_DIR_TYPE_REL;
			// check record length
			if (!np) {
				// does not exist yet
				if (pars->recordlen == 0) {
					return CBM_ERROR_RECORD_NOT_PRESENT;
				}
			} else {
				if (pars->recordlen == 0) {
					// no reclen is given in the open
					pars->recordlen = file->Slot.recordlen;
				} else {
					// there is a rec len in the open and in the file
					// so they need to be the same
					if (pars->recordlen !=
					    file->Slot.recordlen) {
						return
						    CBM_ERROR_RECORD_NOT_PRESENT;
					}
				}
			}
			file->file.recordlen = pars->recordlen;
			// store number of actual records in file; will store 0 on new file
			file->maxrecord = di_rel_record_max(diep, file);

		} else {
			// not a rel file
			if (di_cmd == FS_OPEN_RW) {
				log_error
				    ("Read/Write currently only supported for REL files on disk images\n");
				return CBM_ERROR_FAULT;
			}
		}
	}
	file->chp = 255;
	log_debug("File starts at (%d/%d)\n", file->next_track,
		  file->next_sector);
	if (file_required && np == 0) {
		log_error("Unable to open '%s': file not found\n", filename);
		return CBM_ERROR_FILE_NOT_FOUND;
	}
	if (file_must_not_exist && np > 0) {
		log_error("Unable to open '%s': file exists\n", filename);
		return CBM_ERROR_FILE_EXISTS;
	}
	if (!np) {
		if (pars->filetype == FS_DIR_TYPE_UNKNOWN) {
			pars->filetype = FS_DIR_TYPE_PRG;
		}
		rv = di_create_entry(diep, file, filename, pars);
		if (rv != CBM_ERROR_OK)
			return rv;
	}

	if (di_cmd == FS_OPEN_AP) {
		di_pos_append(diep, file);
	}
	// flag for successful open
	file->access_mode = di_cmd;

	return (pars->filetype ==
		FS_DIR_TYPE_REL) ? CBM_ERROR_OPEN_REL : CBM_ERROR_OK;
}

// ************
// di_read_byte
// ************

static int di_read_byte(di_endpoint_t * diep, File * f, char *retbuf)
{
	if (di_update_chx(diep, f, 0)) {
		return READFLAG_EOF;
	}
	di_fread(retbuf, 1, 1, diep->Ip);
	++f->chp;
	// log_debug("di_read_byte %2.2x\n",(uint8_t)*retbuf);
	if (f->next_track == 0 && f->chp + 1 >= f->next_sector) {
		return READFLAG_EOF;
	}
	return 0;
}

// *************
// di_write_byte
// *************

static int di_write_byte(di_endpoint_t * diep, File * f, uint8_t ch)
{
	int oldpos;
	int err;
	uint8_t t = 0, s = 0;
	uint8_t zero = 0;
	// log_debug("di_write_byte %2.2x\n",ch);
	if (f->chp > 253) {
		// note: when the file is wrapped into a new endpoint, 
		// then access mode is not set. Esp. when *f is a d80 file
		// containing a d64 file that is updated with this method.
		// example is test case fwtests/base/cmdchan1
		if (true /*f->access_mode == FS_OPEN_RW */ ) {
			// to make sure we're not in the middle of a file
			// check the link track number
			di_fseek_tsp(diep, f->cht, f->chs, 0);
			di_fread(&t, 1, 1, diep->Ip);
			di_fread(&s, 1, 1, diep->Ip);
		}
		if (t == 0) {
			// only update link chain if we're not in the middle of a file
			f->chp = 0;
			oldpos = 256 * diep->DI.LBA(f->cht, f->chs);
			err = di_find_free_block_NXTTS(diep, &f->cht, &f->chs);
			if (err != CBM_ERROR_OK) {
				return err;
			}
			di_fseek_pos(diep, oldpos);
			di_fwrite(&f->cht, 1, 1, diep->Ip);
			di_fwrite(&f->chs, 1, 1, diep->Ip);
			di_fseek_tsp(diep, f->cht, f->chs, 0);
			di_fwrite(&zero, 1, 1, diep->Ip);	// new link track
			di_fwrite(&zero, 1, 1, diep->Ip);	// new link sector
			++f->Slot.size;	// increment filesize
			// log_debug("next block: (%d/%d)\n",f->cht,f->chs);
		} else {
			// position at next (existing) block
			f->chp = 0;
			f->cht = t;
			f->chs = s;
			di_fseek_tsp(diep, t, s, 2);
		}
	}
	di_fwrite(&ch, 1, 1, diep->Ip);
	++f->chp;
	return CBM_ERROR_OK;
}

// ***********
// di_read_seq
// ***********

static int
di_read_seq(di_endpoint_t * diep, File * file, char *retbuf, int len, int *eof)
{
	int i;
	log_debug("di_read_seq(fp %d, len=%d)\n", file, len);

	// we need to seek before the actual read, to make sure a parallel access does not
	// disturb the position. Only at the first byte of the file, cht/chs is invalid...
	if (di_update_chx(diep, file, 1)) {
		*eof = READFLAG_EOF;
		return 0;
	}

	for (i = 0; i < len; ++i) {
		*eof = di_read_byte(diep, file, retbuf + i);
		if (*eof)
			return i + 1;
	}
	return len;
}

// ************
// di_writefile
// ************

static int di_writefile(file_t * fp, const char *buf, int len, int is_eof)
{
	int i;
	int err;
	di_endpoint_t *diep = (di_endpoint_t *) fp->endpoint;
	File *file = (File *) fp;

	if (file->access_mode == FS_BLOCK && diep->U2_track)	// fill block for U2 command
	{
		di_write_block(diep, buf, len);
		if (is_eof)
			di_save_buffer(diep);
		return CBM_ERROR_OK;
	}

	log_debug
	    ("di_writefile: diep=%p, write to file %p, lastpos=%d, len=%d\n",
	     diep, file, file->lastpos, len);

	if (file->lastpos > 0) {
		err = di_expand_rel(diep, file, file->lastpos);
		if (err != CBM_ERROR_OK) {
			return -err;
		}
		di_position(diep, file, file->lastpos - 1);
	}
	di_fseek_tsp(diep, file->cht, file->chs, 2 + file->chp);
	for (i = 0; i < len; ++i) {
		if (di_write_byte(diep, file, (uint8_t) buf[i])) {
			return -CBM_ERROR_DISK_FULL;
		}
	}

	return CBM_ERROR_OK;
}

// ------------------------------------------------------------------
// commands

static int di_format(endpoint_t * ep, const char *name)
{
	di_endpoint_t *diep = (di_endpoint_t *) ep;
	Disk_Image_t *di = &diep->DI;

	uint8_t idbuffer[5];
	uint8_t *buf;

	const char *p = index(name, ',');
	int len = strlen(name);

	// the buffer we are going to use for format
	buf_t *bp = NULL;

	// read original disk ID and save it
	di_GETBUF(&bp, diep);
	di_MAPBUF(bp, diep->DI.DirTrack, diep->DI.HdrSector);

	buf = bp->buf;

	memcpy(idbuffer, buf + diep->DI.HdrOffset + 18, 5);

	// -------------------------------------------------------------------
	// clear disk when formatted with ID

	// SETBUF prepares for writing, so need to do that before memset()
	di_SETBUF(bp, 1, 0);
	memset(buf, 0, 256);

	if (p != NULL) {
		len = p - name;
		p++;
		if (*p) {

			// we have an ID part, so we have to fully clear the disk image

			uint8_t track = 1;
			uint8_t sector = 0;

			while (track < di->Tracks) {

				di_SETBUF(bp, track, sector);

				uint8_t maxsect = di->LSEC(track);

				while (sector < maxsect) {

					di_WRBUF(bp);
					sector++;
				}
				sector = 0;
				track++;
			}
		}
	}
	// -------------------------------------------------------------------
	// Now setup the new disk header, empty directory and BAM

	di_SETBUF(bp, diep->DI.DirTrack, diep->DI.HdrSector);

	// disk name block
	if (di->ID == 81) {
		buf[0] = di->DirTrack;
		buf[1] = di->DirSector;
	} else {
		buf[0] = di->bamts[0];
		buf[1] = di->bamts[1];
	}
	// disk header
	if (di->ID == 81) {
		memset(buf + di->HdrOffset, 0xa0, 25);
	} else {
		memset(buf + di->HdrOffset, 0xa0, 27);
	}
	if (len > 16)
		len = 16;
	if (len == 0) {
		if (p) {
			buf[di->HdrOffset] = ',';
			buf[di->HdrOffset + 27] = 0xa0;
		} else {
			buf[di->HdrOffset] = 0x0d;
		}
	} else {
		strncpy((char *)buf + di->HdrOffset, name, len);
	}
	// restore original ID
	memcpy(buf + di->HdrOffset + 18, idbuffer, 2);
	// did we format with ID?
	if (p && *p) {
		len = strlen(p);
		if (len > 2)
			len = 2;
		memcpy(buf + di->HdrOffset + 18, p, len);
		if (len == 1) {
			buf[di->HdrOffset + 19] = 0x0d;
		}
	}
	// DOS version
	buf[2] = di->dosver[1];
	memcpy(buf + di->HdrOffset + 21, di->dosver, 2);

	if (di->DirTrack != di->bamts[0] || di->HdrSector != di->bamts[1]) {
		// header is NOT in the first BAM block (as in the D64 files), so
		// we need to save this block now

		di_WRBUF(bp);
		memset(buf, 0, 256);
	}
	// -------------------------------------------------------------------
	// prepare BAM

	// note: first block is not cleared, as in D64 this already contains the disk header
	uint8_t BAM_Number;	// BAM block for current track
	uint8_t BAM_Increment;
	uint8_t BAM_Offset;
	uint8_t track;
	uint8_t first_track;	// of current BAM block
	uint8_t maxsec;
	uint8_t idx;

	BAM_Number = 0;
	BAM_Offset = di->BAMOffset;
	BAM_Increment = 1 + ((di->Sectors + 7) >> 3);
	track = 1;

	while (BAM_Number < di->BAMBlocks) {
		first_track = track;

		// prepare buffer to write
		di_SETBUF(bp, diep->DI.bamts[BAM_Number * 2],
			  diep->DI.bamts[BAM_Number * 2 + 1]);

		// BAM link address
		if (BAM_Number == di->BAMBlocks - 1 || di->ID == 71) {
			if (di->ID == 81) {
				buf[0] = 0;
				buf[1] = 0xff;
			} else {
				buf[0] = di->DirTrack;
				buf[1] = di->DirSector;
			}
		} else {
			printf("bamts = %02x %02x %02x %02x, Number=%d\n",
			       di->bamts[0], di->bamts[1], di->bamts[2],
			       di->bamts[3], BAM_Number);
			buf[0] = di->bamts[BAM_Number * 2 + 2];
			buf[1] = di->bamts[BAM_Number * 2 + 3];
		}
		// BAM version
		buf[2] = di->dosver[1];

		if (di->ID == 71 && BAM_Number == 1) {
			BAM_Increment = 3;
			BAM_Offset = 0;
		}
		// Track list with free block count and BAM per track
		for (uint8_t cnt = 0;
		     cnt < di->TracksPerBAM && track <= di->Tracks * di->Sides;
		     cnt++, track++) {
			idx = cnt * BAM_Increment + BAM_Offset;
			// prepare BAM itself
			maxsec = di->LSEC(track);
			if (di->ID != 71 || BAM_Number != 1) {
				buf[idx++] = maxsec;	// free blocks number for track
			}
			if (di->ID != 71 || BAM_Number != 1 || track != 53) {
				while (maxsec > 7) {
					buf[idx++] = 0xff;
					maxsec -= 8;
				}
				if (maxsec > 0) {
					buf[idx] = 0;
					while (maxsec > 0) {
						buf[idx] = 1 + (buf[idx] << 1);
						maxsec--;
					}
				}
			} else {
				// D71 BAM track on second side
				maxsec = (maxsec + 1) >> 8;
				while (maxsec > 0) {
					buf[idx++] = 0;
				}
			}
		}
		if (di->ID == 71 && BAM_Number == 0) {
			// free block list for second half of BAM in 1571
			for (uint8_t cnt = 0;
			     cnt < di->TracksPerBAM
			     && track <= di->Tracks * di->Sides;
			     cnt++, track++) {
				idx = 221 + cnt;
				if (track != 53) {
					maxsec = di->LSEC(track);
					buf[idx] = maxsec;
				} else {
					buf[idx] = 0;
				}
			}
			// reset track counter
			track = di->Tracks + 1;
		}
		// now mask out DIR and BAM blocks
		if (di->DirTrack >= first_track && di->DirTrack < track) {
			idx =
			    (di->DirTrack - 1) * BAM_Increment + di->BAMOffset;
			buf[idx]--;	// free blocks
			buf[idx + 1 + (di->DirSector >> 3)] &=
			    (0xff - (1 << (di->DirSector & 7)));
			// don't free the same block twice; Dir header may be mixed into first BAM (D64)
			if (di->DirTrack != di->bamts[0]
			    || di->HdrSector != di->bamts[1]) {
				buf[idx]--;	// free blocks
				buf[idx + 1 + (di->HdrSector >> 3)] &=
				    (0xff - (1 << (di->HdrSector & 7)));
			}
		}
		if (di->ID != 71 || BAM_Number != 1) {
			for (uint8_t cnt = 0; cnt < di->BAMBlocks; cnt++) {
				if (di->bamts[cnt * 2] >= first_track
				    && di->bamts[cnt * 2] < track) {
					idx =
					    (di->bamts[cnt * 2] -
					     1) * BAM_Increment + di->BAMOffset;
					buf[idx]--;	// free blocks
					buf[idx + 1 +
					    (di->bamts[cnt * 2 + 1] >> 3)] &=
					    (0xff -
					     (1 <<
					      (di->bamts[cnt * 2 + 1] & 7)));
				}
			}
		}
		// start and end track for BAM
		if (di->BAMOffset >= 6) {
			if (di->ID == 81) {
				buf[3] = ~buf[2];	// one's complement of version number
				buf[4] = idbuffer[0];
				buf[5] = idbuffer[1];
				buf[6] = 0xc0;	// "I/O byte"
				//  bit 7: verify on if set
				//  bit 6: check header CRC if set
				buf[7] = 0x00;	// "Autoboot flag"
			} else {
				buf[4] = first_track;
				buf[5] = track;
			}
		}
		// write out BAM block
		di_WRBUF(bp);

		// clear buffer
		memset(buf, 0, 256);

		++BAM_Number;
	}

	// -------------------------------------------------------------------
	// prepare first directory block

	di_SETBUF(bp, diep->DI.DirTrack, diep->DI.DirSector);
	memset(buf, 0, 256);
	buf[1] = 0xff;
	di_WRBUF(bp);

	di_FREBUF(&bp);

	// re-read BAM
	di_read_BAM(diep);

	return CBM_ERROR_OK;
}

// **************
// di_delete_file
// **************

static void di_delete_file(di_endpoint_t * diep, slot_t * slot)
{
	uint8_t t, s;
	log_debug("di_delete_file #%d <%s>\n", slot->number, slot->filename);

	slot->type = 0;		// mark as deleted
	di_write_slot(diep, slot);
	t = slot->start_track;
	s = slot->start_sector;
	while (t)		// follow chain for freeing blocks
	{
		di_block_free(diep, t, s);
		di_fseek_tsp(diep, t, s, 0);
		di_fread(&t, 1, 1, diep->Ip);
		di_fread(&s, 1, 1, diep->Ip);
	}
	// di_print_block(diep,slot->pos & 0xffff00);
}

// **********
// di_scratch
// **********

static int di_scratch(file_t * file)
{
	File *fp = (File *) file;
	di_endpoint_t *diep = (di_endpoint_t *) file->endpoint;

	di_delete_file(diep, &fp->Slot);

	return CBM_ERROR_OK;
}

// *********
// di_move
// *********

static int di_move(file_t * fromfile, file_t * todir, const char *toname)
{

	(void)todir;		// silence

	di_endpoint_t *diep = (di_endpoint_t *) fromfile->endpoint;

	slot_t *slot;
	slot_t newslot;

	log_debug("di_rename (%s) to (%s)\n", fromfile->filename, toname);

	const char *nameto = conv_to_alloc(toname, &di_provider);

	// check if target exists
	di_first_slot(diep, &newslot);
	if (di_match_slot
	    (diep, &newslot, (uint8_t *) nameto, FS_DIR_TYPE_UNKNOWN)) {
		mem_free(nameto);
		return CBM_ERROR_FILE_EXISTS;
	}
	// fromfile is known and we have the slot already
	File *fp = (File *) fromfile;
	slot = &fp->Slot;

	int n = strlen(nameto);
	if (n > 16) {
		n = 16;
	}
	memset(slot->filename, 0xA0, 16);	// fill filename with $A0
	memcpy(slot->filename, nameto, n);
	di_write_slot(diep, slot);

	mem_free(nameto);
	return CBM_ERROR_OK;
}

// TODO: detect loop / break after max len
static size_t di_realsize(file_t * file)
{

	File *f = (File *) file;
	di_endpoint_t *diep = (di_endpoint_t *) file->endpoint;

	size_t len = 0;

	uint8_t next_t = f->Slot.start_track;
	uint8_t next_s = f->Slot.start_sector;

	// each block is 254 data bytes
	do {
		di_fseek_tsp(diep, next_t, next_s, 0);
		f->cht = next_t;
		f->chs = next_s;
		di_fread(&next_t, 1, 1, diep->Ip);
		di_fread(&next_s, 1, 1, diep->Ip);
		if (next_t == 0) {
			// no next block
			break;
		}
		len += 254;
	}
	while (1);

	f->next_track = next_t;
	f->next_sector = next_s;

	len += next_s - 1;

	log_debug("Found length of %d for file %s\n", len, file->filename);

	return len;

}

//***********
// di_create 
//***********

static int
di_create(file_t * dirp, file_t ** newfile, const char *pattern,
	  openpars_t * pars, int type)
{
	di_endpoint_t *diep = (di_endpoint_t *) dirp->endpoint;

	// validate name for Dxx correctness
	// note that path separators are allowed, as the CBM drives happily created
	// files with '/' in them.
	if (strchr(pattern, ':') != NULL) {
		return CBM_ERROR_SYNTAX_PATTERN;
	}
	if (strchr(pattern, '*') != NULL) {
		return CBM_ERROR_SYNTAX_PATTERN;
	}
	if (strchr(pattern, '?') != NULL) {
		return CBM_ERROR_SYNTAX_PATTERN;
	}
	if (strchr(pattern, ',') != NULL) {
		return CBM_ERROR_SYNTAX_PATTERN;
	}

	if (type != FS_OPEN_RD) {
		if (dirp->writable == 0 || diep->Ip->writable == 0) {
			return CBM_ERROR_WRITE_PROTECT;
		}
	}

	File *file = di_reserve_file(diep);
	file->access_mode = type;
	file->file.writable = (type == FS_OPEN_RD) ? 0 : 1;
	file->file.seekable = 1;

	int rv = di_create_entry(diep, file, pattern, pars);

	di_fflush(&file->file);

	if (rv != CBM_ERROR_OK) {

		di_dump_file((file_t *) file, 1, 0);

		reg_remove(&diep->base.files, file);
		mem_free(file);
	} else {

		*newfile = (file_t *) file;
	}

	return rv;
}

//***********
// di_open
//***********

static int di_open(file_t * fp, openpars_t * pars, int type)
{
	int rv = CBM_ERROR_FAULT;

	File *file = (File *) fp;

	if (type == FS_OPEN_DR) {
		rv = di_open_dir(file);
	} else {
		rv = di_open_file(file, pars, type);
	}
	return rv;
}

// ***********
// di_readfile
// ***********

static int di_readfile(file_t * fp, char *retbuf, int len, int *eof)
{
	int rv = 0;
	di_endpoint_t *diep = (di_endpoint_t *) fp->endpoint;
	File *file = (File *) fp;
	log_debug("di_readfile(%p fp=%p len=%d\n", diep, file, len);

	if (fp->dirstate != DIRSTATE_NONE) {
		// directory
		rv = di_read_dir_entry(diep, file, retbuf, len, eof);
	} else {
		if (file->access_mode == FS_BLOCK) {
			return di_read_block(diep, file, retbuf, len, eof);
		} else {
			rv = di_read_seq(diep, file, retbuf, len, eof);
		}
	}
	return rv;
}

// *******
// di_init
// *******

static void di_init(void)
{
	log_debug("di_init\n");

	provider_register(&di_provider);

	reg_init(&di_endpoint_registry, "di_endpoint_registry", 5);
}

// ----------------------------------------------------------------------------------
//    Debug code

static void di_dump_ep(di_endpoint_t * fsep, int indent)
{

	const char *prefix = dump_indent(indent);
	int newind = indent + 1;
	const char *eppref = dump_indent(newind);

	log_debug("%sprovider='%s';\n", prefix, fsep->base.ptype->name);
	log_debug("%sis_temporary='%d';\n", prefix, fsep->base.is_temporary);
	log_debug("%sis_assigned='%d';\n", prefix, fsep->base.is_assigned);
	log_debug("%sroot_file=%p; // '%s'\n", prefix, fsep->Ip,
		  fsep->Ip->filename);
	log_debug("%sfiles={;\n", prefix);
	for (int i = 0;; i++) {
		File *file = (File *) reg_get(&fsep->base.files, i);
		log_debug("%s// file at %p\n", eppref, file);
		if (file != NULL) {
			log_debug("%s{\n", eppref, file);
			if (file->file.handler->dump != NULL) {
				file->file.handler->dump((file_t *) file, 0,
							 newind + 1);
			}
			log_debug("%s{\n", eppref, file);
		} else {
			break;
		}
	}
	log_debug("%s}\n", prefix);
}

static void di_dump(int indent)
{

	const char *prefix = dump_indent(indent);
	int newind = indent + 1;
	const char *eppref = dump_indent(newind);

	log_debug("%s// disk image provider\n", prefix);
	log_debug("%sendpoints={\n", prefix);
	for (int i = 0;; i++) {
		di_endpoint_t *fsep =
		    (di_endpoint_t *) reg_get(&di_endpoint_registry, i);
		if (fsep != NULL) {
			log_debug("%s// endpoint %p\n", eppref, fsep);
			log_debug("%s{\n", eppref);
			di_dump_ep(fsep, newind + 1);
			log_debug("%s}\n", eppref);
		} else {
			break;
		}
	}
	log_debug("%s}\n", prefix);

}

static void di_dump_file(file_t * fp, int recurse, int indent)
{

	const char *prefix = dump_indent(indent);

	File *file = (File *) fp;

	log_debug("%shandler='%s';\n", prefix, file->file.handler->name);
	log_debug("%sparent='%p';\n", prefix, file->file.parent);
	if (recurse) {
		log_debug("%s{\n", prefix);
		if (file->file.parent != NULL
		    && file->file.parent->handler->dump != NULL) {
			file->file.parent->handler->dump(file->file.parent, 1,
							 indent + 1);
		}
		log_debug("%s}\n", prefix);

	}
	log_debug("%sisdir='%d';\n", prefix, file->file.isdir);
	log_debug("%sdirstate='%d';\n", prefix, file->file.dirstate);
	log_debug("%spattern='%s';\n", prefix, file->file.pattern);
	log_debug("%sfilesize='%d';\n", prefix, file->file.filesize);
	log_debug("%sfilename='%s';\n", prefix, file->file.filename);
	log_debug("%srecordlen='%d';\n", prefix, file->file.recordlen);
	log_debug("%smode='%d';\n", prefix, file->file.mode);
	log_debug("%stype='%d';\n", prefix, file->file.type);
	log_debug("%sattr='%d';\n", prefix, file->file.attr);
	log_debug("%swritable='%d';\n", prefix, file->file.writable);
	log_debug("%sseekable='%d';\n", prefix, file->file.seekable);

	log_debug("%snext_track='%d';\n", prefix, file->next_track);
	log_debug("%snext_sector='%d';\n", prefix, file->next_sector);
	log_debug("%scht='%d';\n", prefix, file->cht);
	log_debug("%schs='%d';\n", prefix, file->chs);
	log_debug("%schp='%d';\n", prefix, file->chp);

}

#if 0

// *************
// di_dump_block
// *************

static void di_dump_block(uint8_t * b)
{
	int i, j;
	printf("BLOCK:\n");
	for (j = 0; j < 256; j += 16) {
		for (i = 0; i < 16; ++i)
			printf(" %2.2x", b[i + j]);
		printf("   ");
		for (i = 0; i < 16; ++i) {
			if (b[i + j] > 31 && b[i + j] < 96)
				printf("%c", b[i + j]);
			else
				printf(".");
		}
		printf("\n");
	}
}

// *************
// di_print_slot
// *************

static void di_print_slot(slot_t * slot)
{
	printf("SLOT  %d\n", slot->number);
	printf("name  %s\n", slot->filename);
	printf("pos   %6.6x\n", slot->pos);
	printf("size  %6d\n", slot->size);
	printf("type  %6d\n", slot->type);
	printf("eod   %6d\n", slot->eod);
	printf("next  (%d/%d)\n", slot->next_track, slot->next_sector);
	printf("start (%d/%d)\n", slot->start_track, slot->start_sector);
}

// **************
// di_print_block
// **************

static void di_print_block(di_endpoint_t * diep, int pos)
{
	uint8_t b[16];
	int i, j;
	di_fseek_pos(diep, pos);
	printf("BLOCK: %x\n", pos);
	for (j = 0; j < 256; j += 16) {
		fread(b, 1, 16, diep->Ip);
		for (i = 0; i < 16; ++i)
			printf(" %2.2x", b[i]);
		printf("   ");
		for (i = 0; i < 16; ++i) {
			if (b[i] > 31 && b[i] < 96)
				printf("%c", b[i]);
			else
				printf(".");
		}
		printf("\n");
	}
}

#endif

// ------------------------------------------------------------------
// close file

// ***********
// di_close_fd
// ***********

static int di_close_fd(di_endpoint_t * diep, File * f)
{
	uint8_t t, s, p;

	log_debug
	    ("di_close_fd: diep=%p Closing file %p (%s) access mode = %d\n",
	     diep, f, f->file.filename, f->access_mode);

	if (f->access_mode == 0) {
		// no access mode - not opened, so just return
		// happens after handler gets an error from di_open
		return CBM_ERROR_OK;
	}
	// make sure cht/chs are valid
//  if (!di_update_chx(diep, f, 0)) {
	// not EOF, i.e. cht is not zero

	if (f->access_mode == FS_OPEN_WR ||
	    f->access_mode == FS_OPEN_OW || f->access_mode == FS_OPEN_AP) {
		t = 0;
		s = f->chp + 1;
		di_fseek_tsp(diep, f->cht, f->chs, 0);
		di_fwrite(&t, 1, 1, diep->Ip);
		di_fwrite(&s, 1, 1, diep->Ip);
		log_debug("%p: Updated chain to (%d/%d)\n", diep, t, s);
		di_write_slot(diep, &f->Slot);	// Save new status of directory entry
		log_debug("%p: Status of directory entry saved\n", diep);
		di_sync_BAM(diep);	// Save BAM status
		log_debug("%p: BAM saved.\n", diep);
		di_fsync(diep->Ip);
	} else if (f->access_mode == FS_OPEN_RW) {
		p = f->chp + 1;
		di_fseek_tsp(diep, f->cht, f->chs, 0);
		di_fread(&t, 1, 1, diep->Ip);
		di_fread(&s, 1, 1, diep->Ip);
		if (t == 0 && p > s) {
			// only update the file chain if we're not writing in the middle of it
			di_fseek_tsp(diep, f->cht, f->chs, 1);
			di_fwrite(&p, 1, 1, diep->Ip);
			log_debug("Updated chain to (%d/%d)\n", t, p);
			di_write_slot(diep, &f->Slot);	// Save new status of directory entry
			log_debug("Status of directory entry saved\n");
			di_sync_BAM(diep);	// Save BAM status
			log_debug("BAM saved.\n");
			di_fsync(diep->Ip);
		}
	} else {
		log_debug("Closing read only file, no sync required.\n");
	}
//  } // end if update_chx

	if (f->dospattern != NULL) {
		// discard const
		mem_free((char *)f->dospattern);
	}
	//di_init_fp(f);
	return 0;
}

static void di_close(file_t * fp, int recurse)
{
	File *file = (File *) fp;
	(void)recurse;		// unused, as we have no subdirs

	di_endpoint_t *diep = (di_endpoint_t *) fp->endpoint;

	di_close_fd(diep, file);

	reg_remove(&diep->base.files, file);

	if (reg_size(&diep->base.files) == 0) {
		di_freeep(fp->endpoint);
	}

	mem_free(file);
}

static int di_equals(file_t * thisfile, file_t * otherfile)
{

	if (otherfile->handler != &di_file_handler) {
		return 1;
	}

	if (otherfile->endpoint != thisfile->endpoint) {
		return 1;
	}

	return ((File *) thisfile)->Slot.number -
	    ((File *) otherfile)->Slot.number;
}

/**
 * make an endpoint from the root directory for an assign
 * (handler_resolve_assign only calls this on isdir=1 which is root dir here only
 */
static int di_to_endpoint(file_t * file, endpoint_t ** outep)
{

	if (file->handler != &di_file_handler) {
		log_error("Wrong file type (unexpected)\n");
		return CBM_ERROR_FAULT;
	}

	endpoint_t *ep = file->endpoint;
	*outep = ep;

	// prevent it from being closed here
	ep->is_assigned++;

	di_close(file, 1);

	// reset counter
	ep->is_assigned--;

	log_debug("di_to_endpoint: file=%p -> diep=%p\n", file, *outep);

	return CBM_ERROR_OK;
}

// *********
// di_root - get the a directory file_t for the root directory, "$" in this case
// Used in starting a file name parse, when this endpoint is assigned to a drive.
// *********
//
static file_t *di_root(endpoint_t * ep)
{

	di_endpoint_t *diep = (di_endpoint_t *) ep;

	File *file = di_reserve_file(diep);
	file->file.endpoint = ep;
	file->file.filename = mem_alloc_str("$");
	//file->file.pattern = mem_alloc_str("*");
	file->file.writable = diep->Ip->writable;
	file->file.isdir = 1;

	// TODO: move from global to file
	di_first_slot(diep, &diep->Slot);

	log_debug("di_root: diep=%p -> root=%p\n", file);

	return (file_t *) file;
}

// *********
// di_wrap
// *********
//
// wrap a file_t that represents a Dxx file into a temporary endpoint, 
// and return the root file_t of it to access the directory of the 
// Dxx image.
//
// This is called when traversing a path, so Dxx files can be "seen" as 
// subdirectories e.g. in another container (larger Dxx file, zip file etc).

static int di_wrap(file_t * file, file_t ** wrapped)
{
	cbm_errno_t err = CBM_ERROR_FILE_NOT_FOUND;

	log_debug("di_wrap:\n");

	// first check name
	const char *name = file->filename;
	int l = 0;
	if (name == NULL || (l = strlen(name)) < 4) {
		return err;
	}

	if (name[l - 4] != '.' || (name[l - 3] != 'd' && name[l - 3] != 'D')) {
		return err;
	}
	// check existing endpoints (so we re-use them and do not confuse
	// underlying data structures by parallel access paths

	for (int i = 0;; i++) {
		di_endpoint_t *diep = reg_get(&di_endpoint_registry, i);

		if (diep == NULL) {
			// no more endpoint
			break;
		}

		log_debug("checking ep %p for reuse (root=%p)\n", diep,
			  diep->Ip);

		if (!diep->Ip->handler->equals(diep->Ip, file)) {
			// root of endpoint equals the given parent file
			// so we reuse the endpoint

			log_debug("Found ep %p to reuse with file %p\n", diep,
				  file);

			*wrapped = di_root((endpoint_t *) diep);

			// closing original path
			log_debug("Closing original file %p (recursive)\n",
				  diep, file);
			file->handler->close(file, 1);

			return CBM_ERROR_OK;
		}
	}

	// allocate a new endpoint
	di_endpoint_t *newep = (di_endpoint_t *) di_newep(name);
	newep->Ip = file;
	newep->base.is_temporary = 1;

	if ((err = di_load_image(newep, file)) == CBM_ERROR_OK) {
		// image identified correctly
		*wrapped = di_root((endpoint_t *) newep);

		log_debug("di_wrap (%p: %s w/ pattern %s) -> %p\n",
			  file, file->filename, file->pattern, *wrapped);

		(*wrapped)->pattern =
		    file->pattern == NULL ? NULL : mem_alloc_str(file->pattern);

		file_t *parent = file->handler->parent(file);
		if (parent != NULL) {
			// loose parent reference
			while (file != NULL) {
				if (file->parent == parent) {
					file->parent = NULL;
					break;
				}
				file = file->parent;
			}
			parent->handler->close(parent, 1);
		}

		err = CBM_ERROR_OK;
	} else {
		// we don't need to close file, so clear it here
		newep->Ip = NULL;
		di_freeep((endpoint_t *) newep);
	}

	return err;
}

// ----------------------------------------------------------------------------------

handler_t di_file_handler = {
	"di_file_handler",
	NULL,			// resolve - not required
	di_close,		// close
	di_open,		// open a file_t
	handler_parent,		// default parent() impl
	di_seek,		// seek
	di_readfile,		// readfile
	di_writefile,		// writefile
	NULL,			// truncate
	di_direntry,		// direntry
	di_create,		// create
	di_fflush,		// flush data to disk
	di_equals,		// check if two files are the same
	di_realsize,		// compute and return the real linear file size
	di_scratch,		// scratch
	NULL,			// mkdir not supported
	NULL,			// rmdir not supported
	di_move,		// move a file
	di_dump_file		// dump
};

provider_t di_provider = {
	"di",
	"PETSCII",
	di_init,
	NULL,			// newep - not needed as only via wrap
	NULL,			// tempep - not needed as only via wrap
	di_to_endpoint,		// to_endpoint
	di_ep_free,		// unassign
	di_root,		// file_t* (*root)(endpoint_t *ep);  // root directory for the endpoint
	di_wrap,		// wrap while CDing into D64 file
	di_direct,
	di_format,		// format
	di_dump			// dump
};
