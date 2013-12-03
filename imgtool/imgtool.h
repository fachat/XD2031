/**************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat, Nils Eilers

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301, USA.

***************************************************************************/

#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0
#endif
#endif


// Maximum number of disk images, each copied into RAM
#define MAX_IMG 6502

typedef struct {
   char *          filename;
   Disk_Image_t    di;
   uint8_t *       image;
   uint8_t *       error_table;
   unsigned int    number_of_bad_blocks;
} di_t;

typedef struct {
   int             number_of_images;   // number of images
   int             bad_images;         // number of images with bad blocks
   di_t            di[MAX_IMG];
   bool *          weak_block;         // Array of bools per track indicating
                                       // if a block holds "weak" data, that is
                                       // blocks read as good but data differs
                                       // across multiple images
} imgset_t;

typedef struct {
   int     filetype;
   uint8_t start_track;
   uint8_t start_sector;
   uint8_t ss_track;
   uint8_t ss_sector;
   uint8_t reclen;
   int     blocks;
   char    ascii_filename[16 + 1];
   char    petscii_filename[16 + 1];
} file_t;


int scandisk(di_t *di, bool testing, bool *weak);

int is_bad_block(int fdc_err);

bool scan(di_t *di, bool *weak);
