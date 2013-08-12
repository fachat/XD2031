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
 * This file provides disk image definitions
 *
 * In this file the actual command work is done for
 * Commodore disk images of type d64, d71, d80, d81, d82
 */


#ifndef DISKIMG_H
#define DISKIMG_H

#define  MAX_BUFFER_SIZE  64

#define	BLK_OFFSET_NEXT_TRACK	0
#define	BLK_OFFSET_NEXT_SECTOR	1

#define SSG_SIDE_SECTORS_MAX 	6	/* max number of side sectors in side sector group */
#define SSB_INDEX_SECTOR_MAX   	120	/* max number of blocks in a side sector */

#define SSB_OFFSET_SUPER_254   	2	/* flag when set to 254 it is a super side sector */
#define SSS_OFFSET_SSB_POINTER  3	/* start of sector group addresses in sss */
#define SSS_INDEX_SSB_MAX   	126	/* max number of super side sector blocks */

#define	SSB_OFFSET_SECTOR_NUM	2	/* side sector number field in side sector */
#define	SSB_OFFSET_RECORD_LEN	3	/* record length field in side sector */
#define	SSB_OFFSET_SSG		4	/* start of side sector addresses in current side sector group */
#define	SSB_OFFSET_SECTOR	16	/* start of sector addresses in ss */


typedef struct Disk_Image
{
   uint8_t ID;                    // Image type (64,71,80,81,82)
   uint8_t Tracks;                // Tracks per side
   uint8_t Sectors;               // Max. sectors per track
   uint8_t Sides;                 // Sides (2 for D71 and D82)
   uint8_t BAMBlocks;             // Number of BAM blocks
   uint8_t BAMOffset;             // Start of BAM in block
   uint8_t TracksPerBAM;          // Tracks per BAM
   uint8_t DirInterleave;         // Interleave on directory track
   uint8_t DatInterleave;         // Interleave on data tracks
   uint8_t HasSSB;		  // when set disk has super side blocks in REL files
   unsigned int  Blocks;          // Size in blocks
   unsigned int  RelBlocks;       // Max REL file size in blocks
   int (*LBA)(int t, int s);      // Logical Block Address calculation
   uint8_t DirTrack;              // Header and directory track
   uint8_t bamts[8];		  // up to four BAM block addresses (t/s)
} Disk_Image_t;

int diskimg_identify(Disk_Image_t *di, unsigned int filesize);

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


#endif

