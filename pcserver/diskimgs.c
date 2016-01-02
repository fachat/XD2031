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

#include <inttypes.h>

#include "log.h"
#include "diskimgs.h"
#include "stdbool.h"


/* functions for computing LBA (Logical Block Address) */
/* return -1 for illegal T/S */

static int LBA64(int t, int s)
{

   if((s < 0) || (t < 1)) return -1;                                            // T      #T  S/T
   if(t <= 17) return ((s >= 21) ? -1 : s +                         (t- 1)*21); // 01-17 (17) 21
   if(t <= 24) return ((s >= 19) ? -1 : s + 17*21 +                 (t-18)*19); // 18-24 ( 7) 19
   if(t <= 30) return ((s >= 18) ? -1 : s + 17*21 +  7*19 +         (t-25)*18); // 25-30 ( 6) 18
   if(t <= 35) return ((s >= 17) ? -1 : s + 17*21 +  7*19 +  6*18 + (t-31)*17); // 31-35 ( 5) 17
   return -1;
}


static int LBA71(int t, int s)
{
   int lba;

   if (t < 36) return LBA64(t,s);
   return (((lba = LBA64(t-35,s)) < 0) ? -1 : 683 + lba);
}

static int LBA80(int t, int s)
{

   if((s < 0) || (t < 1)) return -1;                                            // T      #T  S/T
   if(t <= 39) return ((s >= 29) ? -1 : s +                         (t- 1)*29); // 01-39 (39) 29
   if(t <= 53) return ((s >= 27) ? -1 : s + 39*29 +                 (t-40)*27); // 40-53 (14) 27
   if(t <= 64) return ((s >= 25) ? -1 : s + 39*29 + 14*27 +         (t-54)*25); // 54-64 (11) 25
   if(t <= 77) return ((s >= 23) ? -1 : s + 39*29 + 14*27 + 11*25 + (t-65)*23); // 65-77 (13) 23
   return -1;
}

static int LBA82(int t, int s)
{
   int lba;

   if (t < 78) return LBA80(t,s);
   return (((lba = LBA80(t-77,s)) < 0) ? -1 : 2083 + lba);
}

static int LBA81(int t, int s)
{
   if((s < 0) || (s > 39) || (t < 1) || (t > 80)) return -1;
  return s + (t-1) * 40;
}

// Disk image definitions
// D64: 
        /* 700 blocks + 6 side sectors */
// D81: 
        /* 3000 blocks + 4 side sector groups + 1 side sector + super side
           sector */
// D80:
        /* 720 blocks + 6 side sectors */
// D82:
        /* SFD1001: 4089 blocks + 5 side sector groups + 5 side sector +
           super side sector */
        /* 8250: 4090 blocks + 5 side sector groups + 5 side sector +
           super side sector */
        /* The SFD cannot create a file with REL 4090 blocks, but it can
           read it.  We will therefore use 4090 as our limit. */

//                          ID  Tr  Se  S  B  Of  TB  D  I  SS  Blck   Rel   map  Dir_T/S  BAM blocks                   ErrTbl
static Disk_Image_t d64 = { 64, 35, 21, 1, 1,  4, 35, 3, 11, 0,  683,  706, LBA64, 18, 1, { 18, 0,  0, 0,  0, 0,  0, 0 }, 0};
static Disk_Image_t d71 = { 71, 35, 21, 2, 2,  4, 35, 3, 11, 0, 1366,  706, LBA71, 18, 1, { 18, 0, 53, 0,  0, 0,  0, 0 }, 0};
static Disk_Image_t d81 = { 81, 80, 40, 1, 2, 16, 40, 1, 2,  1, 3200, 3026, LBA81, 40, 3, { 40, 1, 40, 2,  0, 0,  0, 0 }, 0};
static Disk_Image_t d80 = { 80, 77, 29, 1, 2,  6, 50, 3, 5,  0, 2083,  726, LBA80, 39, 1, { 38, 0, 38, 3,  0, 0,  0, 0 }, 0};
static Disk_Image_t d82 = { 82, 77, 29, 2, 4,  6, 50, 3, 5,  1, 4166, 4126, LBA82, 39, 1, { 38, 0, 38, 3, 38, 6, 38, 9 }, 0};


int diskimg_identify(Disk_Image_t *di, unsigned int filesize) {

   	if (filesize == d64.Blocks * 256) {
		*di = d64;
	} else
	if (filesize == d64.Blocks * 256 + d64.Blocks) {
		*di = d64;
		di->HasErrorTable = true;
	} else
	if (filesize == d71.Blocks * 256) {
		*di = d71;
	} else
	if (filesize == d71.Blocks * 256 + d71.Blocks) {
		*di = d71;
		di->HasErrorTable = true;
	} else
	if (filesize == d80.Blocks * 256) {
		*di = d80;
	} else
	if (filesize == d80.Blocks * 256 + d80.Blocks) {
		*di = d80;
		di->HasErrorTable = true;
	} else
	if (filesize == d82.Blocks * 256) {
		*di = d82;
	} else
	if (filesize == d82.Blocks * 256 + d82.Blocks) {
		*di = d82;
		di->HasErrorTable = true;
	} else
	if (filesize == d81.Blocks * 256) {
		*di = d81;
	} else
	if (filesize == d81.Blocks * 256 + d81.Blocks) {
		*di = d81;
		di->HasErrorTable = true;
	} else {
		log_error("Invalid/unsupported disk image\n");
		return 0; // not an image file
	}

   	return 1; // success
}

