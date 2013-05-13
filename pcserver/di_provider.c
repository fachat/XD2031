/****************************************************************************

    Commodore disk image Serial line server
    Copyright (C) 2012 Andre Fachat

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
 * This file is a filesystem provider implementation, to be
 * used with the FSTCP program on an OS/A65 computer.
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
#include <dirent.h>
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
   BYTE          CurrentTrack;     // start track for scannning of BAM
   slot_t        Slot;             // directory slot
   File          files[MAXFILES];  // files inside disk image
}  di_endpoint_t;

extern provider_t di_provider;


// ************
// di_fseek_tsp
// ************

void di_fseek_tsp(di_endpoint_t *diep, BYTE track, BYTE sector, BYTE ptr)
{
   fseek(diep->Ip,ptr+256*diep->DI.LBA(track,sector),SEEK_SET);
}

// ************
// di_fseek_pos
// ************

void di_fseek_pos(di_endpoint_t *diep, int pos)
{
   fseek(diep->Ip,pos,SEEK_SET);
}

// *******
// init_fp
// *******

static void init_fp(File *fp)
{
  fp->chan = -1;
  fp->buf  = NULL;
  fp->is_first  = 0;
  fp->cht = 0;
  fp->chs = 0;
  fp->chp = 0;
}

// **********
// PrintBlock
// **********

void PrintBlock(di_endpoint_t *diep, int pos)
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

// *********
// DumpBlock
// *********

void DumpBlock(BYTE *b)
{
   int i,j;
   printf("BLOCK:\n");
   for (j=0 ; j<256 ; j+=16)
   {
      for (i=0 ; i < 16; ++i) printf(" %2.2x",b[i+j]);
      printf("   ");
      for (i=0 ; i < 16; ++i)
      {
        if (b[i] > 31 && b[i] < 96) printf("%c",b[i+j]);
        else printf(".");
      }
      printf("\n");
   }
}

// *********
// PrintSlot
// *********

void PrintSlot(slot_t *slot)
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

// *********
// WriteSlot
// *********

void WriteSlot(di_endpoint_t *diep, slot_t *slot)
{
   BYTE p[32];

   log_debug("WriteSlot %d\n",slot->number);
   PrintSlot(slot);
   memset(p,0,32);      // clear slot
   memset(p+5,0xa0,16); // fill name with $A0
   memcpy(p+5,slot->filename,strlen((char *)slot->filename));
   p[ 2] = slot->type;
   p[ 3] = slot->start_track;
   p[ 4] = slot->start_sector;
   p[30] = slot->size & 0xff;
   p[31] = slot->size >> 8;

   log_debug("WriteSlot pos %x\n",slot->pos);
   di_fseek_pos(diep,slot->pos+2);
   fwrite(p+2,1,30,diep->Ip);
   PrintBlock(diep,slot->pos & 0xffffff00);
}

// *******
// SyncBAM
// *******

void SyncBAM(di_endpoint_t *diep)
{
   int i;

   for (i=0 ; i < diep->DI.BAMBlocks ; ++i)
   {
      di_fseek_pos(diep,diep->BAMpos[i]);
      fwrite(diep->BAM[i],1,256,diep->Ip);
   }
}

// ********
// close_fd
// ********

static int close_fd(di_endpoint_t *diep, File *f)
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
     WriteSlot(diep,&f->Slot); // Save new status of directory entry
     SyncBAM(diep);            // Save BAM status
  }
  init_fp(f);
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
       close_fd(cep, &(cep->files[i]));
   }
   mem_free(cep->curpath);
   mem_free(ep);
}

// *******
// ReadBAM
// *******

void ReadBAM(di_endpoint_t *diep)
{
   int i;

   for (i=0 ; i < diep->DI.BAMBlocks ; ++i)
   {
      diep->BAM[i] = (BYTE *)malloc(256);
      di_fseek_pos(diep,diep->BAMpos[i]);
      fread(diep->BAM[i],1,256,diep->Ip);
   }
}

// *********
// LoadImage
// *********

int LoadImage(di_endpoint_t *diep, const char *filename)
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

   ReadBAM(diep);
   log_debug("LoadImage(%s) as d%d\n",filename,diep->DI.ID);
   return 1; // success
}

// ********
// di_newep
// ********

static endpoint_t *di_newep(endpoint_t *parent, const char *path)
{
   log_debug("Setting di endpoint to '%s' parent %p\n", path,(void *)parent);
   di_endpoint_t *diep = malloc(sizeof(di_endpoint_t));
   diep->curpath = malloc(strlen(path)+1);
   strcpy(diep->curpath,path);
   for(int i=0;i<MAXFILES;i++) init_fp(&(diep->files[i]));
   diep->base.ptype = &di_provider;
   LoadImage(diep,path);
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
  return (endpoint_t*) diep;
}

// ************
// reserve_file
// ************

static File *reserve_file(di_endpoint_t *diep, int chan)
{
   log_debug("reserve_file(%d)\n",chan);
   for (int i = 0; i < MAXFILES; i++)
   {
      if (diep->files[i].chan == chan) close_fd(diep,&(diep->files[i]));
      if (diep->files[i].chan < 0)
      {
         File *fp = &(diep->files[i]);
         init_fp(fp);
         fp->chan = chan;
         log_debug("reserving file %p for chan %d\n", fp, chan);
         return &(diep->files[i]);
      }
   }
   log_warn("Did not find free fs session for channel=%d\n", chan);
   return NULL;
}

// *********
// find_file
// *********

static File *find_file(di_endpoint_t *diep, int chan)
{
   log_debug("findfile(%p,%d)\n",diep,chan);
   for (int i = 0; i < MAXFILES; i++)
   {
      if (diep->files[i].chan == chan) return &(diep->files[i]);
   }
   log_warn("Did not find di session for channel=%d\n", chan);
   return NULL;
}


// *******************
// AllocateBLockBuffer
// *******************

int AllocateBLockBuffer(di_endpoint_t *diep, File *f)
{
  log_debug("AllocateBlockBuffer 0\n");

  diep->buf[0] = (BYTE *)calloc(256,1); // allocate and clear
  if (diep->buf[0] == NULL)
  {
    log_warn("Buffer memory alloc failed!");
    return 0; // no memory
  }
  f->buf = diep->buf[0];
  f->chp = 0; // Initialize block pointer (B-P) zo 0
  return 1; // success
}

// **********
// GetBuffer
// **********

void GetBuffer(di_endpoint_t *diep, BYTE t, BYTE s)
{
   log_debug("GetBuffer U1 (%d/%d)\n",t,s);
   di_fseek_tsp(diep,t,s,0);
   fread(diep->buf[0],1,256,diep->Ip);
   // DumpBlock(diep->buf[0]);
}

// ********
// di_block
// ********

int di_block(endpoint_t *ep, int chan, char *buf)
{
   BYTE cmd    = buf[0];
   BYTE drive  = buf[1];
   BYTE track  = buf[2];
   BYTE sector = buf[3];

   log_debug("BLOCK cmd: %d, ch=%d ,dr=%d, tr=%d, se=%d\n",
             cmd, chan, drive, track, sector);

   switch (cmd)
   {
      case FS_BLOCK_U1: GetBuffer((di_endpoint_t *)ep,track,sector); break;
      case FS_BLOCK_U2: log_debug("U2\n"); break;
      case FS_BLOCK_BR: log_debug("BR\n"); break;
      case FS_BLOCK_BW: log_debug("BW\n"); break;
      case FS_BLOCK_BP: log_debug("BP\n"); break;
   }
   return ERROR_OK;
}

// **********
// read_block
// **********

static int read_block(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof)
{
   File *file = find_file((di_endpoint_t *)ep, tfd);
   log_debug("read_block: file=%p, len=%d\n", file, len);
   if (!file) return -ERROR_FAULT;

   int avail = 256 - file->chp;
   int n = len;
   if (len > avail)
   {
      n = avail;
      *eof = 1;
   }
   log_debug("read_block: avail=%d, n=%d\n", avail, n);
   if (n > 0)
   {
      memcpy(retbuf, file->buf + file->chp, n);
      file->chp += n;
   }
   return n;
}


// ********
// di_close
// ********

static void di_close(endpoint_t *ep, int tfd)
{
   log_debug("di_close %d\n",tfd);
   File *file = find_file((di_endpoint_t *)ep, tfd);
   if (file) close_fd((di_endpoint_t *)ep,file);
   sync();
}

// *********
// CBM_match
// *********

int CBM_match(BYTE *x, BYTE *y)
{
   int i;
   BYTE a,b;

   log_debug("CBM_match(%s,%s)\n",x,y);

   for (i=0 ; i < 16; ++i)
   {
      a = x[i];
      b = y[i];
      if (a ==  0  && b ==  0 ) return 0; // match
      if (a == '*' || b == '*') return 0; // match
      if (a == '?' || b == '?') continue; // wild letter
      if (a < b) return -1;
      if (a > b) return  1;
   }
   return 0; // match
}

// ********
// ReadSlot
// ********

void ReadSlot(di_endpoint_t *diep, slot_t *slot)
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
   log_debug("ReadSlot <%s>\n",slot->filename);
}

// *********
// FirstSlot
// *********

void FirstSlot(di_endpoint_t *diep, slot_t *slot)
{
   log_debug("FirstSlot\n");
   if (diep->DI.ID == 80 || diep->DI.ID == 82)
      slot->pos  = 256 * diep->DI.LBA(39,1);
   else if (diep->DI.ID == 81)
      slot->pos  = 256 * diep->DI.LBA(40,3);
   else
      slot->pos  = 256 * diep->DI.LBA(diep->BAM[0][0],diep->BAM[0][1]);
   slot->number  =   0;
   slot->eod     =   0;
}

// ********
// NextSlot
// ********

int NextSlot(di_endpoint_t *diep,slot_t *slot)
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

// *********
// MatchSlot
// *********

int MatchSlot(di_endpoint_t *diep,slot_t *slot, BYTE *name)
{
   do
   {
      ReadSlot(diep,slot);
      if (slot->type && !CBM_match(slot->filename,name)) return 1; // found
   }  while (NextSlot(diep,slot));
   return 0; // not found
}

// **********
// ClearBlock
// **********

void ClearBlock(di_endpoint_t *diep, int pos)
{
   BYTE p[256];
   
   memset(p,0,256);
   di_fseek_pos(diep,pos);
   fwrite(p,1,256,diep->Ip);
}


// **************
// UpdateDirChain
// **************

void UpdateDirChain(di_endpoint_t *diep, slot_t *slot, BYTE t, BYTE s)
{
   di_fseek_pos(diep,slot->pos & 0xffff00);
   fwrite(&t,1,1,diep->Ip);
   fwrite(&s,1,1,diep->Ip);
}

// *******************
// AllocateNewDirBlock
// *******************

int AllocateNewDirBlock(di_endpoint_t *diep, slot_t *slot)
{
   BYTE i,t,s;
   BYTE *bp;

   t  = diep->DI.DirTrack;
   bp = diep->BAM[0] + t * 4;
   if (bp[0] > 0) // any free sectors on dir track?
   for (i=0 ; i < diep->DI.DirInterleave ; ++i)
   for (s=i ; s <  diep->DI.Sectors ; s += diep->DI.DirInterleave);
   if (bp[1+(s>>3)] & (1 << (s & 7)))
   { 
      bp[1+(s>>3)] &= ~(1 << (s & 7));
      --bp[0]; // decrement free blocks on track
      SyncBAM(diep);
      UpdateDirChain(diep,slot,t,s);
      slot->pos         = 256 * diep->DI.LBA(t,s);
      slot->eod         = 0;
      ClearBlock(diep,slot->pos);
      slot->next_track  = 0;
      slot->next_sector = 0;
      log_debug("AllocateNewDirBlock (%d/%d)\n",t,s);
      return 1;
   }
   return 0; // directory full
}

// ************
// FindFreeSlot
// ************

int FindFreeSlot(di_endpoint_t *diep, slot_t *slot)
{
   FirstSlot(diep,slot);
   do   
   {
      ReadSlot(diep,slot);
      if (slot->type == 0) return 1; // found
   }  while (NextSlot(diep,slot));
   if (AllocateNewDirBlock(diep,slot)) return 1;
   return 0; // not found
}

// *******
// ScanBAM
// *******

int ScanBAM(Disk_Image_t *di, BYTE *bam, BYTE interleave)
{
   BYTE i; // interleave index
   BYTE s; // sector index

   for (i=0 ; i < interleave ; ++i)
   for (s=i ; s < di->Sectors ; s += interleave)
   if (bam[s>>3] & (1 << (s & 7)))
   { 
      bam[s>>3] &= ~(1 << (s & 7));
      return s;
   }
   return -1; // no free sector
}

// *********
// NextTrack
// *********

BYTE NextTrack(di_endpoint_t *diep)
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
   
// *************
// di_scan_track
// *************

int di_scan_track(di_endpoint_t *diep, BYTE Track)
{
   int   BAM_Number;     // BAM block for current track
   int   BAM_Offset;  
   int   BAM_Increment;
   int   Sector;
   BYTE *fbl;            // pointer to track free blocks
   BYTE *bam;            // pointer to track BAM
   Disk_Image_t *di = &diep->DI;

   // log_debug("di_scan_track(%d)\n",Track);
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
   if (fbl[0]) Sector = ScanBAM(di,bam,di->DatInterleave);
   else        Sector = -1;
   if (Sector >= 0) --fbl[0]; // decrease free blocks counter
   return Sector;
}

// *************
// FindFreeBlock
// *************

int FindFreeBlock(di_endpoint_t *diep, File *f)
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
         SyncBAM(diep);
         f->chp = 0;
         f->cht = diep->CurrentTrack;
         f->chs = Sector;
         log_debug("FindFreeBlock (%d/%d)\n",f->cht,f->chs);
         return di->LBA(diep->CurrentTrack,Sector);
      }
   } while (NextTrack(diep) != StartTrack);
   return -1; // No free block -> DISK FULL
}

// ***********
// CreateEntry // TODO: set file type
// ***********

int CreateEntry(di_endpoint_t *diep, int tfd, BYTE *name)
{
   log_debug("CreateEntry(%s)\n",name);
   File *file = find_file(diep, tfd);
   if (!file) return ERROR_FAULT;
   if (!FindFreeSlot(diep,&file->Slot)) return ERROR_DISK_FULL;
   if (FindFreeBlock(diep,file) < 0)    return ERROR_DISK_FULL;
   strcpy((char *)file->Slot.filename,(char *)name);
   file->Slot.size = 1;
   file->Slot.type = 0x82; // PRG
   file->Slot.start_track  = file->cht;
   file->Slot.start_sector = file->chs;
   file->chp = 0;
   WriteSlot(diep,&file->Slot);
   // PrintSlot(&file->Slot);
   return ERROR_OK;
}

// *********
// PosAppend
// *********

void PosAppend(di_endpoint_t *diep, File *f)
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
   log_debug("PosAppend (%d/%d) %d\n",t,s,ns);
}

// *********
// ASCII2CBM
// *********

void ASCII2CBM(BYTE *a, BYTE *b)
{
   while (*a)
   {
      if (*a >= 'A' && *a <= 'Z') *b = *a | 0x80;
      else if (*a >= 'a' && *a <= 'z') *b = *a - 0x20;
      else *b = *a;
      ++a;
      ++b;
   }
   *b = 0;
}

// *********
// CBM2ASCII
// *********

void CBM2ASCII(BYTE *a, BYTE *b)
{
   while (*a)
   {
      if (*a >= 'A' && *a <= 'Z') *b = *a + 0x20;
      else if (*a >= 'A'+0x80 && *a <= 'Z'+0x80) *b = *a & 0x7f;
      else *b = *a;
      ++a;
      ++b;
   }
   *b = 0;
}

// ********
// OpenFile
// ********

static int OpenFile(endpoint_t *ep, int tfd, BYTE *filename, int di_cmd)
{
   int np,rv;
   File *file;
   enum boolean { FALSE, TRUE };
 
   log_info("OpenFile(..,%d,%s)\n", tfd, filename);
   di_endpoint_t *diep = (di_endpoint_t*) ep;
   file = reserve_file(diep, tfd);
 
   int file_required       = FALSE;
   int file_must_not_exist = FALSE;

   ASCII2CBM(filename,file->CBM_file);

   file->access_mode = di_cmd;
 
   switch(di_cmd)
   {
      case FS_OPEN_AP:
      case FS_OPEN_RD: file_required       = TRUE; break;
      case FS_OPEN_WR: file_must_not_exist = TRUE; break;
      case FS_OPEN_OW: break;
      default:
         log_error("Internal error: OpenFile with di_cmd %d\n", di_cmd);
         return ERROR_FAULT;
   }
   FirstSlot(diep,&file->Slot);
   np  = MatchSlot(diep,&file->Slot,file->CBM_file);
   file->next_track  = file->Slot.start_track;
   file->next_sector = file->Slot.start_sector;
   file->chp = 255;
   log_debug("File starts at (%d/%d)\n",file->next_track,file->next_sector);
   if (file_required && np == 0)
   {
     log_error("Unable to open '%s': file not found\n", filename);
     return ERROR_FILE_NOT_FOUND;
   }
   if (file_must_not_exist && np > 0)
   {
     log_error("Unable to open '%s': file exists\n", filename);
     return ERROR_FILE_EXISTS;
   }
   if (!np)
   {
      rv = CreateEntry(diep, tfd, file->CBM_file);
      if (rv != ERROR_OK) return rv;
   }
   if (di_cmd == FS_OPEN_AP) PosAppend(diep,file);
   return ERROR_OK;
}

// **********
// di_opendir
// **********

static int di_opendir(endpoint_t *ep, int tfd, const char *buf)
{
   di_endpoint_t *diep = (di_endpoint_t*) ep;
   log_debug("di_opendir(%s)\n",buf);
   File *file = reserve_file(diep, tfd);

   if (file)
   {
      if (buf && buf[0]) strcpy((char *)file->dirpattern, buf);
      else               strcpy((char *)file->dirpattern, "*");
      file->is_first = 1;
      return ERROR_OK;
   }
   else return ERROR_FAULT;
}

char *extension[6] = { "DEL","SEQ","PRG","USR","REL","CBM" };

// *********
// FillEntry
// *********

int FillEntry(BYTE *dest, slot_t *slot)
{
   char *p = (char *)dest + FS_DIR_NAME;

   log_debug("FillEntry(%s)\n",slot->filename);

   dest[FS_DIR_LEN  ] = 0;
   dest[FS_DIR_LEN+1] = slot->size;
   dest[FS_DIR_LEN+2] = slot->size >> 8;
   dest[FS_DIR_LEN+3] = 0;
   dest[FS_DIR_MODE]  = FS_DIR_MOD_FIL;
   
   strcpy(p,(const char *)slot->filename);
   CBM2ASCII((BYTE *)p,(BYTE *)p);

   dest[FS_DIR_ATTR]  = slot->type & 0x7f; // Until splat bit is correct
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
   CBM2ASCII((BYTE *)dest+FS_DIR_NAME,(BYTE *)dest+FS_DIR_NAME);
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

   dest[FS_DIR_LEN+0] = 0;
   dest[FS_DIR_LEN+1] = FreeBlocks & 0xff;
   dest[FS_DIR_LEN+2] = FreeBlocks >> 8;
   dest[FS_DIR_LEN+3] = 0;
   dest[FS_DIR_MODE]  = FS_DIR_MOD_FRE;
   dest[FS_DIR_NAME]  = 0;
   return FS_DIR_NAME + 1; 
}

// **************
// read_dir_entry
// **************

int read_dir_entry(di_endpoint_t *diep, int tfd, char *retbuf, int *eof)
{
   int rv = 0;
   File *file = find_file(diep, tfd);
   log_debug("read_dir_entry(%d)\n",tfd);

   if (!file) return -ERROR_FAULT;

   if (file->is_first == 1)
   {
      file->is_first++;
      rv = di_directory_header(retbuf,diep);
      FirstSlot(diep,&diep->Slot);
      return rv;
   }

   if (!diep->Slot.eod && MatchSlot(diep,&diep->Slot,file->dirpattern))
   {    
      rv = FillEntry((BYTE *)retbuf,&diep->Slot);
      NextSlot(diep,&diep->Slot);
      return rv;
   }

   *eof = 1;
   return di_blocks_free(retbuf,diep);
}

// ********
// ReadByte
// ********

int ReadByte(di_endpoint_t *diep, File *f, char *retbuf)
{
   if (f->chp > 253)
   {
      f->chp = 0;
      if (f->next_track == 0) return 1; // EOF
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
   // log_debug("ReadByte %2.2x\n",(BYTE)*retbuf);
   if (f->next_track == 0 && f->chp+1 >= f->next_sector) return 1;
   return 0;
}

// *********
// WriteByte
// *********

int WriteByte(di_endpoint_t *diep, File *f, BYTE ch)
{
   int oldpos;
   int block;
   BYTE zero = 0;
   // log_debug("WriteByte %2.2x\n",ch);
   if (f->chp > 253)
   {
      f->chp = 0;
      oldpos = 256 * diep->DI.LBA(f->cht,f->chs);
      block = FindFreeBlock(diep,f);
      if (block < 0) return ERROR_DISK_FULL;
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
   return ERROR_OK;
}

// *********
// read_file
// *********

static int read_file(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof)
{
   int i;
   di_endpoint_t *diep = (di_endpoint_t*) ep;
   log_debug("read_file(chan %d, len=%d)\n",tfd,len);
   File *file = find_file(diep, tfd);
   log_debug("read_file(file = %p\n",file);

   for (i=0 ; i < len ; ++i)
   {
      *eof = ReadByte(diep, file, retbuf+i);
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
   File *f = find_file(diep, tfd);

   di_fseek_tsp(diep,f->cht,f->chs,2+f->chp);
   for (i=0 ; i < len ; ++i)
      if (WriteByte(diep, f, (BYTE)buf[i])) return -ERROR_DISK_FULL;

   if (is_eof)
   {
      log_debug("Close fd=%d normally on write file received an EOF\n", tfd);
      di_close(ep, tfd);
   }
   return ERROR_OK;
}

// *************
// di_block_free
// *************

void di_block_free(di_endpoint_t *diep, BYTE Track, BYTE Sector)
{
   int   BAM_Number;     // BAM block for current track
   int   BAM_Offset;  
   int   BAM_Increment;
   BYTE *fbl;            // pointer to track free blocks
   BYTE *bam;            // pointer to track BAM
   Disk_Image_t *di = &diep->DI;

   log_debug("di_block_free(%d,%d)\n",Track,Sector);
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
   if (!(bam[Sector>>3] & (1 << (Sector & 7))))   // allocated ?
   {
      ++fbl[0];      // increase # of free blocks on track
      bam[Sector>>3] |= (1 << (Sector & 7)); // mark as free (1)
   }
}

// **********
// DeleteSlot
// **********

void DeleteSlot(di_endpoint_t *diep, slot_t *slot)
{
   BYTE t,s;
   log_debug("DeleteSlot #%d <%s>\n",slot->number,slot->filename);
   
   slot->type = 0;          // delete
   WriteSlot(diep,slot);
   t = slot->start_track ;
   s = slot->start_sector;
   while (t)
   {
      di_block_free(diep,t,s);
      di_fseek_tsp(diep,t,s,0);
      fread(&t,1,1,diep->Ip);
      fread(&s,1,1,diep->Ip);
   }
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
   ASCII2CBM((BYTE *)buf,(BYTE *)buf);
   log_debug("di_scratch(%s)\n",buf);

   *outdeleted = 0;
   FirstSlot(diep,&slot);
   do
   {
      if ((found = MatchSlot(diep,&slot,(BYTE *)buf)))
      {
         DeleteSlot(diep,&slot);
         ++(*outdeleted);
      }
   }  while (found && NextSlot(diep,&slot));
   SyncBAM(diep);
   return ERROR_SCRATCHED;  // FILES SCRATCHED message
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

   FirstSlot(diep,&slot);
   if ((found = MatchSlot(diep,&slot,(BYTE *)nameto)))
   {
      return ERROR_FILE_EXISTS;
   }

   FirstSlot(diep,&slot);
   if ((found = MatchSlot(diep,&slot,(BYTE *)namefrom)))
   {
      n = strlen(nameto);
      if (n > 16) n = 16;
      memset(slot.filename,0xA0,16); // fill filename with $A0
      memcpy(slot.filename,nameto,n);
      WriteSlot(diep,&slot);
      return ERROR_OK;
   }
   return ERROR_FILE_NOT_FOUND;
}

// *****
// di_cd
// *****

static int di_cd(endpoint_t *ep, char *buf)
{
   log_debug("di_cd %p %s\n",ep,buf);
   return ERROR_OK;
}


//***********
// di_open_rd
//***********

static int di_open_rd(endpoint_t *ep, int tfd, const char *buf)
{
   return OpenFile(ep, tfd, (BYTE *)buf, FS_OPEN_RD);
}

//***********
// di_open_wr
//***********

static int di_open_wr(endpoint_t *ep, int tfd, const char *buf,
                        const int is_overwrite)
{
  if (is_overwrite) return OpenFile(ep, tfd, (BYTE *)buf, FS_OPEN_OW);
  else              return OpenFile(ep, tfd, (BYTE *)buf, FS_OPEN_WR);
}

//***********
// di_open_ap
//***********

static int di_open_ap(endpoint_t *ep, int tfd, const char *buf)
{
       return OpenFile(ep, tfd, (BYTE *)buf, FS_OPEN_AP);
}

// **********
// di_open_rw
// **********

static int di_open_rw(endpoint_t *ep, int tfd, const char *buf)
{
   log_debug("di_open_rw(ep,%d,%s)\n",tfd,buf);
   di_endpoint_t *diep = (di_endpoint_t*) ep;
   if (*buf == '#') // ok, open a direct block channel
   {
      File *file = reserve_file(diep, tfd);
      if (!AllocateBLockBuffer(diep,file))
      {
         close_fd((di_endpoint_t *)ep,file);
         log_error("Could not reserve file\n");
         return ERROR_NO_CHANNEL;
      }
      return ERROR_OK;
   }
   return ERROR_DRIVE_NOT_READY;
}

// ***********
// di_readfile
// ***********

static int di_readfile(endpoint_t *ep, int chan, char *retbuf, int len, int *eof)
{
   int rv = 0;
   di_endpoint_t *diep = (di_endpoint_t*) ep;
   log_debug("di_readfile(%p chan=%d len=%d\n",ep,chan,len);
   
   File *f = find_file(diep, chan);

   if (f->is_first) rv = read_dir_entry(diep, chan, retbuf, eof);
   else if (f->buf) rv = read_block(ep, chan, retbuf, len, eof);
   else             rv = read_file(ep, chan, retbuf, len, eof);
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
   di_block,        // int         (*block    )(endpoint_t *ep, int chan, char *buf);
   NULL             // int         (*direct   )(endpoint_t *ep, char *buf, ...
};

