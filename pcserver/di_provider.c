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


// structure for directory slot handling

typedef struct
{
   int   number;          // current slot number
   int   pos;             // file position
   int   size;            // file size in (254 byte) blocks
   uint8_t  next_track;   // next directory track
   uint8_t  next_sector;  // next directory sector
   uint8_t  filename[20]; // filename (C string zero terminated)
   uint8_t  type;         // file type
   uint8_t  start_track;  // first track
   uint8_t  start_sector; // first sector
   uint8_t  ss_track;     // side sector track
   uint8_t  ss_sector;    // side sector sector
   uint8_t  recordlen;    // REL file record length
   uint8_t  eod;          // end of directory
} slot_t;

typedef struct
{
   file_t file;
   slot_t Slot;           // 
   uint8_t  *buf;         // direct channel block buffer
   uint8_t  CBM_file[20]; // filename with CBM charset
   const char *dospattern;  // directory match pattern in PETSCII
   uint8_t  is_first;     // is first directory entry?
   uint8_t  next_track;
   uint8_t  next_sector;
   uint8_t  cht;          // chain track
   uint8_t  chs;          // chain sector
   uint8_t  chp;          // chain pointer
   uint8_t  access_mode;
   uint16_t lastpos;	  // last P record number + 1, to expand to on write if > 0
   uint16_t maxrecord;	  // the last record number available in the file
} File;

typedef struct
{                                     // derived from endpoint_t
   endpoint_t    base;                // payload
   file_t       *Ip;                  // Image file pointer
   //char         *curpath;             // malloc'd current path
   Disk_Image_t  DI;                  // mounted disk image
   uint8_t      *BAM[4];              // Block Availability Maps
   int           BAMpos[4];           // File position of BAMs
   uint8_t      *buf[5];              // direct channel block buffer
   uint8_t       chan[5];             // channel #
   uint8_t       bp[5];               // buffer pointer
   uint8_t       CurrentTrack;        // start track for scannning of BAM
   uint8_t       U2_track;            // track  for U2 command
   uint8_t       U2_sector;           // sector for U2 command
   slot_t        Slot;                // directory slot
}  di_endpoint_t;

extern provider_t di_provider;

handler_t di_file_handler;

// prototypes
static uint8_t di_next_track(di_endpoint_t *diep);
static int di_block_alloc(di_endpoint_t *diep, uint8_t *track, uint8_t *sector);
static int di_block_free(di_endpoint_t *diep, uint8_t Track, uint8_t Sector);
static unsigned int di_rel_record_max(di_endpoint_t *diep, File *f);
static int di_expand_rel(di_endpoint_t *diep, File *f, int recordno);
static int di_position(di_endpoint_t *ep, File* fp, int recordno);
static int di_close_fd(di_endpoint_t *diep, File *f);
static void di_read_BAM(di_endpoint_t *diep);
static void di_first_slot(di_endpoint_t *diep, slot_t *slot);

// ------------------------------------------------------------------
// management of endpoints

static void endpoint_init(const type_t *t, void *obj) {
        (void) t;       // silence unused warning
        di_endpoint_t *fsep = (di_endpoint_t*)obj;
        reg_init(&(fsep->base.files), "di_endpoint_files", 16);
        fsep->base.ptype = &di_provider;
}

static type_t endpoint_type = {
        "di_endpoint",
        sizeof(di_endpoint_t),
        endpoint_init
};

// **********
// di_init_fp
// **********

static void di_init_fp(const type_t *t, void *obj)
{
  (void) t;
  File *fp = (File*)obj;

  fp->buf  = NULL;
  fp->is_first  = 0;
  fp->cht = 0;
  fp->chs = 0;
  fp->chp = 0;
  fp->file.handler = &di_file_handler;
  fp->dospattern = NULL;
}

static type_t file_type = {
        "di_file",
        sizeof(File),
        di_init_fp
};

// ***************
// di_reserve_file
// ***************

static File *di_reserve_file(di_endpoint_t *diep)
{
	File *file = mem_alloc(&file_type);
	file->file.endpoint = (endpoint_t*)diep;

        log_debug("di_reserve_file %p\n", file);

	reg_append(&diep->base.files, file);

	return file;
}

// *************
// di_load_image
// *************

static cbm_errno_t di_load_image(di_endpoint_t *diep, const file_t *file)
{
   
   log_debug("image size = %d\n",diep->Ip->filesize);

   if (diskimg_identify(&(diep->DI), diep->Ip->filesize)) {
	int numbamblocks = diep->DI.BAMBlocks;
	for (int i = 0; i < numbamblocks; i++) {
		diep->BAMpos[i] = 256 * diep->DI.LBA(diep->DI.bamts[(i<<1)], diep->DI.bamts[(i<<1)+1]);
	}
   }
   else 
   {
      log_error("Invalid/unsupported disk image\n");
      return CBM_ERROR_FILE_TYPE_MISMATCH; // not an image file
   }

   di_read_BAM(diep);
   log_debug("di_load_image(%s) as d%d\n",file->filename,diep->DI.ID);
   return CBM_ERROR_OK; // success
}

// ********
// di_newep
// ********

static endpoint_t *di_newep(endpoint_t *parent, const char *path)
{
   (void) parent; // silence -Wunused-parameter

   //int i;
   di_endpoint_t *diep = malloc(sizeof(di_endpoint_t));
   //diep->curpath = malloc(strlen(path)+1);
   //strcpy(diep->curpath,path);
   //for(int i=0;i<MAXFILES;i++) di_init_fp(&(diep->files[i]));
   diep->base.ptype = &di_provider;
   //if(!di_load_image(diep,path)) return NULL; // not a valid disk image
   //for (i=0 ; i < 5 ; ++i) diep->chan[i] = -1;
   log_debug("di_newep(%s) = %p\n",path,diep);
   return (endpoint_t*) diep;
}

// *********
// di_tempep
// *********

static endpoint_t *di_tempep(char **name)
{
   while (**name == dir_separator_char()) (*name)++;

   // cut off last filename part (either file name or dir mask)
   char *end = strrchr(*name, dir_separator_char());

   di_endpoint_t *diep = NULL;

   if (end != NULL) // we have a '/'
   {
      *end = 0;
      diep = (di_endpoint_t*) di_newep(NULL, *name);
      *name = end+1;  // filename part
  }
  else // no '/', so only mask, path is root
  {
    diep = (di_endpoint_t*) di_newep(NULL, ".");
  }
  log_debug("di_tempep(%s) = %p\n",*name,diep);
  return (endpoint_t*) diep;
}

// *********
// di_freeep
// *********

static void di_freeep(endpoint_t *ep)
{
	File *f = NULL;
   	di_endpoint_t *cep = (di_endpoint_t*) ep;
	if (reg_size(&ep->files)) {
		log_warn("di_freeep(): closing endpoint with %n open files!\n", reg_size(&ep->files));
	}

        while ((f = (File*)reg_get(&ep->files, 0)) != NULL) {
		log_warn("di_freeep(): force closing file %p\n", reg_size(&ep->files));
                di_close_fd(cep, f);
        }
   	mem_free(ep);
}

// *********
// di_root
// *********
//
static file_t* di_root(endpoint_t *ep, uint8_t isroot) {

	log_debug("di_root:\n");

   	di_endpoint_t *diep = (di_endpoint_t*) ep;

	File *file = di_reserve_file(diep);

	file->file.filename = mem_alloc_str("$");

	// TODO: move from global to file
        di_first_slot(diep,&diep->Slot);

	log_debug("di_root -> root=%p\n", file);

	return (file_t*)file;
}



// *********
// di_wrap
// *********
//
// wrap a file_t that represents a Dxx file into a temporary endpoint, 
// and return the root file_t of it to access the directory of the 
// Dxx image.

static int di_wrap(file_t *file, file_t **wrapped)
{
	cbm_errno_t err = CBM_ERROR_FILE_NOT_FOUND;

	log_debug("di_wrap:\n");

	// first check name
	const char *name = file->filename;
	int l = 0;
	if (name == NULL || (l = strlen(name)) < 4) {
		return err;
	}
	
	if (name[l - 4] != '.' || 
		(name [l-3] != 'd' && name [l-3] != 'D')) {
		return err;
	}

   	di_endpoint_t *diep = mem_alloc(&endpoint_type);
	diep->Ip = file;
	diep->base.is_temporary = 1;

	if ((err = di_load_image(diep, file)) == CBM_ERROR_OK) {
		// image identified correctly
		*wrapped = di_root((endpoint_t*)diep, 1);

		log_debug("di_wrap (%p: %s w/ pattern %s) -> %p\n", 
				file, file->filename, file->pattern, *wrapped);

		(*wrapped)->pattern = mem_alloc_str(file->pattern);
		err = CBM_ERROR_OK;
	} else {
		di_freeep((endpoint_t*)diep);
	}

	return err;
}


// ------------------------------------------------------------------
// adapter methods to handle indirection via file_t instead of FILE*
// note: this provider reads/writes single bytes in many cases
// always(?) preceeded by a seek, so there is room for improvements

static inline cbm_errno_t di_fseek(file_t *file, long pos, int whence) {
	return file->handler->seek(file, pos, whence);
}

static inline void di_fread(void *ptr, size_t size, size_t nmemb, file_t *file) {
	int readfl;
	file->handler->readfile(file, (char*) ptr, size * nmemb, &readfl);
}

static inline void di_fwrite(void *ptr, size_t size, size_t nmemb, file_t *file) {
	file->handler->writefile(file, (char*) ptr, size * nmemb, 0);
}

static inline void di_fflush(file_t *file) {
	// TODO
}

static inline void di_fsync(file_t *file) {
	// TODO
     // if(res) log_error("os_fsync failed: (%d) %s\n", os_errno(), os_strerror(os_errno()));
}

// ************
// di_assert_ts
// ************

static int di_assert_ts(di_endpoint_t *diep, uint8_t track, uint8_t sector)
{
   if (diep->DI.LBA(track, sector) < 0)
      return CBM_ERROR_ILLEGAL_T_OR_S;
   return CBM_ERROR_OK;
}

// ************
// di_fseek_tsp
// ************

static void di_fseek_tsp(di_endpoint_t *diep, uint8_t track, uint8_t sector, uint8_t ptr)
{
   long seekpos = ptr+256*diep->DI.LBA(track,sector);
   log_debug("seeking to position %ld for t/s/p=%d/%d/%d\n", seekpos, track, sector, ptr);
   di_fseek(diep->Ip, seekpos,SEEKFLAG_ABS);
}

// ************
// di_fseek_pos
// ************

static void di_fseek_pos(di_endpoint_t *diep, int pos)
{
   di_fseek(diep->Ip,pos,SEEKFLAG_ABS);
}

// *************
// di_write_slot
// *************

static void di_write_slot(di_endpoint_t *diep, slot_t *slot)
{
   uint8_t p[32];

   log_debug("di_write_slot %d\n",slot->number);
   // di_print_slot(slot);
   memset(p,0,32);      // clear slot
   memset(p+5,0xa0,16); // fill name with $A0
   memcpy(p+5,slot->filename,strlen((char *)slot->filename));
   p[ 2] = slot->type;
   p[ 3] = slot->start_track;
   p[ 4] = slot->start_sector;
   p[21] = slot->ss_track;
   p[22] = slot->ss_sector;
   p[23] = slot->recordlen;
   p[30] = slot->size & 0xff;
   p[31] = slot->size >> 8;

   log_debug("di_write_slot pos %x\n",slot->pos);
   di_fseek_pos(diep,slot->pos+2);
   di_fwrite(p+2,1,30,diep->Ip);
   // di_print_block(diep,slot->pos & 0xffffff00);
}

// ***********
// di_sync_BAM
// ***********

static void di_sync_BAM(di_endpoint_t *diep)
{
   int i;

   for (i=0 ; i < diep->DI.BAMBlocks ; ++i)
   {
      di_fseek_pos(diep,diep->BAMpos[i]);
      di_fwrite(diep->BAM[i],1,256,diep->Ip);
   }
}

// ***********
// di_close_fd
// ***********

static int di_close_fd(di_endpoint_t *diep, File *f)
{
  uint8_t t,s,p;

  log_debug("Closing file %p access mode = %d\n", f, f->access_mode);

  if (f->access_mode == FS_OPEN_WR ||
      f->access_mode == FS_OPEN_OW ||
      f->access_mode == FS_OPEN_AP)
  {
     t = 0;
     s = f->chp+1;
     di_fseek_tsp(diep,f->cht,f->chs,0);
     di_fwrite(&t,1,1,diep->Ip);
     di_fwrite(&s,1,1,diep->Ip);
     log_debug("Updated chain to (%d/%d)\n",t,s);
     di_write_slot(diep,&f->Slot); // Save new status of directory entry
     log_debug("Status of directory entry saved\n");
     di_sync_BAM(diep);            // Save BAM status
     log_debug("BAM saved.\n");
     di_fsync(diep->Ip);
  } else 
  if (f->access_mode == FS_OPEN_RW) {
     p = f->chp+1;
     di_fseek_tsp(diep,f->cht,f->chs,0);
     di_fread(&t,1,1,diep->Ip);
     di_fread(&s,1,1,diep->Ip);
     if (t == 0 && p > s) {
	// only update the file chain if we're not writing in the middle of it
     	di_fseek_tsp(diep,f->cht,f->chs,1);
     	di_fwrite(&p,1,1,diep->Ip);
     	log_debug("Updated chain to (%d/%d)\n",t,p);
     	di_write_slot(diep,&f->Slot); // Save new status of directory entry
     	log_debug("Status of directory entry saved\n");
     	di_sync_BAM(diep);            // Save BAM status
     	log_debug("BAM saved.\n");
     	di_fsync(diep->Ip);
     }
  } else {
    log_debug("Closing read only file, no sync required.\n");
  }

  	if (f->dospattern != NULL) {
		// discard const
		mem_free((char*)f->dospattern);
	}

  //di_init_fp(f);
  return 0;
}

// ***********
// di_read_BAM
// ***********

static void di_read_BAM(di_endpoint_t *diep)
{
   int i;

   for (i=0 ; i < diep->DI.BAMBlocks ; ++i)
   {
      diep->BAM[i] = (uint8_t *)malloc(256);
      di_fseek_pos(diep,diep->BAMpos[i]);
      di_fread(diep->BAM[i],1,256,diep->Ip);
   }
}


// ***************
// di_alloc_buffer
// ***************

static int di_alloc_buffer(di_endpoint_t *diep)
{
  log_debug("di_alloc_buffer 0\n");
  if (!diep->buf[0]) diep->buf[0] = (uint8_t *)calloc(256,1);
  if (!diep->buf[0]) return 0; // OOM
  diep->bp[0] = 0;
  return 1; // success
}

// **************
// di_load_buffer
// **************

static int di_load_buffer(di_endpoint_t *diep, uint8_t track, uint8_t sector)
{
   if (!di_alloc_buffer(diep)) return 0; // OOM
   log_debug("di_load_buffer %p->%p U1(%d/%d)\n",diep,diep->buf[0],track,sector);
   di_fseek_tsp(diep,track,sector,0);
   di_fread(diep->buf[0],1,256,diep->Ip);
   // di_dump_block(diep->buf[0]);
   return 1; // OK
}

// **************
// di_save_buffer
// **************

static int di_save_buffer(di_endpoint_t *diep)
{
   log_debug("di_save_buffer U2(%d/%d)\n",diep->U2_track,diep->U2_sector);
   di_fseek_tsp(diep,diep->U2_track,diep->U2_sector,0);
   di_fwrite(diep->buf[0],1,256,diep->Ip);
   di_fflush(diep->Ip);
   diep->U2_track = 0;
   // di_dump_block(diep->buf[0]);
   return 1; // OK
}

// **************
// di_flag_buffer
// **************

static void di_flag_buffer(di_endpoint_t *diep, uint8_t track, uint8_t sector)
{
   log_debug("di_flag_buffer U2(%d/%d)\n",track,sector);
   diep->U2_track  = track;
   diep->U2_sector = sector;
   diep->bp[0]     = 0;
}

// *************
// di_read_block
// *************

static int di_read_block(di_endpoint_t *diep, File *file, char *retbuf, int len, int *eof)
{
   log_debug("di_read_block: chan=%p len=%d\n", file, len);

   int avail = 256 - diep->bp[0];
   int n = len;
   if (len > avail)
   {
      n = avail;
      *eof = READFLAG_EOF;
   }
   log_debug("di_read_block: avail=%d, n=%d\n", avail, n);
   if (n > 0)
   {
      log_debug("memcpy(%p,%p,%d)\n",retbuf, diep->buf[0] + diep->bp[0], n);
      memcpy(retbuf, diep->buf[0] + diep->bp[0], n);
      diep->bp[0] += n;
   }
   return n;
}

// **************
// di_write_block
// **************

static int di_write_block(di_endpoint_t *diep, char *buf, int len)
{
   log_debug("di_write_block: len=%d at ptr %d\n", len, diep->bp[0]);

   int avail = 256 - diep->bp[0];
   int n = len;
   if (len > avail) n = avail;
   if (n > 0)
   {
      memcpy(diep->buf[0] + diep->bp[0], buf, n);
      diep->bp[0] += n;
   }
   return n;
}

// *********
// di_direct
// *********

static int di_direct(endpoint_t *ep, char *buf, char *retbuf, int *retlen)
{
   int rv = CBM_ERROR_OK;

   di_endpoint_t *diep = (di_endpoint_t *)ep;
   file_t *fp = NULL;

   uint8_t cmd    = (uint8_t)buf[FS_BLOCK_PAR_CMD    -1];
   uint8_t track  = (uint8_t)buf[FS_BLOCK_PAR_TRACK  -1];	// ignoring high byte
   uint8_t sector = (uint8_t)buf[FS_BLOCK_PAR_SECTOR -1];	// ignoring high byte
   uint8_t chan   = (uint8_t)buf[FS_BLOCK_PAR_CHANNEL-1];

   log_debug("di_direct(cmd=%d, tr=%d, se=%d ch=%d\n",cmd,track,sector,chan);
   rv = di_assert_ts(diep,track,sector);
   if (rv != CBM_ERROR_OK) return rv; // illegal track or sector

   switch (cmd)
   {
      case FS_BLOCK_BR:
      case FS_BLOCK_U1: 
	di_load_buffer(diep,track,sector); 
   	diep->chan[0] = chan; // assign channel # to buffer

        //handler_resolve_block(ep, chan, &fp);

        channel_set(chan, fp);
	break;
      case FS_BLOCK_BW:
      case FS_BLOCK_U2: 
      	if (!di_alloc_buffer(diep)) {
		return CBM_ERROR_NO_CHANNEL; // OOM
	}
	di_flag_buffer(diep,track,sector); 
   	diep->chan[0] = chan; // assign channel # to buffer
        //handler_resolve_block(ep, chan, &fp);

        channel_set(chan, fp);
      case FS_BLOCK_BA:
	rv = di_block_alloc(diep, &track, &sector);
	break;
      case FS_BLOCK_BF:
	rv = di_block_free(diep, track, sector);
	break;
   }

   retbuf[0] = track;	// low byte
   retbuf[1] = 0;	// high byte
   retbuf[2] = sector;	// low byte
   retbuf[3] = 0;	// high byte
   *retlen = 4;

   return rv;
}


// ************
// di_read_slot
// ************

static void di_read_slot(di_endpoint_t *diep, slot_t *slot)
{
   int i=0;
   uint8_t p[32];
   di_fseek_pos(diep,slot->pos);
   di_fread(p,1,32,diep->Ip);
   memset(slot->filename,0,20);
   while (i < 16 && p[i+5] != 0xa0)
   {
      slot->filename[i] = p[i+5];
      ++i;
   }
   slot->filename[i]  = 0;
   slot->type         = p[ 2];
   slot->start_track  = p[ 3];
   slot->start_sector = p[ 4];
   slot->size         = p[30] + 256 * p[31];
   if ((slot->pos & 0xff) == 0) // first slot of block
   {
      slot->next_track  = p[0];
      slot->next_sector = p[1];
   }
   slot->ss_track     = p[21];
   slot->ss_sector    = p[22];
   slot->recordlen    = p[23];

   log_debug("di_read_slot <(%02x ...) %s>\n",slot->filename[0], slot->filename);
}

// *************
// di_first_slot
// *************

// TODO: check consistency with real images, resp. DirSector in the disk image definitions
static void di_first_slot(di_endpoint_t *diep, slot_t *slot)
{
   log_debug("di_first_slot\n");
   if (diep->DI.ID == 80 || diep->DI.ID == 82)
      slot->pos  = 256 * diep->DI.LBA(39,1);
   else if (diep->DI.ID == 81)
      slot->pos  = 256 * diep->DI.LBA(40,3);
   else
      slot->pos  = 256 * diep->DI.LBA(diep->BAM[0][0],diep->BAM[0][1]);
   slot->number  =   0;
   slot->eod     =   0;
}

// ************
// di_next_slot
// ************

static int di_next_slot(di_endpoint_t *diep,slot_t *slot)
{
   if ((++slot->number & 7) == 0) // read next dir block
   {
      log_debug("Next Slot (%d/%d)\n",slot->next_track,slot->next_sector);
      if (slot->next_track == 0)
      {
         slot->eod = 1;
         return 0; // end of directory
      }
      slot->pos = 256 * diep->DI.LBA(slot->next_track,slot->next_sector);
   }  
   else slot->pos += 32;
   return 1;
}

// *************
// di_match_slot
// *************
// @deprecated
static int di_match_slot(di_endpoint_t *diep,slot_t *slot, const uint8_t *name, uint8_t type)
{
   do
   {
      di_read_slot(diep,slot);
      if (slot->type 
		&& ((type == FS_DIR_TYPE_UNKNOWN) || ((slot->type & FS_DIR_ATTR_TYPEMASK) == type))
		&& compare_pattern((char*)slot->filename,(char*)name, advanced_wildcards)) {
		return 1; // found
      }
   }  while (di_next_slot(diep,slot));
   return 0; // not found
}

// **************
// di_clear_block
// **************

static void di_clear_block(di_endpoint_t *diep, int pos)
{
   uint8_t p[256];
   
   memset(p,0,256);
   di_fseek_pos(diep,pos);
   di_fwrite(p,1,256,diep->Ip);
}


// *******************
// di_update_dir_chain
// *******************

static void di_update_dir_chain(di_endpoint_t *diep, slot_t *slot, uint8_t sector)
{
   di_fseek_pos(diep,slot->pos & 0xffff00);
   di_fwrite(&diep->DI.DirTrack,1,1,diep->Ip);
   di_fwrite(&sector           ,1,1,diep->Ip);
}

// ***********
// di_scan_BAM
// ***********

// scan to find a new sector for a file, using the interleave
// allocate the sector found
static int di_scan_BAM_interleave(Disk_Image_t *di, uint8_t *bam, uint8_t interleave)
{
   uint8_t i; // interleave index
   uint8_t s; // sector index

   for (i=0 ; i < interleave ; ++i) {
   	for (s=i ; s < di->Sectors ; s += interleave) {
   		if (bam[s>>3] & (1 << (s & 7)))
   		{ 
   		   bam[s>>3] &= ~(1 << (s & 7));
   		   return s;
   		}
	}
   }
   return -1; // no free sector
}

// linearly scan to find a new sector for a file, for B-A
// do NOT allocate the block
static int di_scan_BAM_linear(Disk_Image_t *di, uint8_t *bam, uint8_t firstSector)
{
   uint8_t s = firstSector; // sector index

   do {
   	if (bam[s>>3] & (1 << (s & 7)))
   	{ 
   	   return s;
   	}
	s = s + 1;
	if (s >= di->Sectors) {
		s = 0;
	}
   } while (s != firstSector);

   return -1; // no free sector
}

// do allocate a block
static void di_alloc_BAM(uint8_t *bam, uint8_t sector)
{
	bam[sector>>3] &= ~(1 << (sector & 7));
}

// calculate the position of the BAM entry for a given track
static void di_calculate_BAM(di_endpoint_t *diep, uint8_t Track, uint8_t **outBAM, uint8_t **outFbl) {
   int   BAM_Number;     // BAM block for current track
   int   BAM_Offset;  
   int   BAM_Increment;
   Disk_Image_t *di = &diep->DI;
   uint8_t *fbl;            // pointer to track free blocks
   uint8_t *bam;            // pointer to track free blocks

   BAM_Number    = (Track - 1) / di->TracksPerBAM;
   BAM_Offset    = di->BAMOffset; // d64=4  d80=6  d81=16
   BAM_Increment = 1 + ((di->Sectors + 7) >> 3);
   fbl = diep->BAM[BAM_Number] + BAM_Offset
       + ((Track-1) % di->TracksPerBAM) * BAM_Increment;
   bam = fbl + 1; // except 1571 2nd. side
   if (di->ID == 71 && Track > di->Tracks)
   {
      fbl = diep->BAM[0] + 221 + (Track-36);
      bam = diep->BAM[1] +   3 * (Track-36);
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
static int di_block_alloc(di_endpoint_t *diep, uint8_t *track, uint8_t *sector) {
   int  Sector;         // sector of next free block
   Disk_Image_t *di = &diep->DI;
   uint8_t *fbl;	// pointer to free blocks byte
   uint8_t *bam;	// pointer to BAM bit field

   diep->CurrentTrack = *track;
   if (diep->CurrentTrack < 1 || diep->CurrentTrack > di->Tracks * di->Sides) {
      diep->CurrentTrack = di->DirTrack - 1; // start track
   }

   // initialize with the starting sector
   Sector = *sector;
   do
   {
	// calculate block free and BAM pointers for track
	di_calculate_BAM(diep, diep->CurrentTrack, &bam, &fbl);

	// find a free block in track
	// returns free sector found or -1 for none found
	Sector = di_scan_BAM_linear(di, bam, Sector);

	if (Sector < 0) {
		// no free block found
		// start sector for next track
		Sector = 0;
	} else {
		// found a free block
		if (diep->CurrentTrack == *track && Sector == *sector) {
			// the block found is the one requested
			// so allocate it before we return
			di_alloc_BAM(bam, Sector);
			// sync BAM to disk (image)
			di_sync_BAM(diep);
			// ok
			return CBM_ERROR_OK;
		}
		// we found another block
		*track = diep->CurrentTrack;
		*sector = Sector;
		return CBM_ERROR_NO_BLOCK;
	}
   } while (di_next_track(diep) != *track);

   return CBM_ERROR_DISK_FULL;
}


// *************
// di_scan_track
// *************

static int di_scan_track(di_endpoint_t *diep, uint8_t Track)
{
   int   Sector;
   int   Interleave;
   uint8_t *fbl;            // pointer to track free blocks
   uint8_t *bam;            // pointer to track BAM
   Disk_Image_t *di = &diep->DI;

   // log_debug("di_scan_track(%d)\n",Track);

   if (Track == di->DirTrack) Interleave = di->DirInterleave;
   else                       Interleave = di->DatInterleave;

   di_calculate_BAM(diep, Track, &bam, &fbl);

   if (fbl[0]) Sector = di_scan_BAM_interleave(di,bam,Interleave);
   else        Sector = -1;
   if (Sector >= 0) --fbl[0]; // decrease free blocks counter
   return Sector;
}

// *************************
// di_allocate_new_dir_block
// *************************

static int di_allocate_new_dir_block(di_endpoint_t *diep, slot_t *slot)
{
   int sector;

   sector = di_scan_track(diep,diep->DI.DirTrack);
   if (sector < 0) return 1; // directory full

   di_sync_BAM(diep);
   di_update_dir_chain(diep,slot,sector);
   slot->pos = 256 * diep->DI.LBA(diep->DI.DirTrack,sector);
   slot->eod = 0;
   di_clear_block(diep,slot->pos);
   slot->next_track  = 0;
   slot->next_sector = 0;
   log_debug("di_allocate_new_dir_block (%d/%d)\n",diep->DI.DirTrack,sector);
   return 0; // OK
}

// *****************
// di_find_free_slot
// *****************

static int di_find_free_slot(di_endpoint_t *diep, slot_t *slot)
{
   di_first_slot(diep,slot);
   do   
   {
      di_read_slot(diep,slot);
      if (slot->type == 0) return 0; // found
   }  while (di_next_slot(diep,slot));
   return di_allocate_new_dir_block(diep,slot);
}

// *************
// di_next_track
// *************

static uint8_t di_next_track(di_endpoint_t *diep)
{
   if (diep->CurrentTrack < diep->DI.DirTrack) // outbound
   {
      if (--diep->CurrentTrack < 1) diep->CurrentTrack = diep->DI.DirTrack + 1;
   }
   else // inbound or move to side 2
   {
      if (++diep->CurrentTrack > diep->DI.Tracks * diep->DI.Sides)
         diep->CurrentTrack = diep->DI.DirTrack - 1;
   }
   return diep->CurrentTrack;
}
   
// ******************
// di_find_free_block
// ******************

static int di_find_free_block(di_endpoint_t *diep, File *f)
{
   int  StartTrack;     // here begins the scan
   int  Sector;         // sector of next free block
   Disk_Image_t *di = &diep->DI;

   if (diep->CurrentTrack < 1 || diep->CurrentTrack > di->Tracks * di->Sides)
      diep->CurrentTrack = di->DirTrack - 1; // start track
   StartTrack = diep->CurrentTrack;

   do
   {
      Sector = di_scan_track(diep,diep->CurrentTrack);
      if (Sector >= 0)
      {
         di_sync_BAM(diep);
         f->chp = 0;
         f->cht = diep->CurrentTrack;
         f->chs = Sector;
         log_debug("di_find_free_block (%d/%d)\n",f->cht,f->chs);
         return di->LBA(diep->CurrentTrack,Sector);
      }
   } while (di_next_track(diep) != StartTrack);
   return -1; // No free block -> DISK FULL
}

// ***************
// di_create_entry 
// ***************

static int di_create_entry(di_endpoint_t *diep, File *file, const char *name, openpars_t *pars)
{
   log_debug("di_create_entry(%s)\n",name);

   if (!file) return CBM_ERROR_FAULT;
   if (di_find_free_slot(diep,&file->Slot)) return CBM_ERROR_DISK_FULL;
   strcpy((char *)file->Slot.filename,name);
   file->Slot.type = 0x80 | pars->filetype;
   file->chp = 0;
   file->Slot.ss_track  = 0;	// invalid, i.e. new empty file if REL
   file->Slot.ss_sector = 0;
   file->Slot.recordlen = pars->recordlen;
   if (pars->filetype != FS_DIR_TYPE_REL) {
   	if (di_find_free_block(diep,file) < 0)   return CBM_ERROR_DISK_FULL;
   	file->Slot.start_track  = file->cht;
   	file->Slot.start_sector = file->chs;
   	file->Slot.size = 1;
   } else {
   	file->Slot.start_track  = 0;
   	file->Slot.start_sector = 0;

   	// store number of actual records in file; will store 0 on new file
   	file->maxrecord = di_rel_record_max(diep, file);
	// expand file to at least one record (which extends to the first block)
	di_expand_rel(diep, file, 1);
   	log_debug("Setting maxrecord to %d\n", file->maxrecord);
   }
   di_write_slot(diep,&file->Slot);
   // di_print_slot(&file->Slot);
   return CBM_ERROR_OK;
}

// *************
// di_pos_append
// *************

static void di_pos_append(di_endpoint_t *diep, File *f)
{
   uint8_t t,s,nt,ns; 
   
   nt = f->next_track;
   ns = f->next_sector;
   while (nt)
   {
      t = nt;
      s = ns;
      di_fseek_tsp(diep,t,s,0);
      di_fread(&nt,1,1,diep->Ip);
      di_fread(&ns,1,1,diep->Ip);
   }
   f->cht =  t;
   f->chs =  s;
   f->chp = ns-1;
   log_debug("di_pos_append (%d/%d) %d\n",t,s,ns);
}

// ************
// di_open_dir
// ************
// open a directory read
static int di_open_dir(File *file) {

	int er = CBM_ERROR_FAULT;

        log_debug("ENTER: di_open_dr(%p (%s))\n",file,
                        (file == NULL)?"<nil>":file->file.filename);

	// crude root check
        if(strcmp(file->file.filename, "$") == 0) {
          file->file.dirstate = DIRSTATE_FIRST;

          log_exitr(CBM_ERROR_OK);
          return CBM_ERROR_OK;
        } else {
          log_error("Error opening directory");
          log_exitr(er);
          return er;
        }
}


// ************
// di_open_file
// ************

static int di_open_file(File *file, openpars_t *pars, int di_cmd)
{
   int np,rv;

   const char *filename = file->file.filename;

   log_info("OpenFile(%s,%c,%d)\n", filename, pars->filetype + 0x30, pars->recordlen);

   if (pars->recordlen > 254) {
	return CBM_ERROR_OVERFLOW_IN_RECORD;
   }

   di_endpoint_t *diep = (di_endpoint_t*) (file->file.endpoint);
 
   int file_required       = false;
   int file_must_not_exist = false;

   file->access_mode = di_cmd;
 
   switch(di_cmd)
   {
      case FS_OPEN_AP:
      case FS_OPEN_RD: file_required       = true; break;
      case FS_OPEN_WR: file_must_not_exist = true; break;
      case FS_OPEN_OW: break;
      case FS_OPEN_RW:
	 if (pars->filetype != FS_DIR_TYPE_REL) {
         	log_error("Read/Write currently only supported for REL files on disk images\n");
		return CBM_ERROR_FAULT;
	 }
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
	np=1;
   } else {
   	di_first_slot(diep,&file->Slot);
   	np  = di_match_slot(diep,&file->Slot,(const uint8_t*) filename, pars->filetype);
   	file->next_track  = file->Slot.start_track;
   	file->next_sector = file->Slot.start_sector;
	if ((pars->filetype == FS_DIR_TYPE_REL) || ((file->Slot.type & FS_DIR_ATTR_TYPEMASK) == FS_DIR_TYPE_REL) ) {
		pars->filetype = FS_DIR_TYPE_REL;
		file->access_mode = FS_OPEN_RW;
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
				if (pars->recordlen != file->Slot.recordlen) {
					return CBM_ERROR_RECORD_NOT_PRESENT;
				}
			}
		}
	}
   }
   file->chp = 255;
   log_debug("File starts at (%d/%d)\n",file->next_track,file->next_sector);
   if (file_required && np == 0)
   {
     log_error("Unable to open '%s': file not found\n", filename);
     return CBM_ERROR_FILE_NOT_FOUND;
   }
   if (file_must_not_exist && np > 0)
   {
     log_error("Unable to open '%s': file exists\n", filename);
     return CBM_ERROR_FILE_EXISTS;
   }
   if (!np)
   {
      if (pars->filetype == FS_DIR_TYPE_UNKNOWN) {
	pars->filetype = FS_DIR_TYPE_PRG;
      }
      rv = di_create_entry(diep, file, filename, pars);
      if (rv != CBM_ERROR_OK) return rv;
   }

   if (di_cmd == FS_OPEN_AP) {
	di_pos_append(diep,file);
   }
   return (pars->filetype == FS_DIR_TYPE_REL) ? CBM_ERROR_OPEN_REL : CBM_ERROR_OK;
}


char *extension[6] = { "DEL","SEQ","PRG","USR","REL","CBM" };

// *************
// di_fill_entry
// *************

static int di_fill_entry(uint8_t *dest, slot_t *slot)
{
   char *p = (char *)dest + FS_DIR_NAME;

   log_debug("di_fill_entry(%s, %d blocks)\n", slot->filename, slot->size);

   dest[FS_DIR_LEN  ] = 0;
   dest[FS_DIR_LEN+1] = slot->size;
   dest[FS_DIR_LEN+2] = slot->size >> 8;
   dest[FS_DIR_LEN+3] = 0;
   dest[FS_DIR_MODE]  = FS_DIR_MOD_FIL;
   
   memset(dest+FS_DIR_YEAR,0,6);	// date+time

   strcpy(p,(const char *)slot->filename);

   dest[FS_DIR_ATTR] = ((slot->type &
	(FS_DIR_ATTR_SPLAT | FS_DIR_ATTR_LOCKED
	  | FS_DIR_ATTR_TRANS | FS_DIR_ATTR_TYPEMASK))
	| FS_DIR_ATTR_ESTIMATE)
	^ FS_DIR_ATTR_SPLAT;	// the other way round

   return FS_DIR_NAME + strlen(p) + 1;
}

// *******************
// di_directory_header
// *******************

static int di_directory_header(char *dest, di_endpoint_t *diep)
{
   memset(dest+FS_DIR_LEN,0,4);		// length
   memset(dest+FS_DIR_YEAR,0,6);	// date+time
   dest[FS_DIR_MODE]  = FS_DIR_MOD_NAM;

   if (diep->DI.ID == 80 || diep->DI.ID == 82)
   {
      di_fseek_tsp(diep,39,0, 6);
      di_fread(dest+FS_DIR_NAME   ,1,16,diep->Ip);
      di_fseek_tsp(diep,39,0,24);
      di_fread(dest+FS_DIR_NAME+16,1, 5,diep->Ip);
   }
   else if (diep->DI.ID == 81)
   {
      di_fseek_tsp(diep,40,0, 4);
      di_fread(dest+FS_DIR_NAME   ,1,16,diep->Ip);
      di_fseek_tsp(diep,40,0,22);
      di_fread(dest+FS_DIR_NAME+16,1, 5,diep->Ip);
   }
   else
   {
      memcpy(dest+FS_DIR_NAME   ,diep->BAM[0]+0x90,16);
      memcpy(dest+FS_DIR_NAME+16,diep->BAM[0]+0xA2, 5);
   }
   // fix up $a0 into $20 characters
   for (int i = FS_DIR_NAME; i < FS_DIR_NAME + 22; i++) {
	if (dest[i] == 0xa0) {
		dest[i] = 0x20;
	}
   }
   dest[FS_DIR_NAME + 22] = 0;
   log_debug("di_directory_header (%s)\n",dest+FS_DIR_NAME);
   return FS_DIR_NAME + 23; 
}

// **************
// di_blocks_free
// **************

static int di_blocks_free(char *dest, di_endpoint_t *diep)
{
   int           FreeBlocks;
   int           BAM_Number;        // BAM block for current track
   int           BAM_Increment;
   int           Track;
   int           i;
   uint8_t      *fbl;               // pointer to track free blocks
   Disk_Image_t *di = &diep->DI;

   FreeBlocks    = 0; 
   BAM_Number    = 0;
   BAM_Increment = 1 + ((di->Sectors + 7) >> 3);
   Track         = 1;

   while (BAM_Number < 4 && diep->BAM[BAM_Number])
   {
      fbl = diep->BAM[BAM_Number] + di->BAMOffset;
      if (di->ID == 71 && Track > di->Tracks)
      {
         fbl = diep->BAM[0] + 221;
         BAM_Increment = 1;
      }
      for (i=0 ; i < di->TracksPerBAM && Track <= di->Tracks * di->Sides; ++i)
      {
         if (Track != di->DirTrack) FreeBlocks += *fbl;
         fbl += BAM_Increment;
         ++Track;
      }
      ++BAM_Number;
   }

   log_debug("di_blocks_free: %u\n", FreeBlocks);
   FreeBlocks <<= 8;

   dest[FS_DIR_ATTR]  = FS_DIR_ATTR_ESTIMATE;
   dest[FS_DIR_LEN+0] = FreeBlocks;
   dest[FS_DIR_LEN+1] = FreeBlocks >>  8;
   dest[FS_DIR_LEN+2] = FreeBlocks >> 16;
   dest[FS_DIR_LEN+3] = 0;
   dest[FS_DIR_MODE]  = FS_DIR_MOD_FRE;
   dest[FS_DIR_NAME]  = 0;
   return FS_DIR_NAME + 1; 
}

/*******************
 * get the next directory entry in the directory given as fp.
 * If isresolve is set, then the disk header and blocks free entries are skipped
 * 
 * TODO: move directory traversing from endpoint to file !!!!!!!!!
 */
static int di_direntry(file_t *fp, file_t **outentry, int isresolve, int *readflag) {

	// here we (currently) only use it in resolve, not in read_dir_entry,
	// so we don't care about isresolve and first/last entry

	log_debug("di_direntry(fp=%p)\n", fp);

	cbm_errno_t rv = CBM_ERROR_FAULT;

   	if (!fp) {
		return rv;
	}

	rv = CBM_ERROR_OK;
	*outentry = NULL;

	di_endpoint_t *diep = (di_endpoint_t*)fp->endpoint;
	File *file = (File*) fp;

	if (file->dospattern == NULL) {
		char *pattern = mem_alloc_str(fp->pattern);
		provider_convto(diep->base.ptype)(pattern, strlen(pattern), pattern, strlen(pattern));
		file->dospattern = pattern;
	}

	*readflag = READFLAG_DENTRY;
	const char *outpattern;
	file_t *wrapfile = NULL;

	do {

		if (diep->Slot.eod) {
			*outentry = NULL;
			fp->dirstate = DIRSTATE_END;
			// end of search
			break;
		}

      		di_read_slot(diep,&diep->Slot);

		File *entry = di_reserve_file(diep);

		entry->file.parent = fp;
		entry->file.mode = FS_DIR_MOD_FIL;
		entry->file.type = diep->Slot.type & FS_DIR_ATTR_TYPEMASK;
		entry->file.attr = diep->Slot.type & (~FS_DIR_ATTR_TYPEMASK);
		// convert to external charset
		entry->file.filename = conv_from_alloc((const char*)diep->Slot.filename, &di_provider); 

// TODO		
//		if (diep->base.writable) {
//			entry->file.attr |= FS_DIR_ATTR_LOCKED;
//		}

		if ( handler_next((file_t*)entry, FS_OPEN_DR, file->dospattern, &outpattern, &wrapfile)
			== CBM_ERROR_OK) {
			*outentry = wrapfile;
			rv = CBM_ERROR_OK;
			break;
		}

		// cleanup to read next entry
		entry->file.handler->close((file_t*)entry, 0);
		entry = NULL;
      		di_next_slot(diep,&diep->Slot);
   	} while (1);

	return rv;
}


// *****************
// di_read_dir_entry
// *****************

static int di_read_dir_entry(di_endpoint_t *diep, File *file, char *retbuf, int *eof)
{
   int rv = 0;
   log_debug("di_read_dir_entry(%p, dospattern=(%02x ...) %s)\n",file,file->dospattern[0], file->dospattern);

   if (!file) return -CBM_ERROR_FAULT;

   *eof = READFLAG_DENTRY;

   if (file->file.dirstate == DIRSTATE_FIRST)
   {
      file->file.dirstate ++;
      rv = di_directory_header(retbuf,diep);
      di_first_slot(diep,&diep->Slot);
      return rv;
   }

   if (!diep->Slot.eod && di_match_slot(diep,&diep->Slot,(uint8_t*)file->dospattern, FS_DIR_TYPE_UNKNOWN))
   {    
      rv = di_fill_entry((uint8_t *)retbuf,&diep->Slot);
      di_next_slot(diep,&diep->Slot);
      return rv;
   }

   if (file->file.dirstate != DIRSTATE_END) {
      file->file.dirstate = DIRSTATE_END;
      rv = di_blocks_free(retbuf, diep);
   }
   *eof |= READFLAG_EOF;
   return rv;
}

// ************
// di_read_byte
// ************

static int di_read_byte(di_endpoint_t *diep, File *f, char *retbuf)
{
   if (f->chp > 253)
   {
      f->chp = 0;
      if (f->next_track == 0) return READFLAG_EOF; // EOF
      di_fseek_tsp(diep,f->next_track,f->next_sector,0);
      f->cht = f->next_track;
      f->chs = f->next_sector;
      di_fread(&f->next_track ,1,1,diep->Ip);
      di_fread(&f->next_sector,1,1,diep->Ip);
      // log_debug("this block: (%d/%d)\n",f->cht,f->chs);
      // log_debug("next block: (%d/%d)\n",f->next_track,f->next_sector);
   }
   di_fread(retbuf,1,1,diep->Ip);
   ++f->chp;
   // log_debug("di_read_byte %2.2x\n",(uint8_t)*retbuf);
   if (f->next_track == 0 && f->chp+1 >= f->next_sector) return READFLAG_EOF;
   return 0;
}

// *************
// di_write_byte
// *************

static int di_write_byte(di_endpoint_t *diep, File *f, uint8_t ch)
{
   int oldpos;
   int block;
   uint8_t t = 0, s = 0;
   uint8_t zero = 0;
   // log_debug("di_write_byte %2.2x\n",ch);
   if (f->chp > 253)
   {
    	if (f->access_mode == FS_OPEN_RW) {
		// to make sure we're not in the middle of a file
		// check the link track number
     		di_fseek_tsp(diep,f->cht,f->chs,0);
     		di_fread(&t,1,1,diep->Ip);
     		di_fread(&s,1,1,diep->Ip);
	}
	if (t == 0) {
		// only update link chain if we're not in the middle of a file
      f->chp = 0;
      oldpos = 256 * diep->DI.LBA(f->cht,f->chs);
      block = di_find_free_block(diep,f);
      if (block < 0) return CBM_ERROR_DISK_FULL;
      di_fseek_pos(diep,oldpos);
      di_fwrite(&f->cht,1,1,diep->Ip);
      di_fwrite(&f->chs,1,1,diep->Ip);
      di_fseek_tsp(diep,f->cht,f->chs,0);
      di_fwrite(&zero,1,1,diep->Ip); // new link track
      di_fwrite(&zero,1,1,diep->Ip); // new link sector
      ++f->Slot.size; // increment filesize
      // log_debug("next block: (%d/%d)\n",f->cht,f->chs);
	} else {
		// position at next (existing) block
      		f->chp = 0;
		f->cht = t;
		f->chs = s;
      		di_fseek_tsp(diep,t,s,2);
	}
   }
   di_fwrite(&ch,1,1,diep->Ip);
   ++f->chp;
   return CBM_ERROR_OK;
}

// ***********
// di_read_seq
// ***********

static int di_read_seq(di_endpoint_t *diep, File *file, char *retbuf, int len, int *eof)
{
   int i;
   log_debug("di_read_seq(fp %d, len=%d)\n",file,len);

   // we need to seek before the actual read, to make sure a paralle access does not
   // disturb the position. Only at the first byte of the file, cht/chs is invalid...
   if (file->chp < 254) {
   	di_fseek_tsp(diep,file->cht,file->chs,2+file->chp);
   }

   for (i=0 ; i < len ; ++i)
   {
      *eof = di_read_byte(diep, file, retbuf+i);
      if (*eof) return i+1;
   }
   return len;
}

// ************
// di_writefile
// ************

static int di_writefile(file_t *fp, char *buf, int len, int is_eof)
{
   int i;
   int err;
   di_endpoint_t *diep = (di_endpoint_t*) fp->endpoint;
   File *file = (File*) fp;

   if (diep->U2_track) // fill block for U2 command
   {
      di_write_block(diep,buf,len);
      if (is_eof) di_save_buffer(diep);
      return CBM_ERROR_OK;
   }

   log_debug("write to file %p, lastpos=%d\n", file, file->lastpos);

   if (file->lastpos > 0) {
	err = di_expand_rel(diep, file, file->lastpos - 1);
	if (err != CBM_ERROR_OK) {
		return -err;
	}
	di_position(diep, file, file->lastpos - 1);
   }
   di_fseek_tsp(diep,file->cht,file->chs,2+file->chp);
   for (i=0 ; i < len ; ++i) {
      if (di_write_byte(diep, file, (uint8_t)buf[i])) {
	return -CBM_ERROR_DISK_FULL;
      }
   }

   return CBM_ERROR_OK;
}

// *************
// di_block_free
// *************

static int di_block_free(di_endpoint_t *diep, uint8_t Track, uint8_t Sector)
{
   uint8_t *fbl;            // pointer to track free blocks
   uint8_t *bam;            // pointer to track BAM

   log_debug("di_block_free(%d,%d)\n",Track,Sector);

   di_calculate_BAM(diep, Track, &bam, &fbl);

   if (!(bam[Sector>>3] & (1 << (Sector & 7))))   // allocated ?
   {
      ++fbl[0];      // increase # of free blocks on track
      bam[Sector>>3] |= (1 << (Sector & 7)); // mark as free (1)

      di_sync_BAM(diep);

      return CBM_ERROR_OK;
   }

   return CBM_ERROR_OK;
}

// **************
// di_delete_file
// **************

static void di_delete_file(di_endpoint_t *diep, slot_t *slot)
{
   uint8_t t,s;
   log_debug("di_delete_file #%d <%s>\n",slot->number,slot->filename);
   
   slot->type = 0;          // mark as deleted
   di_write_slot(diep,slot);
   t = slot->start_track ;
   s = slot->start_sector;
   while (t)               // follow chain for freeing blocks
   {
      di_block_free(diep,t,s);
      di_fseek_tsp(diep,t,s,0);
      di_fread(&t,1,1,diep->Ip);
      di_fread(&s,1,1,diep->Ip);
   }
   // di_print_block(diep,slot->pos & 0xffff00);
}

// **********
// di_scratch
// **********

static int di_scratch(endpoint_t *ep, char *buf, int *outdeleted)
{
   di_endpoint_t *diep = (di_endpoint_t*) ep;
   slot_t slot;
   int found;
   int l = strlen(buf);
   if (l && buf[l-1] == 13) buf[l-1] = 0; // remove CR
   log_debug("di_scratch(%s)\n",buf);

   *outdeleted = 0;
   di_first_slot(diep,&slot);
   do
   {
      if ((found = di_match_slot(diep,&slot,(uint8_t *)buf, FS_DIR_TYPE_UNKNOWN)))
      {
         di_delete_file(diep,&slot);
         ++(*outdeleted);
      }
   }  while (found && di_next_slot(diep,&slot));
   di_sync_BAM(diep);
   return CBM_ERROR_SCRATCHED;  // FILES SCRATCHED message
}

// *********
// di_rename
// *********

static int di_rename(endpoint_t *ep, char *nameto, char *namefrom)
{
   int n;
   di_endpoint_t *diep = (di_endpoint_t*) ep;
   slot_t slot;
   int found;
   int l = strlen(namefrom);
   if (l && namefrom[l-1] == 13) namefrom[l-1] = 0; // remove CR
   log_debug("di_rename (%s) to (%s)\n",namefrom,nameto);

   // check if target exists

   di_first_slot(diep,&slot);
   if ((found = di_match_slot(diep,&slot,(uint8_t *)nameto, FS_DIR_TYPE_UNKNOWN)))
   {
      return CBM_ERROR_FILE_EXISTS;
   }

   di_first_slot(diep,&slot);
   if ((found = di_match_slot(diep,&slot,(uint8_t *)namefrom, FS_DIR_TYPE_UNKNOWN)))
   {
      n = strlen(nameto);
      if (n > 16) n = 16;
      memset(slot.filename,0xA0,16); // fill filename with $A0
      memcpy(slot.filename,nameto,n);
      di_write_slot(diep,&slot);
      return CBM_ERROR_OK;
   }
   return CBM_ERROR_FILE_NOT_FOUND;
}

// *****
// di_cd
// *****

static int di_cd(endpoint_t *ep, char *buf)
{
   log_debug("di_cd %p %s\n",ep,buf);
   return CBM_ERROR_OK;
}


//***********
// di_rel_record_max
//***********
//
// find the record_max value to know when we have to expand a REL file
// Called from di_open, so it has the correct value
// modelled after VICE's vdrive_rel_record_max

static unsigned int di_rel_record_max(di_endpoint_t *diep, File *f) {

   	uint8_t sidesector[256];

	unsigned int super, side;
   	unsigned int j, k, l, o;

   	// find the number of side sector groups
  
   	// read first side sector
	uint8_t ss_track = f->Slot.ss_track;
	uint8_t ss_sector = f->Slot.ss_sector;

	if (ss_track == 0) {
		// not a REL file or a new one
		return 0;
	}
	// read the first side sector
      	di_fseek_tsp(diep,ss_track,ss_sector,0);
      	di_fread(sidesector,1,256,diep->Ip);
 
	super = 0;
	// number of side sector groups
	if (sidesector[SSB_OFFSET_SUPER_254] == 0xfe) {
		// sector is a super side sector
		// count how many side sector groups are used -> side
		o = SSS_OFFSET_SSB_POINTER;
		for (super = 0; super < SSS_INDEX_SSB_MAX && sidesector[o] != 0; super++, o+=2);

		if (super == 0) {
			return 0;
		}
		super--;

		// last side sector group 
		ss_track = sidesector[SSS_OFFSET_SSB_POINTER + (super << 1)];
		ss_sector = sidesector[SSS_OFFSET_SSB_POINTER + (super << 1) + 1];
	
	      	di_fseek_tsp(diep,ss_track,ss_sector,0);
	      	di_fread(sidesector,1,256,diep->Ip);
	}

	// now sidesector contains the first block of the last side sector group
	// find last sector in side sector group (guaranteed to find)
	o = SSB_OFFSET_SSG;
	for (side = 0; side < SSG_SIDE_SECTORS_MAX && sidesector[o] != 0; side++, o+=2);
	
	side--;
	if (side > 0) {	
		// not the first one (which is already in the buffer)
		ss_track = sidesector[SSB_OFFSET_SSG + (side << 1)];
		ss_sector = sidesector[SSB_OFFSET_SSG + (side << 1) + 1];
	
	      	di_fseek_tsp(diep,ss_track,ss_sector,0);
	      	di_fread(sidesector,1,256,diep->Ip);
	}

	// here we have the last side sector of the last side sector chain in the buffer.
    	// obtain the last byte of the sector according to the index 
    	j = ( sidesector[ BLK_OFFSET_NEXT_SECTOR ] + 1 - SSB_OFFSET_SECTOR ) / 2;
	// now get the track and sector of the last block
    	j--;
	o = SSB_OFFSET_SECTOR + 2 * j;
    	ss_track = sidesector[o];
    	ss_sector = sidesector[o + 1];

	// read the last sector of the file
      	di_fseek_tsp(diep,ss_track,ss_sector,0);
      	di_fread(sidesector,1,256,diep->Ip);

	// number of bytes in this last sector
	o = sidesector[BLK_OFFSET_NEXT_SECTOR] - 1;

    	/* calculate the total bytes based on the number of super side, side
           sectors, and last byte index */
	k = super * SSG_SIDE_SECTORS_MAX + side;	// side sector
	k *= SSB_INDEX_SECTOR_MAX;			// times numbers of sectors per side sector
	k += j;					// plus sector in the side sector
	k *= 254;				// times bytes per sector
	k += o;					// plus bytes in last sector

    	/* divide by the record length, and get the maximum records */
    	l = k / f->Slot.recordlen;

    	return l;
}

// flush all dirty side sectors in the given group
static void di_flush_sidesectors(di_endpoint_t *diep, uint8_t *sidesectorgroup, 
		uint8_t *ssg_track, uint8_t *ssg_sector, uint8_t *ssg_dirty) {

	for (int i = 0; i < SSG_SIDE_SECTORS_MAX; i++) {
		if (ssg_dirty[i]) {
			di_fseek_tsp(diep, ssg_track[i], ssg_sector[i], 0);
			di_fwrite(sidesectorgroup + (i*256), 1, 256, diep->Ip);
			ssg_dirty[i] = 0;
		}
	}
}

// flush dirty super side sectors in the given group
static void di_flush_supersector(di_endpoint_t *diep, uint8_t *supersector, 
		uint8_t sss_track, uint8_t sss_sector, uint8_t sss_dirty) {

	if (sss_dirty) {
		di_fseek_tsp(diep, sss_track, sss_sector, 0);
		di_fwrite(supersector, 1, 256, diep->Ip);
	}
}

// read side sector blocks 2 to 6 (as given with the first side sector that is not read)
static void di_read_sidesector_group(di_endpoint_t *diep, uint8_t *sidesectorgroup, 
		uint8_t *ssg_track, uint8_t *ssg_sector, uint8_t *ssg_dirty) {

	uint8_t track, sector;

	for (int i = 1; i < SSG_SIDE_SECTORS_MAX; i++) {
		track = sidesectorgroup[SSB_OFFSET_SSG + (i << 1)];
		sector = sidesectorgroup[SSB_OFFSET_SSG + (i << 1) + 1];
		// read sectors, but keep if already in buffer (to not overwrite dirty ones)
		if (track > 0 && (track != ssg_track[i] || sector != ssg_sector[i])) {
			di_fseek_tsp(diep, track, sector, 0);
			di_fread(sidesectorgroup + (i*256), 1, 256, diep->Ip);
			ssg_dirty[i] = 0;
		}
		ssg_track[i] = track;
		ssg_sector[i] = sector;
	}
}


//***********
// di_rel_add_sectors
//***********
//
// add one or more sectors (up to the given nrecords) to an existing 
// REL file. Updates the BAM while allocating.
// Also updates f->maxrecord
// TODO: currently only does one block (nrecords is not used), but this is lets 
// it traverse the whole side sector stuff for each sector, so nrecords will be
// used in a later rev to do multiple blocks in one call
//
// Note: loosely based on VICE's vdrive_rel_add_sector()
//
// Note: Disk full errors are not handled gracefully.
// May or may not leave residue in form of unlinked, but marked allocated sectors
//
int di_rel_add_sectors(di_endpoint_t *diep, File *f, unsigned int nrecords) {

	unsigned int i;

	log_debug("di_rel_add_sectors f=%p to nrecords=%d\n", f, nrecords);

	// find the total number of blocks this file uses
	i = f->Slot.size;

	// compare with maximum for disk image
	if (i >= diep->DI.RelBlocks) {
		return CBM_ERROR_TOO_LARGE;
	}
	
   	uint8_t supersector[256];
   	uint8_t sidesectorgroup[256 * SSG_SIDE_SECTORS_MAX];
   	uint8_t datasector[256];
	uint8_t sss_track, sss_sector, sss_dirty;
	uint8_t ssg_track[SSG_SIDE_SECTORS_MAX], ssg_sector[SSG_SIDE_SECTORS_MAX], ssg_dirty[SSG_SIDE_SECTORS_MAX];
	uint8_t slot_dirty;
	uint8_t last_track, last_sector;	// T/S of last REL block
	uint8_t new_track, new_sector;	// T/S of last REL block

	unsigned int super, side;
   	unsigned int j, k, o;

	slot_dirty = 0;
	sss_track = 0;
	sss_dirty = 0;
	for (i = 0; i < SSG_SIDE_SECTORS_MAX; i++) {
		ssg_track[i] = 0;
		ssg_dirty[i] = 0;
	}

	// allocate new data block
	// we're going to do that anyway, and the original DOS allocates it before the
	// side sector, so we do as well.
	if (di_find_free_block(diep, f) < 0) {
		return CBM_ERROR_DISK_FULL;
	}
	f->Slot.size++;
	slot_dirty = 1;

	new_track = f->cht;
	new_sector = f->chs;

	// file does not yet exist
	if (f->Slot.start_track == 0) {
		f->Slot.start_track = new_track;
		f->Slot.start_sector = new_sector;
		slot_dirty = 1;
	}

	// -----------------------------------------------
   	// find the first non-empty side sector group
  
   	// read first side sector
	uint8_t track = f->Slot.ss_track;
	uint8_t sector = f->Slot.ss_sector;

	// do we have a super side sector?
	if (diep->DI.HasSSB) {
		if (track == 0) {
			// create super side sector block
			if (di_find_free_block(diep, f) < 0) {
				return CBM_ERROR_DISK_FULL;
			}
			f->Slot.size++;
			slot_dirty = 1;

			sss_track = f->cht;
			sss_sector = f->chs;

			memset(supersector, 0, 256);
			supersector[SSB_OFFSET_SUPER_254] = 254;
			// fill in t/s of first side sector block at offset 0/1 and 3/4 later
			sss_dirty = 1;

			f->Slot.ss_track = f->cht;
			f->Slot.ss_sector = f->chs;
			slot_dirty = 1;
		} else {
			sss_track = track;
			sss_sector = sector;
			// read the super side sector
			di_fseek_tsp(diep, track, sector, 0);
			di_fread(supersector, 1, 256, diep->Ip);
		}	

		// let's find the last side sector group and check if it's empty
					
		o = SSS_OFFSET_SSB_POINTER;
		for (super = 0; super < SSS_INDEX_SSB_MAX && supersector[o] != 0; super++, o+=2);

	}

	// sss and no group in sss found, or non-sss and no side sector in slot
	// note: we do as if we always have a sss, because on write we ignore it when
	// non-sss image
	if ((diep->DI.HasSSB && super == 0) || ((!diep->DI.HasSSB) && track == 0)) {

		log_debug(" - creating the first side sector group for file %p\n", f);

		// no side sector group so far, create the first one
		// create super side sector block
		if (di_find_free_block(diep, f) < 0) {
			return CBM_ERROR_DISK_FULL;
		}
		f->Slot.size++;
		slot_dirty = 1;

		ssg_track[0] = f->cht;
		ssg_sector[0] = f->chs;

		memset(sidesectorgroup, 0, 256);
		sidesectorgroup[BLK_OFFSET_NEXT_SECTOR] = SSB_OFFSET_SECTOR - 1;	// no pointer in file
		sidesectorgroup[SSB_OFFSET_RECORD_LEN] = f->Slot.recordlen;
		// first entry in side sector addresses list in first side sector points to itself
		sidesectorgroup[SSB_OFFSET_SSG] = ssg_track[0];
		sidesectorgroup[SSB_OFFSET_SSG + 1] = ssg_sector[0];

		// update supersector
		supersector[BLK_OFFSET_NEXT_TRACK] = ssg_track[0];
		supersector[BLK_OFFSET_NEXT_SECTOR] = ssg_sector[0];
		supersector[SSS_OFFSET_SSB_POINTER] = ssg_track[0];
		supersector[SSS_OFFSET_SSB_POINTER + 1] = ssg_sector[0];

		sss_dirty = 1;
		ssg_dirty[0] = 1;

		if (!diep->DI.HasSSB) {
			// if not super side sector, update dir entry with address of 
			// this first side sector
			f->Slot.ss_track = f->cht;
			f->Slot.ss_sector = f->chs;
			slot_dirty = 1;
		}
	} else {
		if (diep->DI.HasSSB) {
			// if there is a super side sector, read address of side sector group
			// from it (else track/sector already contains the side sector group address)
			super--;
			
			track = supersector[SSS_OFFSET_SSB_POINTER + (super << 1)];
			sector = supersector[SSS_OFFSET_SSB_POINTER + (super << 1) + 1];
		}

		ssg_track[0] = track;
		ssg_sector[0] = sector;

		// read the side sector
		di_fseek_tsp(diep, track, sector, 0);
		di_fread(sidesectorgroup, 1, 256, diep->Ip);
	}

	log_debug(" - ssg[0] is t/s: %d/%d\n", ssg_track[0], ssg_sector[0]);

	// now sidesectorgroup contains the first block of the last side sector group
	// find last sector in side sector group (guaranteed to find)
	o = SSB_OFFSET_SSG;
	for (side = 0; (side < SSG_SIDE_SECTORS_MAX) && (sidesectorgroup[o] != 0); side++, o+=2);
	
	log_debug(" - last side sector is %d\n", side);

	side--;
	if (side > 0) {	
		// not the first one (which is already in the buffer)
		track = sidesectorgroup[SSB_OFFSET_SSG + (side << 1)];
		sector = sidesectorgroup[SSB_OFFSET_SSG + (side << 1) + 1];
	
	      	di_fseek_tsp(diep,track,sector,0);
	      	di_fread(sidesectorgroup + (side * 256),1,256,diep->Ip);

		ssg_track[side] = track;
		ssg_sector[side] = sector;
	}

	// here we have the last side sector of the last side sector group in the buffer.
    	// Obtain the number of last used entry in the sector table according to the 
	// last valid byte index in the next sector field at offset 1
    	j = ( sidesectorgroup[ (side * 256) + BLK_OFFSET_NEXT_SECTOR ] + 1 - SSB_OFFSET_SECTOR ) / 2;

	log_debug(" - last entry in side sector is %d\n", j);

	if (j > 0) {
		// now get the track and sector of the last block
		o = (side * 256) + SSB_OFFSET_SECTOR + 2 * (j-1);
    		last_track = sidesectorgroup[o];
    		last_sector = sidesectorgroup[o + 1];
	} else {
		last_track = 0;
		last_sector = 0;
	}

	log_debug(" - last file sector is %d/%d\n", last_track, last_sector);

	if (last_track > 0) {
		// read the last sector of the file
	      	di_fseek_tsp(diep,last_track,last_sector,0);
	      	di_fread(datasector,1,256,diep->Ip);
	}

    	// Check if this side sector is full, allocate a new one if necessary
	// then update side sector with new data block
	//
	// note: the side sector groups actually form a continous file
	// comprising of all side sectors. So if a new side sector group
	// is needed, the last sector in the previous side sector group must
	// get the correct block link as well.
	if ( j == SSB_INDEX_SECTOR_MAX) {

		log_debug(" - allocate new side sector block\n");

		// allocate a new block for a side sector
		if (di_find_free_block(diep, f) < 0) {
			return CBM_ERROR_DISK_FULL;
		}
		f->Slot.size++;
		slot_dirty = 1;

		track = f->cht;
		sector = f->chs;

		// is a new side sector group needed?
		// i.e. is this the last side sector in a group (and it's full)?
		if (side == SSG_SIDE_SECTORS_MAX - 1) {
			// yes, create a new group
			
			log_debug(" - need to start a new side sector group\n");

			// update pointers in last block of preceeding side sector group
			sidesectorgroup[side*256 + BLK_OFFSET_NEXT_TRACK] = track;
			sidesectorgroup[side*256 + BLK_OFFSET_NEXT_SECTOR] = sector;
			ssg_dirty[side] = 1;

			di_flush_sidesectors(diep, sidesectorgroup, ssg_track, ssg_sector, ssg_dirty);

			ssg_track[0] = track;
			ssg_sector[0] = sector;
			ssg_dirty[0] = 1;

			memset(sidesectorgroup, 0, 256);
			sidesectorgroup[SSB_OFFSET_SECTOR_NUM] = 0;
			sidesectorgroup[SSB_OFFSET_SSG] = track;
			sidesectorgroup[SSB_OFFSET_SSG + 1] = sector;

			// update super side sector
			super++;
			if (super >= SSS_INDEX_SSB_MAX) {
				return CBM_ERROR_DISK_FULL;
			}
			o = SSS_OFFSET_SSB_POINTER + super * 2;
			supersector[o] = track;
			supersector[o + 1] = sector;
			sss_dirty = 1;

			// setup reference for the first side sector
			o = 0;
		} else {
			// no, sector group is not full, update old group
			// "side" contains the number of the new side sector in this group
			side++;
	
			di_read_sidesector_group(diep, sidesectorgroup, ssg_track, ssg_sector, ssg_dirty);

			// update current side sector indices
			for (k = 0; k < SSG_SIDE_SECTORS_MAX; k++) {
				if (ssg_track[k]) {
					sidesectorgroup[(k * 256) + SSB_OFFSET_SSG + (side << 1)] = track;
					sidesectorgroup[(k * 256) + SSB_OFFSET_SSG + (side << 1) + 1] = sector;
					ssg_dirty[k] = 1;
				}
			}

			// update pointers in last block
			sidesectorgroup[(side-1)*256 + BLK_OFFSET_NEXT_TRACK] = track;
			sidesectorgroup[(side-1)*256 + BLK_OFFSET_NEXT_SECTOR] = sector;
			ssg_dirty[side-1] = 1;

			// update new sector
			sidesectorgroup[(side*256) + SSB_OFFSET_SECTOR_NUM] = side;
			// copy side sector track and sector list from first side sector
			for (k = 0; k < SSG_SIDE_SECTORS_MAX * 2; k++) {
				sidesectorgroup[(side*256) + SSB_OFFSET_SSG + k] =
					sidesectorgroup[SSB_OFFSET_SSG + k];
			}
			// setup reference to side sector
			o = (side * 256);

			// set dirty
			ssg_dirty[side] = 1;
			ssg_track[side] = track;
			ssg_sector[side] = sector;
		}

		// update side sector contents
		sidesectorgroup[o + BLK_OFFSET_NEXT_TRACK] = 0;
		sidesectorgroup[o + BLK_OFFSET_NEXT_SECTOR] = SSB_OFFSET_SECTOR + 1;
		sidesectorgroup[o + SSB_OFFSET_RECORD_LEN] = f->Slot.recordlen;
		sidesectorgroup[o + SSB_OFFSET_SECTOR] = new_track;
		sidesectorgroup[o + SSB_OFFSET_SECTOR + 1] = new_sector;
	} else {
		// last side sector is not full
		// just update with new data
	
		o = (side * 256);	
		// track and sector of data block
		sidesectorgroup[o + SSB_OFFSET_SECTOR + 2 * j] = new_track;
		sidesectorgroup[o + SSB_OFFSET_SECTOR + 2 * j + 1] = new_sector;
		
		sidesectorgroup[o + BLK_OFFSET_NEXT_SECTOR] = SSB_OFFSET_SECTOR + 2 * j + 1;

		ssg_dirty[side] = 1;
	}

	// flush metadata
	di_flush_sidesectors(diep, sidesectorgroup, ssg_track, ssg_sector, ssg_dirty);
	if (diep->DI.HasSSB) {
		di_flush_supersector(diep, supersector, sss_track, sss_sector, sss_dirty);
	}
	if (slot_dirty) {
		di_write_slot(diep, &f->Slot);
	}

	// now fill up the last data block in datasector / last_track / last_sector
	// Note: as I'm not using Position to seek the record, but traverse the 
	// side sectors, I know that I really am in the last sector
	
	k = 0;

	// fill up current last data sector 
	if (last_track > 0) {
		// start of next sector
		o = datasector[BLK_OFFSET_NEXT_SECTOR] + 1;
		// file link
		datasector[BLK_OFFSET_NEXT_TRACK] = new_track;
		datasector[BLK_OFFSET_NEXT_SECTOR] = new_sector;

        	/* Fill the new records up with the default 0xff 0x00 ... */
        	while (o < 256)
        	{
            		if (k==0) datasector[o] = 0xff;
            		else datasector[o] = 0x00;
            		k = ( k + 1 ) % f->Slot.recordlen;
            		/* increment the maximum records each time we complete a full
            		    record. */
            		if (k==0) f->maxrecord++;
            		o++;
        	}

		log_debug("writing back previous last file sector to %d/%d with link to %d/%d\n",
			last_track, last_sector, new_track, new_sector);

		// write back data sector
		di_fseek_tsp(diep, last_track, last_sector, 0);
		di_fwrite(datasector, 1, 256, diep->Ip);
	}

    	/* Fill new sector with maximum records */
    	o = 2;
    	while (o < 256)
    	{
        	if (k==0) datasector[o] = 0xff;
        	else datasector[o] = 0x00;
        	k = ( k + 1 ) % f->Slot.recordlen;
        	/* increment the maximum records each time we complete a full
            	   record. */
	        if (k==0) f->maxrecord++;
        	o++;
    	}

    	/* set as last sector in REL file */
    	datasector[BLK_OFFSET_NEXT_TRACK] = 0;

	log_debug("set last sector (%d/%d) size to %d\n", new_track, new_sector, 255-k);

    	/* Update the last byte based on how much of the last record we
           filled. */
    	datasector[BLK_OFFSET_NEXT_SECTOR] = 255 - k;

	// write back data sector
	di_fseek_tsp(diep, new_track, new_sector, 0);
	di_fwrite(datasector, 1, 256, diep->Ip);
	
	return CBM_ERROR_OK;	
}

//***********
// di_expand_rel
//***********
//
// internal function to expand a REL file to the given record (so that the record given exists)
// algorithm partly taken from the VICE vdrive_rel_grow() function.
// The algorithm adds a sector to the rel file until we meet the required records

static int di_expand_rel(di_endpoint_t *diep, File *f, int recordno) {

	log_debug("di_expand_rel f=%p to recordno=%d\n", f, recordno);

	int err = CBM_ERROR_OK;

	// recordno starts at 0, maxrecord == 0 indicates new file, therefore also add on equal
	while (err == CBM_ERROR_OK && recordno >= f->maxrecord) {
		// each step adds one (or more) sectors to the REL file
		err = di_rel_add_sectors(diep, f, recordno - f->maxrecord);
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

static int di_position(di_endpoint_t *diep, File *f, int recordno) {

   uint8_t sidesector[256];
   uint8_t reclen = 0;
 
   unsigned int rec_long;	// absolute offset in file
   unsigned int rec_start;	// start of record in block
   unsigned int super, side;	// super side sector index, side sector index
   unsigned int offset;		// temp for the number of file bytes pointed to by blocks in a sss or ss

   uint8_t next_track, next_sector;


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

	// -----------------------------------------
	// find the position pointed to in the image
	uint8_t ss_track = f->Slot.ss_track;
	uint8_t ss_sector = f->Slot.ss_sector;

	if (ss_track == 0) {
		// not a REL file
		return CBM_ERROR_FILE_TYPE_MISMATCH;
	}
	// read the first side sector
      	di_fseek_tsp(diep,ss_track,ss_sector,0);
      	di_fread(sidesector,1,256,diep->Ip);
 	
	if (sidesector[SSB_OFFSET_SUPER_254] == 0xfe) {
		// sector is a super side sector
		// read the address of the first block of the correct side sector chain
		ss_track = sidesector[SSS_OFFSET_SSB_POINTER + (super << 1)];
		ss_sector = sidesector[SSS_OFFSET_SSB_POINTER + 1 + (super << 1)];
	
		if (ss_track == 0) {
			return CBM_ERROR_RECORD_NOT_PRESENT;
		}
	      	di_fseek_tsp(diep,ss_track,ss_sector,0);
	      	di_fread(sidesector,1,256,diep->Ip);
 	} else {
		// no super side sectors, but sector number too large for single side sector chain
		if (super > 0) {
			return CBM_ERROR_RECORD_NOT_PRESENT;
		}
	}
	// here we have the first side sector of the correct side sector chain in the buffer.
	if (side > 0) {
		// need to read the correct side sector first
		// read side sector number
		ss_track = sidesector[SSB_OFFSET_SSG + (side << 1)];
		ss_sector = sidesector[SSB_OFFSET_SSG + 1 + (side << 1)];
		if (ss_track == 0) {
			// sector in a part of the side sector group that isn't created yet
			return CBM_ERROR_RECORD_NOT_PRESENT;
		}
	      	di_fseek_tsp(diep,ss_track,ss_sector,0);
	      	di_fread(sidesector,1,256,diep->Ip);
	}
	// here we have the correct side sector in the buffer.
	ss_track = sidesector[SSB_OFFSET_SECTOR + (offset << 1)];
	ss_sector = sidesector[SSB_OFFSET_SECTOR + 1 + (offset << 1)];
	if (ss_track == 0) {
		// sector in a part of the side sector that is not yet created
		return CBM_ERROR_RECORD_NOT_PRESENT;
	}

	
	// here we have, in ss_track, ss_sector, and rec_start the tsp position of the
	// record as given in the parameter
	di_fseek_tsp(diep, ss_track, ss_sector, 0);
      	di_fread(&next_track ,1,1,diep->Ip);
      	di_fread(&next_sector,1,1,diep->Ip);

	// is there enough space on the last block?
	if (next_track == 0 && next_sector < (rec_start + reclen + 1)) {
		return CBM_ERROR_RECORD_NOT_PRESENT;
	}

	f->cht = ss_track;
	f->chs = ss_sector;
	f->chp = rec_start;
	f->next_track = next_track;
	f->next_sector = next_sector;

	// clean up the "expand me" flag
	f->lastpos = 0;

	log_debug("position -> sector %d/%d/%d\n", f->next_track, f->next_sector, rec_start);

	return CBM_ERROR_OK;

}

//***********
// di_create 
//***********

static int di_create(file_t *dirp, file_t **newfile, const char *pattern, openpars_t *pars, int type) 
{
	File *dir = (File*) dirp;
	di_endpoint_t *diep = (di_endpoint_t*) dirp->endpoint;

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

	File *file = di_reserve_file(diep);

	int rv = di_create_entry(diep, file, pattern, pars);

	di_fflush(&file->file);

	if (rv != CBM_ERROR_OK) {
		
		reg_remove(&diep->base.files, file);
		mem_free(file);
	} else {

		*newfile = file;
	}


	return rv;
}


//***********
// di_open
//***********

static int di_open(file_t *fp, int type) 
{
	openpars_t pars;
	int rv = CBM_ERROR_FAULT;

	pars.filetype = FS_DIR_TYPE_UNKNOWN;
	pars.recordlen = 0;
	
	File *file = (File*) fp;

	if (type == FS_OPEN_DR) {
		rv = di_open_dir(file);
	} else {
   		rv = di_open_file(file, &pars, type);
   		//*reclen = reclen16;
	}
   	return rv;
}

// *****************
// di_direct_channel
// *****************

static int di_direct_channel(di_endpoint_t *diep, int chan)
{
   int i;

   for (i=0 ; i < 5 ; ++i)
      if (diep->chan[i] == chan) return i;
   return -1; // no direct channel
}

// ***********
// di_readfile
// ***********

static int di_readfile(file_t *fp, char *retbuf, int len, int *eof)
{
   int rv = 0;
   di_endpoint_t *diep = (di_endpoint_t*) fp->endpoint;
   File *file = (File*) fp;
   log_debug("di_readfile(%p fp=%p len=%d\n",diep,file,len);

	if (fp->dirstate != DIRSTATE_NONE) {
		// directory
		rv = di_read_dir_entry(diep, file, retbuf, eof);
	} else
	{
/*   
   if (di_direct_channel(diep,chan) >= 0)
   {
      return di_read_block(diep, file, retbuf, len, eof);
   } else {
*/
   		rv = di_read_seq(diep, file, retbuf, len, eof);
	}
   	return rv;
}

// *******
// di_init
// *******

static void di_init(void)
{
   log_debug("di_init\n");
}

// ----------------------------------------------------------------------------------
//    Debug code

#if 0

// *************
// di_dump_block
// *************

static void di_dump_block(uint8_t *b)
{
   int i,j;
   printf("BLOCK:\n");
   for (j=0 ; j<256 ; j+=16)
   {
      for (i=0 ; i < 16; ++i) printf(" %2.2x",b[i+j]);
      printf("   ");
      for (i=0 ; i < 16; ++i)
      {
        if (b[i+j] > 31 && b[i+j] < 96) printf("%c",b[i+j]);
        else printf(".");
      }
      printf("\n");
   }
}

// *************
// di_print_slot
// *************

static void di_print_slot(slot_t *slot)
{
   printf("SLOT  %d\n",slot->number);
   printf("name  %s\n",slot->filename);
   printf("pos   %6.6x\n",slot->pos);
   printf("size  %6d\n",slot->size);
   printf("type  %6d\n",slot->type);
   printf("eod   %6d\n",slot->eod );
   printf("next  (%d/%d)\n",slot->next_track,slot->next_sector);
   printf("start (%d/%d)\n",slot->start_track,slot->start_sector);
}

// **************
// di_print_block
// **************

static void di_print_block(di_endpoint_t *diep, int pos)
{
   uint8_t b[16];
   int i,j;
   di_fseek_pos(diep,pos);
   printf("BLOCK: %x\n",pos);
   for (j=0 ; j<256 ; j+=16)
   {
      fread(b,1,16,diep->Ip);
      for (i=0 ; i < 16; ++i) printf(" %2.2x",b[i]);
      printf("   ");
      for (i=0 ; i < 16; ++i)
      {
        if (b[i] > 31 && b[i] < 96) printf("%c",b[i]);
        else printf(".");
      }
      printf("\n");
   }
}

#endif

static void di_close(file_t *fp, int recurse) {
	File *file = (File*) fp;
	di_endpoint_t *diep = (di_endpoint_t*) fp->endpoint;

	di_close_fd(diep, file);

	reg_remove(&diep->base.files, file);

	if (diep->base.is_temporary && reg_size(&diep->base.files) == 0) {
		di_freeep(fp->endpoint);
	}

	// mem_free(file)?
}


// ----------------------------------------------------------------------------------

/*
provider_t di_provider =
{
  "di",
  "PETSCII",
   di_init,         // void        (*init     )(void);
   di_newep,        // endpoint_t* (*newep    )(endpoint_t *parent, ...
   di_tempep,       // endpoint_t* (*tempep   )(char **par); 
   di_freeep,       // void        (*freeep   )(endpoint_t *ep); 
   NULL,	    // file_t*	   (*wrap     )(file_t *infile);
   di_close,        // void        (*close    )(endpoint_t *ep, int chan);
   di_open,         // int         (*open_rd  )(endpoint_t *ep, int chan, ...
   di_opendir,      // int         (*opendir  )(endpoint_t *ep, int chan, ...
   di_readfile,     // int         (*readfile )(endpoint_t *ep, int chan, ...
   di_writefile,    // int         (*writefile)(endpoint_t *ep, int chan, ...
   di_scratch,      // int         (*scratch  )(endpoint_t *ep, char *name, ...
   di_rename,       // int         (*rename   )(endpoint_t *ep, char *nameto, ...
   di_cd,           // int         (*cd       )(endpoint_t *ep, char *name); 
   NULL,            // int         (*mkdir    )(endpoint_t *ep, char *name); 
   NULL,            // int         (*rmdir    )(endpoint_t *ep, char *name);
   di_position,     // int         (*position )(endpoint_t *ep, int chan, int recordno);
   di_direct        // int         (*direct   )(endpoint_t *ep, char *buf, ...
};
*/

// ----------------------------------------------------------------------------------

handler_t di_file_handler = {
        "fs_file_handler",
        NULL,		// charset name
        NULL,   	// resolve - not required
        di_close,       // close
        di_open,        // open a file_t
	handler_parent,	// default parent() impl
        NULL,	//	dif_seek,               // seek
        di_readfile,            // readfile
        di_writefile,           // writefile
        NULL,                   // truncate
        di_direntry,            // direntry
        di_create               // create
};

provider_t di_provider = {
        "fs",
        "PETSCII",
        di_init,
        di_newep,
        di_tempep,
        di_freeep,
        di_root,        // file_t* (*root)(endpoint_t *ep);  // root directory for the endpoint
        di_wrap,        // wrap while CDing into D64 file
        di_scratch,
        di_rename,
        di_cd,
        NULL,		// mkdir not supported
        NULL,		// rmdir not supported
        di_direct
};

