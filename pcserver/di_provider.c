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

#include "dir.h"
#include "fscmd.h"
#include "provider.h"
#include "errors.h"
#include "mem.h"
#include "wireformat.h"
#include "channel.h"
#include "byte.h"
#include "wildcard.h"

#include "log.h"

#undef DEBUG_READ
#define DEBUG_CMD

#ifndef BYTE
#define BYTE unsigned char
#endif

#define  MAX_BUFFER_SIZE  64

typedef struct Disk_Image
{
   BYTE ID;                    // Image type (64,71,80,81,82)
   BYTE Tracks;                // Tracks per side
   BYTE Sectors;               // Max. sectors per track
   BYTE Sides;                 // Sides (2 for D71 and D82)
   BYTE BAMBlocks;             // Number of BAM blocks
   BYTE BAMOffset;             // Start of BAM in block
   BYTE TracksPerBAM;          // Tracks per BAM
   BYTE DirInterleave;         // Interleave on directory track
   BYTE DatInterleave;         // Interleave on data tracks
   int  Blocks;                // Size in blocks
   int (*LBA)(int t, int s);   // Logical Block Address calculation
   BYTE DirTrack;             // Header and directory track
} Disk_Image_t;

/* functions for computing LBA (Logical Block Address) */

int LBA64(int t, int s)
{
   return s + 21*(t-1) - 2*(t>18)*(t-18) - (t>25)*(t-25) - (t>31)*(t-31);
}

int LBA71(int t, int s)
{
   if (t < 36) return LBA64(t,s);
   return    683 + LBA64(t-35,s);
}

int LBA80(int t, int s)
{
   return s + 29*(t-1) - 2*((t>40)*(t-40) + (t>54)*(t-54) + (t>65)*(t-65));
}

int LBA82(int t, int s)
{
   if (t < 78) return LBA80(t,s);
   return   2083 + LBA80(t-77,s);
}

int LBA81(int t, int s)
{
   return s + (t-1) * 40;
}

//                  ID Tr Se S B Of TB D I Blck      Dir
Disk_Image_t d64 = {64,35,21,1,1, 4,35,3,9, 683,LBA64,18};
Disk_Image_t d80 = {80,70,29,1,2, 6,50,3,5,2083,LBA80,39};
Disk_Image_t d81 = {81,80,40,1,2,16,40,1,1,3200,LBA81,40};

/* Commodore Floppy Formats

     D64 / D71                  D80 / D82                      D81

S  Track  #S  Bl.          S  Track   #S   Bl.          S  Track  #S   Bl.
-----------------------    -------------------          ------------------
0   1-17  21  357          0   1- 39  29  1131          0   1-40  40  1600
0  18-24  19  133          0  40- 53  27   378
0  25-30  18  108          0  54- 64  25   275
0  31-35  17   85   683    0  65- 77  23   299  2083                
-----------------------    -------------------------    ------------------
1  36-52  21  357          1  78-116  29  1131          1  41-80  40  1600
1  53-59  19  133          1 117-130  27   378
1  60-65  18  108          1 131-141  25   275
1  66-70  17   85  1366    1 142-154  23   299  4166                  3200
=======================    =========================   ===================


BAM Block for D64 & D71 (track 18 / sector 0)

---+----+----+----+----+--------------------------------------
00 : 12 | 01 | 41 | 00 | Link to directory block / DOS version
---+----+----+----+----+--------------------------------------
04 : 15 | FF | FF | 1f | track  1: free blocks and BAM
---+----+----+----+----+--------------------------------------
08 : 15 | FF | FF | 1f | track  2: free blocks and BAM
---+----+----+----+----+--------------------------------------
.. : .. | .. | .. | .. | track  n: free blocks and BAM
---+----+----+----+----+--------------------------------------
88 : 11 | FF | FF | 01 | track 34: free blocks and BAM
---+----+----+----+----+--------------------------------------
8C : 11 | FF | FF | 01 | track 35: free blocks and BAM
---+-------------------+--------------------------------------
90 : DISK NAME         | 16 bytes padded with (A0)
---+-------------------+--------------------------------------
A0 : A0 | A0 | ID | ID | Disk ID (A2/A3)
---+----+----+----+----+--------------------------------------
A4 : A0 | 32 | 41 | A0 | DOS version "2A" (A5/A6)
---+----+----+----+----+--------------------------------------
.. : A0 | A0 | A0 | A0 | unused space filled with A0
---+----+----+----+----+--------------------------------------
DC : A0 | 15 | 15 | 15 | D71: free blocks (track 36-38)
---+----+----+----+----+--------------------------------------
E0 : 15 | 15 | 15 | 15 | D71: free blocks (track 39-42)
---+----+----+----+----+--------------------------------------
.. : .. | .. | .. | .. | D71: free blocks
---+----+----+----+----+--------------------------------------
E8 : 12 | 12 | 12 | 11 | D71: free blocks (track 63-66)
---+----+----+----+----+--------------------------------------
EC : 11 | 11 | 11 | 11 | D71: free blocks (track 67-70)
--------------------------------------------------------------

CBM File types:
-------------------------------------
0  DEL  Deleted file            
1  SEQ  Sequential file
2  PRG  Program file
3  USR  User file
4  REL  Random Access (relative) file
5  CBM  D81 directory 

OR'ed with 80: CLOSED   (0: "*" in display)
OR'ed with 40: LOCKED   (1: "<" in display)

Directory block

--------------------------------------------------------------
00 : TR | SE |      chain link to next directory block         
---+----+----+----+-------------------------------------------
02 : TY | BT | BS | file type & track/sector of 1st. block
---+----+----+----+-------------------------------------------
05 : FILE NAME    | filename (16 bytes padded with A0)
---+----+----+----+-------------------------------------------
15 : ST | SS | RS | REL files: side sector t&s, record length
---+----+----+----+-------------------------------------------
18 : -- | -- | -- | unused
---+----+----+---------+--------------------------------------
1C : @T | @S |      Track/Sector for replacement file (@)
---+----+----+---------+--------------------------------------
1E : Lo | Hi |      File size in blocks of 254 bytes
--------------------------------------------------------------
20 :                2. directory entry
--------------------------------------------------------------
E0 :                8. directory entry
--------------------------------------------------------------

*/

// structure for directory slot handling

typedef struct
{
   int   number;       // current slot number
   int   pos;          // file position
   int   size;         // file size in (254 byte) blocks
   BYTE  next_track;   // next directory track
   BYTE  next_sector;  // next directory sector
   BYTE  filename[20]; // filename (C string zero terminated)
   BYTE  type;         // file type
   BYTE  start_track;  // first track
   BYTE  start_sector; // first sector
   BYTE  ss_track;     // side sector track
   BYTE  ss_sector;    // side sector sector
   BYTE  recordlen;    // REL file record length
   BYTE  eod;          // end of directory
} slot_t;

typedef struct
{
   slot_t Slot;        // 
   BYTE  *buf;         // direct channel block buffer
   int   chan;         // channel for which the File is
   BYTE  CBM_file[20]; // filename with CBM charset
   BYTE  dirpattern[MAX_BUFFER_SIZE];
   BYTE  is_first;     // is first directory entry?
   BYTE  next_track;
   BYTE  next_sector;
   BYTE  cht;          // chain track
   BYTE  chs;          // chain sector
   BYTE  chp;          // chain pointer
   BYTE  access_mode;
} File;

typedef struct
{                                  // derived from endpoint_t
   endpoint_t    base;             // payload
   FILE         *Ip;               // Image file pointer
   char         *curpath;          // malloc'd current path
   Disk_Image_t  DI;               // mounted disk image
   BYTE         *BAM[4];           // Block Availability Maps
   int           BAMpos[4];        // File position of BAMs
   BYTE         *buf[5];           // direct channel block buffer
   BYTE          chan[5];          // channel #
   BYTE          bp[5];            // buffer pointer
   BYTE          CurrentTrack;     // start track for scannning of BAM
   BYTE          U2_track;         // track  for U2 command
   BYTE          U2_sector;        // sector for U2 command
   slot_t        Slot;             // directory slot
   File          files[MAXFILES];  // files inside disk image
}  di_endpoint_t;

extern provider_t di_provider;

// prototypes
BYTE di_next_track(di_endpoint_t *diep);
static int di_block_alloc(di_endpoint_t *diep, BYTE *track, BYTE *sector);
static int di_block_free(di_endpoint_t *diep, BYTE Track, BYTE Sector);

// ************
// di_assert_ts
// ************

int di_assert_ts(di_endpoint_t *diep, BYTE track, BYTE sector)
{
   if (track < 1 || track > diep->DI.Tracks * diep->DI.Sides ||
       sector > diep->DI.Sectors) return CBM_ERROR_ILLEGAL_T_OR_S;
   return CBM_ERROR_OK;
}

// ************
// di_fseek_tsp
// ************

void di_fseek_tsp(di_endpoint_t *diep, BYTE track, BYTE sector, BYTE ptr)
{
   long seekpos = ptr+256*diep->DI.LBA(track,sector);
   log_debug("seeking to position %ld for t/s/p=%d/%d/%d\n", seekpos, track, sector, ptr);
   fseek(diep->Ip, seekpos,SEEK_SET);
}

// ************
// di_fseek_pos
// ************

void di_fseek_pos(di_endpoint_t *diep, int pos)
{
   fseek(diep->Ip,pos,SEEK_SET);
}

// **********
// di_init_fp
// **********

static void di_init_fp(File *fp)
{
  fp->chan = -1;
  fp->buf  = NULL;
  fp->is_first  = 0;
  fp->cht = 0;
  fp->chs = 0;
  fp->chp = 0;
}

// **************
// di_print_block
// **************

void di_print_block(di_endpoint_t *diep, int pos)
{
   BYTE b[16];
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

// *************
// di_dump_block
// *************

void di_dump_block(BYTE *b)
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

void di_print_slot(slot_t *slot)
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

// *************
// di_write_slot
// *************

void di_write_slot(di_endpoint_t *diep, slot_t *slot)
{
   BYTE p[32];

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
   fwrite(p+2,1,30,diep->Ip);
   // di_print_block(diep,slot->pos & 0xffffff00);
}

// ***********
// di_sync_BAM
// ***********

void di_sync_BAM(di_endpoint_t *diep)
{
   int i;

   for (i=0 ; i < diep->DI.BAMBlocks ; ++i)
   {
      di_fseek_pos(diep,diep->BAMpos[i]);
      fwrite(diep->BAM[i],1,256,diep->Ip);
   }
}

// ***********
// di_close_fd
// ***********

static int di_close_fd(di_endpoint_t *diep, File *f)
{
  BYTE t,s;
  log_debug("Closing file %p access mode = %d\n", f, f->access_mode);

  if (f->access_mode == FS_OPEN_WR ||
      f->access_mode == FS_OPEN_RW ||
      f->access_mode == FS_OPEN_OW ||
      f->access_mode == FS_OPEN_AP)
  {
     t = 0;
     s = f->chp+1;
     di_fseek_tsp(diep,f->cht,f->chs,0);
     fwrite(&t,1,1,diep->Ip);
     fwrite(&s,1,1,diep->Ip);
     log_debug("Updated chain to (%d/%d)\n",t,s);
     di_write_slot(diep,&f->Slot); // Save new status of directory entry
     di_sync_BAM(diep);            // Save BAM status
     fflush(diep->Ip);
  }
  di_init_fp(f);
  return 0;
}

// *********
// di_freeep
// *********

static void di_freeep(endpoint_t *ep)
{
   di_endpoint_t *cep = (di_endpoint_t*) ep;
   int i;
   for(i=0;i<MAXFILES;i++)
   {
       di_close_fd(cep, &(cep->files[i]));
   }
   mem_free(cep->curpath);
   mem_free(ep);
}

// ***********
// di_read_BAM
// ***********

void di_read_BAM(di_endpoint_t *diep)
{
   int i;

   for (i=0 ; i < diep->DI.BAMBlocks ; ++i)
   {
      diep->BAM[i] = (BYTE *)malloc(256);
      di_fseek_pos(diep,diep->BAMpos[i]);
      fread(diep->BAM[i],1,256,diep->Ip);
   }
}

// *************
// di_load_image
// *************

int di_load_image(di_endpoint_t *diep, const char *filename)
{
   int filesize;

   if (!(diep->Ip = fopen(filename, "rb+"))) return 0;
   fseek(diep->Ip, 0, SEEK_END);
   filesize = ftell(diep->Ip);
   log_debug("image size = %d\n",filesize);

   if (filesize == d64.Blocks * 256)
   {
      diep->DI = d64;
      diep->BAMpos[0] = 256 * diep->DI.LBA(18,0);
   }
   else if (filesize == d64.Blocks * 512)
   {
      diep->DI           = d64;
      diep->DI.ID        =  71;
      diep->DI.Sides     =   2;
      diep->DI.BAMBlocks =   2;
      diep->DI.LBA       = LBA71;
      diep->BAMpos[0]    = 256 * diep->DI.LBA(18,0);
      diep->BAMpos[1]    = 256 * diep->DI.LBA(53,0);
   }
   else if (filesize == d80.Blocks * 256)
   {
      diep->DI = d80;
      diep->BAMpos[0]    = 256 * diep->DI.LBA(38,0);
      diep->BAMpos[1]    = 256 * diep->DI.LBA(38,3);
   }
   else if (filesize == d80.Blocks * 512)
   {
      diep->DI       = d80;
      diep->DI.ID    =  82;
      diep->DI.Sides =   2;
      diep->DI.LBA       = LBA82;
      diep->BAMpos[0]    = 256 * diep->DI.LBA(38,0);
      diep->BAMpos[1]    = 256 * diep->DI.LBA(38,3);
      diep->BAMpos[2]    = 256 * diep->DI.LBA(38,6);
      diep->BAMpos[3]    = 256 * diep->DI.LBA(38,9);
   }
   else if (filesize == d81.Blocks * 256)
   {
      diep->DI = d81;
      diep->BAMpos[0]    = 256 * diep->DI.LBA(40,1);
      diep->BAMpos[1]    = 256 * diep->DI.LBA(40,2);
   }
   else return 0; // not an image file

   di_read_BAM(diep);
   log_debug("di_load_image(%s) as d%d\n",filename,diep->DI.ID);
   return 1; // success
}

// ********
// di_newep
// ********

static endpoint_t *di_newep(endpoint_t *parent, const char *path)
{
   (void) parent; // silence -Wunused-parameter

   int i;
   di_endpoint_t *diep = malloc(sizeof(di_endpoint_t));
   diep->curpath = malloc(strlen(path)+1);
   strcpy(diep->curpath,path);
   for(int i=0;i<MAXFILES;i++) di_init_fp(&(diep->files[i]));
   diep->base.ptype = &di_provider;
   di_load_image(diep,path);
   for (i=0 ; i < 5 ; ++i) diep->chan[i] = -1;
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

// ***************
// di_reserve_file
// ***************

static File *di_reserve_file(di_endpoint_t *diep, int chan)
{
   for (int i = 0; i < MAXFILES; i++)
   {
      if (diep->files[i].chan == chan) di_close_fd(diep,&(diep->files[i]));
      if (diep->files[i].chan < 0)
      {
         File *fp = &(diep->files[i]);
         di_init_fp(fp);
         fp->chan = chan;
         log_debug("reserving file %p for chan %d\n", fp, chan);
         return &(diep->files[i]);
      }
   }
   log_warn("Did not find free di session for channel=%d\n", chan);
   return NULL;
}

// ************
// di_find_file
// ************

static File *di_find_file(di_endpoint_t *diep, int chan)
{
   log_debug("findfile(%p,%d)\n",diep,chan);
   for (int i = 0; i < MAXFILES; i++)
   {
      if (diep->files[i].chan == chan) return &(diep->files[i]);
   }
   log_warn("Did not find di session for channel=%d\n", chan);
   return NULL;
}


// ***************
// di_alloc_buffer
// ***************

int di_alloc_buffer(di_endpoint_t *diep)
{
  log_debug("di_alloc_buffer 0\n");

  if (!diep->buf[0]) diep->buf[0] = (BYTE *)calloc(256,1);
  if (!diep->buf[0]) return 0; // OOM
  diep->bp[0] = 0;
  return 1; // success
}

// **************
// di_load_buffer
// **************

int di_load_buffer(di_endpoint_t *diep, BYTE track, BYTE sector)
{
   if (!di_alloc_buffer(diep)) return 0; // OOM
   log_debug("di_load_buffer %p->%p U1(%d/%d)\n",diep,diep->buf[0],track,sector);
   di_fseek_tsp(diep,track,sector,0);
   fread(diep->buf[0],1,256,diep->Ip);
   // di_dump_block(diep->buf[0]);
   return 1; // OK
}

// **************
// di_save_buffer
// **************

int di_save_buffer(di_endpoint_t *diep)
{
   log_debug("di_save_buffer U2(%d/%d)\n",diep->U2_track,diep->U2_sector);
   di_fseek_tsp(diep,diep->U2_track,diep->U2_sector,0);
   fwrite(diep->buf[0],1,256,diep->Ip);
   fflush(diep->Ip);
   diep->U2_track = 0;
   // di_dump_block(diep->buf[0]);
   return 1; // OK
}

// **************
// di_flag_buffer
// **************

void di_flag_buffer(di_endpoint_t *diep, BYTE track, BYTE sector)
{
   log_debug("di_flag_buffer U2(%d/%d)\n",track,sector);
   diep->U2_track  = track;
   diep->U2_sector = sector;
   diep->bp[0]     = 0;
}

// *************
// di_read_block
// *************

int di_read_block(di_endpoint_t *diep, int tfd, char *retbuf, int len, int *eof)
{
   log_debug("di_read_block: chan=%d len=%d\n", tfd, len);

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

int di_write_block(di_endpoint_t *diep, char *buf, int len)
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

int di_direct(endpoint_t *ep, char *buf, char *retbuf, int *retlen)
{
   int rv = CBM_ERROR_OK;

   di_endpoint_t *diep = (di_endpoint_t *)ep;

   BYTE cmd    = (BYTE)buf[FS_BLOCK_PAR_CMD    -1];
   BYTE track  = (BYTE)buf[FS_BLOCK_PAR_TRACK  -1];	// ignoring high byte
   BYTE sector = (BYTE)buf[FS_BLOCK_PAR_SECTOR -1];	// ignoring high byte
   BYTE chan   = (BYTE)buf[FS_BLOCK_PAR_CHANNEL-1];

   log_debug("di_direct(cmd=%d, tr=%d, se=%d ch=%d\n",cmd,track,sector,chan);
   rv = di_assert_ts(diep,track,sector);
   if (rv != CBM_ERROR_OK) return rv; // illegal track or sector

   switch (cmd)
   {
      case FS_BLOCK_BR:
      case FS_BLOCK_U1: 
	di_load_buffer(diep,track,sector); 
   	diep->chan[0] = chan; // assign channel # to buffer
   	channel_set(chan,ep);
	break;
      case FS_BLOCK_BW:
      case FS_BLOCK_U2: 
      	if (!di_alloc_buffer(diep)) {
		return CBM_ERROR_NO_CHANNEL; // OOM
	}
	di_flag_buffer(diep,track,sector); 
   	diep->chan[0] = chan; // assign channel # to buffer
   	channel_set(chan,ep);
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


// ********
// di_close
// ********

static void di_close(endpoint_t *ep, int tfd)
{
   log_debug("di_close %d\n",tfd);
   File *file = di_find_file((di_endpoint_t *)ep, tfd);
   if (file) di_close_fd((di_endpoint_t *)ep,file);
   os_sync();
}

// ************
// di_read_slot
// ************

void di_read_slot(di_endpoint_t *diep, slot_t *slot)
{
   int i=0;
   BYTE p[32];
   di_fseek_pos(diep,slot->pos);
   fread(p,1,32,diep->Ip);
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

   log_debug("di_read_slot <%s>\n",slot->filename);
}

// *************
// di_first_slot
// *************

void di_first_slot(di_endpoint_t *diep, slot_t *slot)
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

int di_next_slot(di_endpoint_t *diep,slot_t *slot)
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

int di_match_slot(di_endpoint_t *diep,slot_t *slot, BYTE *name)
{
   do
   {
      di_read_slot(diep,slot);
      if (slot->type && compare_pattern((char*)slot->filename,(char*)name)) return 1; // found
   }  while (di_next_slot(diep,slot));
   return 0; // not found
}

// **************
// di_clear_block
// **************

void di_clear_block(di_endpoint_t *diep, int pos)
{
   BYTE p[256];
   
   memset(p,0,256);
   di_fseek_pos(diep,pos);
   fwrite(p,1,256,diep->Ip);
}


// *******************
// di_update_dir_chain
// *******************

void di_update_dir_chain(di_endpoint_t *diep, slot_t *slot, BYTE sector)
{
   di_fseek_pos(diep,slot->pos & 0xffff00);
   fwrite(&diep->DI.DirTrack,1,1,diep->Ip);
   fwrite(&sector           ,1,1,diep->Ip);
}

// ***********
// di_scan_BAM
// ***********

// scan to find a new sector for a file, using the interleave
// allocate the sector found
static int di_scan_BAM_interleave(Disk_Image_t *di, BYTE *bam, BYTE interleave)
{
   BYTE i; // interleave index
   BYTE s; // sector index

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
static int di_scan_BAM_linear(Disk_Image_t *di, BYTE *bam, BYTE firstSector)
{
   BYTE s = firstSector; // sector index

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
static void di_alloc_BAM(BYTE *bam, BYTE sector)
{
	bam[sector>>3] &= ~(1 << (sector & 7));
}

// calculate the position of the BAM entry for a given track
static void di_calculate_BAM(di_endpoint_t *diep, BYTE Track, BYTE **outBAM, BYTE **outFbl) {
   int   BAM_Number;     // BAM block for current track
   int   BAM_Offset;  
   int   BAM_Increment;
   Disk_Image_t *di = &diep->DI;
   BYTE *fbl;            // pointer to track free blocks
   BYTE *bam;            // pointer to track free blocks

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
static int di_block_alloc(di_endpoint_t *diep, BYTE *track, BYTE *sector) {
   int  Sector;         // sector of next free block
   Disk_Image_t *di = &diep->DI;
   BYTE *fbl;	// pointer to free blocks byte
   BYTE *bam;	// pointer to BAM bit field

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

int di_scan_track(di_endpoint_t *diep, BYTE Track)
{
   int   Sector;
   int   Interleave;
   BYTE *fbl;            // pointer to track free blocks
   BYTE *bam;            // pointer to track BAM
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

int di_allocate_new_dir_block(di_endpoint_t *diep, slot_t *slot)
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

int di_find_free_slot(di_endpoint_t *diep, slot_t *slot)
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

BYTE di_next_track(di_endpoint_t *diep)
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

int di_find_free_block(di_endpoint_t *diep, File *f)
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
// di_create_entry // TODO: set file type
// ***************

int di_create_entry(di_endpoint_t *diep, int tfd, BYTE *name, BYTE type, BYTE reclen)
{
   log_debug("di_create_entry(%s)\n",name);
   File *file = di_find_file(diep, tfd);
   if (!file) return CBM_ERROR_FAULT;
   if (di_find_free_slot(diep,&file->Slot)) return CBM_ERROR_DISK_FULL;
   if (di_find_free_block(diep,file) < 0)   return CBM_ERROR_DISK_FULL;
   strcpy((char *)file->Slot.filename,(char *)name);
   file->Slot.type = 0x80 | type;
   file->chp = 0;
   file->Slot.ss_track  = 0;	// invalid;
   file->Slot.ss_sector = 0;
   file->Slot.start_track  = 0;
   file->Slot.start_sector = 0;
   file->Slot.start_track  = file->cht;
   file->Slot.start_sector = file->chs;
   if (type == FS_DIR_TYPE_REL) {
	// on rel files, the size reported is zero after creation
	// despite two blocks being taken
   	file->Slot.size = 0;
        if (di_find_free_block(diep,file) < 0) {
		// couldn't allocate the side sector block
		di_block_free(diep, file->Slot.start_track, file->Slot.start_sector);
		return CBM_ERROR_DISK_FULL;
	} else {
		// found a side sector block
		// TODO: clear out side sector block to default
	}
   	file->Slot.ss_track  = file->cht;
   	file->Slot.ss_sector = file->chs;
   } else {
   	file->Slot.size = 1;
   }
   file->Slot.recordlen = reclen;
   di_write_slot(diep,&file->Slot);
   // di_print_slot(&file->Slot);
   return CBM_ERROR_OK;
}

// *************
// di_pos_append
// *************

void di_pos_append(di_endpoint_t *diep, File *f)
{
   BYTE t,s,nt,ns; 
   
   nt = f->next_track;
   ns = f->next_sector;
   while (nt)
   {
      t = nt;
      s = ns;
      di_fseek_tsp(diep,t,s,0);
      fread(&nt,1,1,diep->Ip);
      fread(&ns,1,1,diep->Ip);
   }
   f->cht =  t;
   f->chs =  s;
   f->chp = ns-1;
   log_debug("di_pos_append (%d/%d) %d\n",t,s,ns);
}

// ************
// di_open_file
// ************

static void di_process_options(BYTE *opts, BYTE *type, BYTE *reclen) {
	BYTE *p = opts;
	BYTE typechar;
	int reclenw;
	int n;
	BYTE *t;

	while (*p != 0) {
		switch(*(p++)) {
		case 'T':
			if (*(p++) == '=') {
				typechar = *(p++);
				switch(typechar) {
				case 'U':	*type = FS_DIR_TYPE_USR; break;
				case 'P':	*type = FS_DIR_TYPE_PRG; break;
				case 'S':	*type = FS_DIR_TYPE_SEQ; break;
				case 'L':	
					*type = FS_DIR_TYPE_REL; 
					n=sscanf((char*)p, "%d", &reclenw);
					if (n == 1 && reclenw > 0 && reclenw < 255) {
						*reclen = reclenw;
					}
					t = strchr((char*)p, ',');
					if (t == NULL) {
						t = p + strlen((char*)p);
					}
					p = t;
					break;
				default:
					log_warn("Unknown open file type option %c\n", typechar);
					break;
				}
			}
			break;
		case ',':
			p++;
			break;
		default:
			// syntax error
			log_warn("error parsing file open options %s\n", opts);
			return;
		}
	}
}

static int di_open_file(endpoint_t *ep, int tfd, BYTE *filename, BYTE *opts, int di_cmd)
{
   int np,rv;
   File *file;
   BYTE type = FS_DIR_TYPE_PRG;	// PRG
   BYTE reclen = 0;		// REL record length (default 0 means is not set)

   di_process_options(opts, &type, &reclen);
 
   log_info("OpenFile(..,%d,%s,%c,%d)\n", tfd, filename, type + 0x30, reclen);
   di_endpoint_t *diep = (di_endpoint_t*) ep;
   file = di_reserve_file(diep, tfd);
 
   int file_required       = FALSE;
   int file_must_not_exist = FALSE;

   file->access_mode = di_cmd;
 
   switch(di_cmd)
   {
      case FS_OPEN_AP:
      case FS_OPEN_RD: file_required       = TRUE; break;
      case FS_OPEN_WR: file_must_not_exist = TRUE; break;
      case FS_OPEN_OW: break;
      case FS_OPEN_RW:
	 if (type != FS_DIR_TYPE_REL) {
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
   	np  = di_match_slot(diep,&file->Slot,filename);
   	file->next_track  = file->Slot.start_track;
   	file->next_sector = file->Slot.start_sector;
	if (type == FS_DIR_TYPE_REL) {
		// check record length
		if (!np) {
			// does not exist yet
			if (reclen == 0) {
				return CBM_ERROR_RECORD_NOT_PRESENT;
			}
		} else {
			if (reclen == 0) {
				// no reclen is given in the open
				reclen = file->Slot.recordlen;
			} else {
				// there is a rec len in the open and in the file
				// so they need to be the same
				if (reclen != file->Slot.recordlen) {
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
      rv = di_create_entry(diep, tfd, filename, type, reclen);
      if (rv != CBM_ERROR_OK) return rv;
   }
   if (di_cmd == FS_OPEN_AP) di_pos_append(diep,file);
   return CBM_ERROR_OK;
}

// **********
// di_opendir
// **********

static int di_opendir(endpoint_t *ep, int tfd, const char *buf, const char *opts)
{
   di_endpoint_t *diep = (di_endpoint_t*) ep;
   log_debug("di_opendir(%s)\n",buf);
   File *file = di_reserve_file(diep, tfd);

   if (file)
   {
      if (buf && buf[0]) strcpy((char *)file->dirpattern, buf);
      else               strcpy((char *)file->dirpattern, "*");
      file->is_first = 1;
      return CBM_ERROR_OK;
   }
   else return CBM_ERROR_FAULT;
}

char *extension[6] = { "DEL","SEQ","PRG","USR","REL","CBM" };

// *************
// di_fill_entry
// *************

int di_fill_entry(BYTE *dest, slot_t *slot)
{
   char *p = (char *)dest + FS_DIR_NAME;
   int sz  = slot->size * 254;

   log_debug("di_fill_entry(%s)\n",slot->filename);

   dest[FS_DIR_LEN  ] = sz;
   dest[FS_DIR_LEN+1] = sz >>  8;
   dest[FS_DIR_LEN+2] = sz >> 16;
   dest[FS_DIR_LEN+3] = 0;
   dest[FS_DIR_MODE]  = FS_DIR_MOD_FIL;
   
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

int di_directory_header(char *dest, di_endpoint_t *diep)
{
   memset(dest+FS_DIR_LEN,0,4);
   dest[FS_DIR_MODE]  = FS_DIR_MOD_NAM;
   if (diep->DI.ID == 80 || diep->DI.ID == 82)
   {
      di_fseek_tsp(diep,39,0, 6);
      fread(dest+FS_DIR_NAME   ,1,16,diep->Ip);
      di_fseek_tsp(diep,39,0,24);
      fread(dest+FS_DIR_NAME+16,1, 5,diep->Ip);
   }
   else if (diep->DI.ID == 81)
   {
      di_fseek_tsp(diep,40,0, 4);
      fread(dest+FS_DIR_NAME   ,1,16,diep->Ip);
      di_fseek_tsp(diep,40,0,22);
      fread(dest+FS_DIR_NAME+16,1, 5,diep->Ip);
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

int di_blocks_free(char *dest, di_endpoint_t *diep)
{
   int   FreeBlocks;
   int   BAM_Number;     // BAM block for current track
   int   BAM_Increment;
   int   Track;
   int   i;
   BYTE *fbl;            // pointer to track free blocks
   Disk_Image_t *di = &diep->DI;

   FreeBlocks    = 0; 
   BAM_Number    = 0;
   BAM_Increment = 1 + ((di->Sectors + 7) >> 3);
   Track         = 1;

   while (diep->BAM[BAM_Number])
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

// *****************
// di_read_dir_entry
// *****************

int di_read_dir_entry(di_endpoint_t *diep, int tfd, char *retbuf, int *eof)
{
   int rv = 0;
   File *file = di_find_file(diep, tfd);
   log_debug("di_read_dir_entry(%d)\n",tfd);

   if (!file) return -CBM_ERROR_FAULT;

   *eof = READFLAG_DENTRY;

   if (file->is_first == 1)
   {
      file->is_first++;
      rv = di_directory_header(retbuf,diep);
      di_first_slot(diep,&diep->Slot);
      return rv;
   }

   if (!diep->Slot.eod && di_match_slot(diep,&diep->Slot,file->dirpattern))
   {    
      rv = di_fill_entry((BYTE *)retbuf,&diep->Slot);
      di_next_slot(diep,&diep->Slot);
      return rv;
   }

   *eof |= READFLAG_EOF;
   return di_blocks_free(retbuf,diep);
}

// ************
// di_read_byte
// ************

int di_read_byte(di_endpoint_t *diep, File *f, char *retbuf)
{
   if (f->chp > 253)
   {
      f->chp = 0;
      if (f->next_track == 0) return READFLAG_EOF; // EOF
      di_fseek_tsp(diep,f->next_track,f->next_sector,0);
      f->cht = f->next_track;
      f->chs = f->next_sector;
      fread(&f->next_track ,1,1,diep->Ip);
      fread(&f->next_sector,1,1,diep->Ip);
      // log_debug("this block: (%d/%d)\n",f->cht,f->chs);
      // log_debug("next block: (%d/%d)\n",f->next_track,f->next_sector);
   }
   fread(retbuf,1,1,diep->Ip);
   ++f->chp;
   // log_debug("di_read_byte %2.2x\n",(BYTE)*retbuf);
   if (f->next_track == 0 && f->chp+1 >= f->next_sector) return READFLAG_EOF;
   return 0;
}

// *************
// di_write_byte
// *************

int di_write_byte(di_endpoint_t *diep, File *f, BYTE ch)
{
   int oldpos;
   int block;
   BYTE zero = 0;
   // log_debug("di_write_byte %2.2x\n",ch);
   if (f->chp > 253)
   {
      f->chp = 0;
      oldpos = 256 * diep->DI.LBA(f->cht,f->chs);
      block = di_find_free_block(diep,f);
      if (block < 0) return CBM_ERROR_DISK_FULL;
      di_fseek_pos(diep,oldpos);
      fwrite(&f->cht,1,1,diep->Ip);
      fwrite(&f->chs,1,1,diep->Ip);
      di_fseek_tsp(diep,f->cht,f->chs,0);
      fwrite(&zero,1,1,diep->Ip); // new link track
      fwrite(&zero,1,1,diep->Ip); // new link sector
      ++f->Slot.size; // increment filesize
      // log_debug("next block: (%d/%d)\n",f->cht,f->chs);
   }
   fwrite(&ch,1,1,diep->Ip);
   ++f->chp;
   return CBM_ERROR_OK;
}

// ***********
// di_read_seq
// ***********

static int di_read_seq(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof)
{
   int i;
   di_endpoint_t *diep = (di_endpoint_t*) ep;
   log_debug("di_read_seq(chan %d, len=%d)\n",tfd,len);
   File *file = di_find_file(diep, tfd);

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

static int di_writefile(endpoint_t *ep, int tfd, char *buf, int len, int is_eof)
{
   int i;
   di_endpoint_t *diep = (di_endpoint_t*) ep;

   if (diep->U2_track) // fill block for U2 command
   {
      di_write_block(diep,buf,len);
      if (is_eof) di_save_buffer(diep);
      return CBM_ERROR_OK;
   }

   File *f = di_find_file(diep, tfd);
   di_fseek_tsp(diep,f->cht,f->chs,2+f->chp);
   for (i=0 ; i < len ; ++i)
      if (di_write_byte(diep, f, (BYTE)buf[i])) return -CBM_ERROR_DISK_FULL;

   if (is_eof)
   {
      log_debug("Close fd=%d normally on write file received an EOF\n", tfd);
      di_close(ep, tfd);
   }
   return CBM_ERROR_OK;
}

// *************
// di_block_free
// *************

static int di_block_free(di_endpoint_t *diep, BYTE Track, BYTE Sector)
{
   BYTE *fbl;            // pointer to track free blocks
   BYTE *bam;            // pointer to track BAM

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

void di_delete_file(di_endpoint_t *diep, slot_t *slot)
{
   BYTE t,s;
   log_debug("di_delete_file #%d <%s>\n",slot->number,slot->filename);
   
   slot->type = 0;          // mark as deleted
   di_write_slot(diep,slot);
   t = slot->start_track ;
   s = slot->start_sector;
   while (t)               // follow chain for freeing blocks
   {
      di_block_free(diep,t,s);
      di_fseek_tsp(diep,t,s,0);
      fread(&t,1,1,diep->Ip);
      fread(&s,1,1,diep->Ip);
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
      if ((found = di_match_slot(diep,&slot,(BYTE *)buf)))
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
   if ((found = di_match_slot(diep,&slot,(BYTE *)nameto)))
   {
      return CBM_ERROR_FILE_EXISTS;
   }

   di_first_slot(diep,&slot);
   if ((found = di_match_slot(diep,&slot,(BYTE *)namefrom)))
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
// di_open_rd
//***********

static int di_open_rd(endpoint_t *ep, int tfd, const char *buf, const char *opts)
{
   return di_open_file(ep, tfd, (BYTE *)buf, (BYTE *)opts, FS_OPEN_RD);
}

//***********
// di_open_wr
//***********

static int di_open_wr(endpoint_t *ep, int tfd, const char *buf, const char *opts,
                        const int is_overwrite)
{
  if (is_overwrite) return di_open_file(ep, tfd, (BYTE *)buf, (BYTE *)opts, FS_OPEN_OW);
  else              return di_open_file(ep, tfd, (BYTE *)buf, (BYTE *)opts, FS_OPEN_WR);
}

//***********
// di_open_ap
//***********

static int di_open_ap(endpoint_t *ep, int tfd, const char *buf, const char *opts)
{
       return di_open_file(ep, tfd, (BYTE *)buf, (BYTE *)opts, FS_OPEN_AP);
}

// **********
// di_open_rw
// **********

static int di_open_rw(endpoint_t *ep, int tfd, const char *buf, const char *opts)
{
       return di_open_file(ep, tfd, (BYTE *)buf, (BYTE *)opts, FS_OPEN_RW);
}


// *****************
// di_direct_channel
// *****************

int di_direct_channel(di_endpoint_t *diep, int chan)
{
   int i;

   for (i=0 ; i < 5 ; ++i)
      if (diep->chan[i] == chan) return i;
   return -1; // no direct channel
}

// ***********
// di_readfile
// ***********

static int di_readfile(endpoint_t *ep, int chan, char *retbuf, int len, int *eof)
{
   int rv = 0;
   di_endpoint_t *diep = (di_endpoint_t*) ep;
   log_debug("di_readfile(%p chan=%d len=%d\n",ep,chan,len);
   
   if (di_direct_channel(diep,chan) >= 0)
   {
      return di_read_block(diep, chan, retbuf, len, eof);
   }

   File *f = di_find_file(diep, chan);

   if (f->is_first) rv = di_read_dir_entry(diep, chan, retbuf, eof);
   else             rv = di_read_seq(ep, chan, retbuf, len, eof);
   return rv;
}

// *******
// di_init
// *******

void di_init(void)
{
   log_debug("di_init\n");
}

// ----------------------------------------------------------------------------------

provider_t di_provider =
{
  "di",
  "PETSCII",
   di_init,         // void        (*init     )(void);
   di_newep,        // endpoint_t* (*newep    )(endpoint_t *parent, ...
   di_tempep,       // endpoint_t* (*tempep   )(char **par); 
   di_freeep,       // void        (*freeep   )(endpoint_t *ep); 
   di_close,        // void        (*close    )(endpoint_t *ep, int chan);
   di_open_rd,      // int         (*open_rd  )(endpoint_t *ep, int chan, ...
   di_open_wr,      // int         (*open_wr  )(endpoint_t *ep, int chan, ...
   di_open_ap,      // int         (*open_ap  )(endpoint_t *ep, int chan, ...
   di_open_rw,      // int         (*open_rw  )(endpoint_t *ep, int chan, ...
   di_opendir,      // int         (*opendir  )(endpoint_t *ep, int chan, ...
   di_readfile,     // int         (*readfile )(endpoint_t *ep, int chan, ...
   di_writefile,    // int         (*writefile)(endpoint_t *ep, int chan, ...
   di_scratch,      // int         (*scratch  )(endpoint_t *ep, char *name, ...
   di_rename,       // int         (*rename   )(endpoint_t *ep, char *nameto, ...
   di_cd,           // int         (*cd       )(endpoint_t *ep, char *name); 
   NULL,            // int         (*mkdir    )(endpoint_t *ep, char *name); 
   NULL,            // int         (*rmdir    )(endpoint_t *ep, char *name);
   NULL,            // int         (*block    )(endpoint_t *ep, int chan, char *buf);
   di_direct        // int         (*direct   )(endpoint_t *ep, char *buf, ...
};

