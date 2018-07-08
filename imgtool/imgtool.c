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

// imgtool: a stand-alone disk image maintenance utility

// Enable fchmod()
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "log.h"
#include "terminal.h"
#include "diskimgs.h"
#include "petscii.h"
#include "imgtool.h"
#include "relfiles.h"
#include "wildcard.h"


static char filetypes[5][4] = { "del", "seq", "prg", "usr", "rel" };

enum ACTIONS { TEST, CATALOG, DUMP };

int is_bad_block(int fdc_err) {
   if (fdc_err == 1) return 0;
   if (fdc_err == 15 || (fdc_err >= 2 && fdc_err <= 11)) return 1;
   // Unknown codes > 0 are treated as errors
   if (fdc_err) return 1;
   return 0;
}

// compute track/sector from LBA
// returns 0 on success, -1 on errors
int lba_to_ts(int lba, int (*LBA)(int t, int s), int* track, int* sector) {
   int t, s, clba;

   s = 0;
   for (t = 1; t <=154; t++) {
      clba = LBA(t, s);
      if (clba < 0 || clba > lba) break;
   }
   t--;
   for (s = 0; s <= 39; s++) {
      clba = LBA(t, s);
      if (clba < 0) return -1;
      if (clba == lba) break;
   }
   *track = t;
   *sector = s;
   return 0;
}

int read_images(imgset_t *imgs, uint8_t error_table_default, bool test_integrity) {
   for (unsigned int i = 0; i < imgs->number_of_images; i++) {
      // File exists?
      struct stat st;
      off_t filesize;
      if (stat (imgs->di[i].filename, &st)) {
         log_errno("%s", imgs->di[i].filename);
         return -1;
      }

      // Is a supported disk image?
      filesize = st.st_size;
      if (!(diskimg_identify(&imgs->di[i].di, filesize))) {
         log_error("%s is not a supported image file\n", imgs->di[i].filename);
         return -1;
      }

      // Read image and error table
      if(imgs->di[i].di.HasErrorTable) imgs->bad_images++;
      imgs->di[i].image = malloc(imgs->di[i].di.Blocks * 256 + imgs->di[i].di.Blocks);
      if(imgs->di[i].image == NULL) {
         log_error("malloc failed!\n");
         return -1;
      }
      imgs->di[i].error_table = imgs->di[i].image + imgs->di[i].di.Blocks * 256;
      int fd = open(imgs->di[i].filename, O_RDONLY | O_BINARY);
      if (fd == -1) {
         log_errno("Unable to open %s", imgs->di[i].filename);
         return -1;
      }
      ssize_t bytes_read_total = 0;
      while(bytes_read_total < filesize) {
         ssize_t bytes_read = read(fd, imgs->di[i].image + bytes_read_total, filesize);
         log_debug("Read returned: %ld\n", bytes_read);
         if (bytes_read < 0) {
            log_errno("Read %s", imgs->di[i].filename);
            return -1;
         }
         bytes_read_total += bytes_read;
      }
      // Clear error table if image has none
      if (!imgs->di[i].di.HasErrorTable) memset (imgs->di[i].error_table, error_table_default, imgs->di[i].di.Blocks);
      // Count bad blocks
      imgs->di[i].number_of_bad_blocks = 0;
      for(unsigned int j = 0; j < imgs->di[i].di.Blocks; j++) {
         if (is_bad_block(imgs->di[i].error_table[j])) {
            imgs->di[i].number_of_bad_blocks++;
         }
      }

      // Show summary and check files
      if (test_integrity) {
         log_info("image type: D%d\n", imgs->di[i].di.ID);
         log_info("filename: %s\n", imgs->di[i].filename);
         log_info("filesize: %lu bytes\n", (long int) filesize);
         if (imgs->di[i].di.HasErrorTable)
            log_info("Image has error table appended\n");
         else
            log_info("No error table appended\n");
         log_info("%u of %u (%.2f%%) blocks %s bad\n",
                 imgs->di[i].number_of_bad_blocks, imgs->di[i].di.Blocks,
                 (float) 100 * imgs->di[i].number_of_bad_blocks / imgs->di[i].di.Blocks,
                 imgs->di[i].number_of_bad_blocks == 1 ? "is" : "are");

         // List bad blocks
         for(unsigned int j = 0; j < imgs->di[i].di.Blocks; j++) {
            if (is_bad_block(imgs->di[i].error_table[j])) {
               int t, s;
               lba_to_ts(j, imgs->di[i].di.LBA, &t, &s);
               log_warn("Bad block (LBA: %4u  T: %3u  S: %2u  O: %06lX): error %3u\n", 
                     j, t, s, (unsigned long) j * 256, imgs->di[i].error_table[j]); 
            }
         }
         // Check files integrity
         scan(&imgs->di[i], NULL);
      }
   }
   return 0;
}

int merge_repair(imgset_t *imgs, char *outfilename, int preserve_table, uint8_t weak_block_entry, 
                 di_t **mdi, bool **weak) {
   unsigned int i, j, b;
   int compares, differs, t, s;
   int rv = 0;

   (void)weak; // silence warning unused parameter

   if(imgs->number_of_images < 2) {
      log_error("Merge repair requires at least two images\n");
      return -1;
   }

   // Make sure, all images are of the same type
   for (i = 1; i < imgs->number_of_images; i++) {
      if (imgs->di[i].di.ID != imgs->di[0].di.ID) {
         log_error("Merge repair requires images of same type\n");
         return -1;
      }
   }

   log_info("------------------------------ MERGE REPAIR ------------------------------\n");
   log_info("Note: further messages refer to the merged image.\n");

   if(!imgs->bad_images) {
      log_warn("All images are marked as good. If it ain't broke, don't fix it.\n");
   }
   if(!outfilename) {
      log_warn("No outfilename given, performing a dry-run\n");
   }

/* Check equality of good blocks

   If there are several images of the same disk, blocks that are marked as good
   should be identical. In reality, this is not always true. There seem
   to be some weak blocks where CBM DOS fails on recognizing them as bad.
   Having several images, we select the variant that was read the most often,
   hoping that the data would read out correctly in most cases and failing only
   sometimes. To be able to select the most common variant of a block, each block
   of each image has a score stored in a dynamic two-dimensional array
   increasing on matching compares.
*/
   int **score = (int**) calloc(imgs->number_of_images, sizeof(int *));
   if(score == NULL) {
      log_error("calloc failed\n");
      return -1;
   }
   for(unsigned int i=0; i < imgs->number_of_images; i++) {
      score[i] = (int *) calloc (imgs->di[0].di.Blocks, sizeof(int));
      if(score[i] == NULL) {
         log_error("calloc failed\n");
         return -1;
      }
   }

   if(imgs->weak_block == NULL) {
      imgs->weak_block = calloc(imgs->di[0].di.Blocks, sizeof(bool));
      if (imgs->weak_block == NULL) {
         log_error("calloc failed\n");
         return -1;
      }
   }

   compares = 0; differs = 0;
   log_info("Verifying equality of good blocks... \n");
   // FIXME: if the \n is removed from the line above, an ERR: should start at a new line
   for (b = 0; b < imgs->di[0].di.Blocks; b++) {
     for (i = 0; i < imgs->number_of_images - 1; i++) {
         for (j = i + 1; j < imgs->number_of_images; j++) {
            compares++;
            if ((imgs->di[i].error_table[b] == 1) && (imgs->di[j].error_table[b] == 1)) {
                  if(memcmp(imgs->di[i].image + b * 256, imgs->di[j].image + b * 256, 256)) {
                     differs++;
                     imgs->weak_block[b] = true;
                     lba_to_ts(b, imgs->di[0].di.LBA, &t, &s);
                     log_error("Block %4u (T: %3u  S: %2u  O: %06lX): %s %s differ\n",
                           b, t, s, b * 256, imgs->di[i].filename, imgs->di[j].filename);
                  } else {
                     score[i][b]++;
                     score[j][b]++;
                  }
            }
         }
      }
   }
   if (differs) {
      int weaks = 0;
      for (b = 0; b < imgs->di[0].di.Blocks; b++) if (imgs->weak_block[b]) weaks++;
      log_error("%u of %u (%.2f%%) compares did not match, %d weak blocks\n",
                 differs, compares, (float) 100 * differs / compares, weaks);

   }
   else log_info("OK (%u compares)\n", compares);

   // Create empty merged image
   uint8_t* merged = calloc(imgs->di[0].di.Blocks * 256 + imgs->di[0].di.Blocks, 1);
   if(!merged) {
      log_error("calloc failed\n");
      return -1;
   }
   uint8_t* merged_table = merged + imgs->di[0].di.Blocks * 256;

   // Merge images
   for (b = 0; b < imgs->di[0].di.Blocks; b++) {
      // If this is a weak block, grab one with highest score
      if (imgs->weak_block[b]) {
         log_debug("Block %u differs across images\n", b);
         int hiscore = 0;
         for (i = 0; i < imgs->number_of_images; i++) {
            log_debug("%s\tscore: %u\n", imgs->di[i].filename, score[i][b]);
            if (score[i][b] > hiscore) hiscore = score[i][b];
         }
         log_debug("Highest score: %d\n", hiscore);
         for (i = 0; i < imgs->number_of_images; i++) {
            if (score[i][b] == hiscore) {
               log_info("Block %4u: ", b);
               if (!hiscore) {
                  log_info("Sorry, there is no 'best' block, picking from first image %s\n", imgs->di[i].filename);
               } else {
                  log_info("Block %4u: picking data from image with best score (%d): %s\n",
                         b, hiscore, imgs->di[i].filename);
               }
               memcpy(merged + b * 256, imgs->di[i].image + b * 256, 256);
               merged_table[b] = weak_block_entry;
               break;
            }
         }
         continue;
      }

      // Best is a block that has been actually read and marked as good
      bool block_found = false;
      for (i = 0; i < imgs->number_of_images; i++) {
         if (imgs->di[i].error_table[b] == 1) {
            memcpy(merged + b * 256, imgs->di[i].image + b * 256, 256);
            merged_table[b] = 1;
            block_found = true;
            break;
         }
      }
      // Next best option is a block that is marked as good
      // (but is not necessarily really read)
      if (!block_found) {
         for (i = 0; i < imgs->number_of_images; i++) {
            if (!is_bad_block(imgs->di[i].error_table[b])) {
               memcpy(merged + b * 256, imgs->di[i].image + b * 256, 256);
               merged_table[b] = imgs->di[i].error_table[b];
               block_found = true;
               break;
            }
         }
      }
      // Last option: take the next best bad block
      if(!block_found) {
         memcpy(merged + b * 256, imgs->di[0].image + b * 256, 256);
         merged_table[b] = imgs->di[0].error_table[b];
      }
   }


   // Count remaining bad blocks
   int bad_blocks_left = 0;
   for (i = 0; i < imgs->di[0].di.Blocks; i++) if (is_bad_block(merged_table[i])) bad_blocks_left++;
   if (bad_blocks_left) {
      log_info("Sorry, there are still %u bad blocks (%.2f%%)\n", bad_blocks_left, (float) 100 * bad_blocks_left / imgs->di[0].di.Blocks);
   } else {
      log_info("Congrats, the image could get repaired.\n");
   }

   // Write image
   if (outfilename) {
      int fd = open(outfilename, O_WRONLY | O_CREAT | O_CREAT | O_BINARY);
      if (fd == -1) {
         log_errno("Unable to create %s", outfilename);
         return -1;
      }
#ifndef _WIN32
      // Set permissions to rw-r--r--
      if(fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) {
         log_errno("Unable to change permissions for %s\n", outfilename);
         rv = -1;
      }
#endif
      ssize_t imgsize = imgs->di[0].di.Blocks * 256;
      if (bad_blocks_left) log_info("Writing image with error table appended\n");
      else if (preserve_table) log_info("Writing good image, preserving error table\n");
      else log_info("Writing good image without error table appended\n");
      if (bad_blocks_left || preserve_table) imgsize += imgs->di[0].di.Blocks;
      ssize_t bytes_written_total = 0;
      while (bytes_written_total < imgsize) {
         ssize_t bytes_written = write(fd, merged + bytes_written_total, imgsize);
         log_debug("write() returned: %ld\n", bytes_written);
         if (bytes_written == -1) {
            log_errno("Error on writing image %s", outfilename);
            rv = -1;
            break;
         }
         bytes_written_total += bytes_written;
      }
      if (close(fd)) {
         log_errno("Error on closing image file\n");
         rv = -1;
      }
   }

   if (outfilename == NULL) outfilename = "Merged image";
   (**mdi).filename = outfilename;                 // filename
   (**mdi).di = imgs->di[0].di;                    // Disk_Image_t definitions
   (**mdi).image = merged;                         // contents
   (**mdi).error_table = merged_table;             // error table
   (**mdi).number_of_bad_blocks = bad_blocks_left; // number of bad blocks

   scan(*mdi, imgs->weak_block);

   for (i = 0; i < imgs->number_of_images; free(score[i++]));
   free(score);
   return rv;
}

void extract_name(char* dest, const uint8_t *src, bool petscii_conversion) {
   if (petscii_conversion)
      for (int i = 0; i < 16; i++) dest[i] = petscii_to_ascii(src[i]);
   else
      for (int i = 0; i < 16; i++) dest[i] = src[i];
   for (int i = 15; i > 0; i--) 
      if ((uint8_t) dest[i] == 0xA0)
         dest[i] = 0;
      else break;
   dest[16] = 0;
}

int follow_link_chain(di_t *di, uint8_t t, uint8_t s, int action, bool *weak, char *filename, int maxblocks) {
   // FIXME: out of memory on circular references
   log_debug("Following link chain of '%s'\n", filename);
   int b = 1;
   int err;
   int lba;
   bool weak_warned = false;
   bool faulty = false;
   uint8_t *contents = NULL;
   int linked_blocks = 0; // number of actually linked blocks

   if (action == DUMP) {
      contents = malloc(maxblocks * 254);
      if (contents == NULL) {
         log_error("malloc failed\n");
         return 1;
      }
   }

   for(;;) {
      lba = di->di.LBA(t, s);
      if (lba < 0) err = -1;
      else err = di->error_table[lba];
      log_debug("Block %04d/%04d (LBA: %4d  T: %3d  S: %2d  O: %06lX) E: %d\n",
            b, maxblocks, lba, t, s, (unsigned long) lba * 256, err);
      if (lba < 0) {
         faulty = true;
         log_error("\"%s\" linked to invalid T: %d S: %d\n", filename, t, s);
         break;
      }

      if (++linked_blocks > maxblocks) {
         log_warn("Size of %s exceeds %d blocks\n", filename, maxblocks);
         contents = realloc(contents, linked_blocks * 254);
         if (contents == NULL) {
            log_error("realloc failed\n");
            return 1;
         }
      }

      if (is_bad_block(err)) {
         faulty = true;
         if (action == TEST) 
            log_error("\"%s\" corrupted by bad block (error %d)."
                      " %d of %d blocks available.\n",
                      filename, err, b-1, maxblocks);
         if (action == CATALOG) {
            color_log_error();
            if (weak_warned) printf("/ ");
            printf("BAD (%d/%d good) ", b-1, maxblocks);
            color_default();
         }
         break;
      }

      if (weak != NULL) if (weak[lba]) {
         faulty = true;
         if (!weak_warned) {
            if (action == TEST) {
                  log_error("\"%s\" differs across images\n", filename);
            }
            if (action == CATALOG) {
               color_log_error();
               printf("WEAK ");
               color_default();
            }
            weak_warned = true;
         }
      }

      if (action == DUMP) memcpy(contents + 254 * (linked_blocks - 1), di->image + lba * 256 + 2, 254);

      t = di->image[lba * 256    ];
      s = di->image[lba * 256 + 1];
      b++;
      if (!t) break;
   }

   // Check directory block size against linked size for good files
   if (!faulty && linked_blocks != maxblocks)
      log_warn("\"%s\": directory says %d blocks, but link chain gives %d blocks\n",
            filename, maxblocks, linked_blocks);

   if (action == DUMP) {
      log_info("Dump of \"%s\":\n", filename);
      log_hexdump((char*) contents, linked_blocks * 254, 1);
   }

   free(contents);
   return (faulty ? -1 : 0);
}

bool dirwalk(di_t *di, bool *weak, char *filemask, void *common,
            bool (*action)(di_t *di, bool *weak, file_t *file, void *common)) 
{
   int t = di->di.DirTrack;
   int s = di->di.DirSector;
   int lba;
   int offset = 0;
   uint8_t *p;
   int next_track, next_sector;
   file_t f;
   bool faulty = false;
   int matches = 0;

   for(;;) {
      lba = di->di.LBA(t,s);
      if (lba < 0)
      {
         log_error("Illegal track or sector (t=%d, s=%d), aborting dir walk\n", t, s);
         faulty++;
         break;
      }

      if(is_bad_block(di->error_table[lba])) {
         faulty = true;
         log_error("Directory corrupt\n");
         // TODO: try to continue
         break;
      }
      if(weak != NULL) if (weak[lba]) {
         faulty = true;
         log_error("Directory differs across images\n");
      }

      p = di->image + lba * 256 + offset;

      if (offset == 0) {
         next_track = p[0];
         next_sector = p[1];
      }

      f.filetype     = p[2];
      f.start_track  = p[3];
      f.start_sector = p[4];
      f.ss_track     = p[0x15];
      f.ss_sector    = p[0x16];
      f.reclen       = p[0x17];
      f.blocks       = p[0x1e] | p[0x1f] << 8;
      extract_name(f.petscii_filename, p + 5, false);
      extract_name(f.ascii_filename,   p + 5, true);

      if (f.filetype) {
         if (compare_pattern(f.ascii_filename, filemask, true)) {
            matches++;
            if (action(di, weak, &f, common)) faulty = true;
         }
      }

      offset += 0x20;
      if (offset > 0xe0) {
         if (next_track == 0) break;
         t = next_track;
         s = next_sector;
         offset = 0;
      }
   }

   if(!matches) log_error("No files matched '%s'\n", filemask);

   return faulty;
}

typedef struct {
   int number_of_files;
   int blocks_used;
} catalog_t;

static bool catalog_file(di_t *di, bool *weak, file_t *f, void *common) {
   catalog_t *c = common;
   c->number_of_files++;
   c->blocks_used += f->blocks;
   bool faulty = false;
   uint8_t *p;
   int load_addr = -1;

   printf("%-4d \"%s\"", f->blocks, f->ascii_filename);
   int spaces = 16 - strlen(f->ascii_filename);
   while (spaces--) putchar(' ');
   printf("%c%s%c", 
            f->filetype & 64 ? '>' : ' ', 
            filetypes[f->filetype & 7], 
            f->filetype & 127 ? ' ' : '*' );

   switch(f->filetype & 0x0f) {
      case 2: // PRG
         p = di->image + di->di.LBA(f->start_track, f->start_sector) * 256 + 2;
         load_addr = p[0] | p[1] << 8;
      case 1: // SEQ
      case 3: // USR
         if   (load_addr > 0) printf(" $%04X ", load_addr);
         else                 printf("       ");
         if(follow_link_chain(di, f->start_track, f->start_sector, CATALOG,
            weak, f->ascii_filename, f->blocks)) 
         {
            faulty = true;
         }
         break;
      case 4: // REL
         if (process_relfile(di, f->start_track, f->start_sector, f->ss_track,
            f->ss_sector, f->reclen, false, false, weak, f->ascii_filename))
         {
            faulty = true;
         }
         break;
      default:
         log_error("Filetype %d ignored\n", f->filetype);
         faulty = true;
   }
   printf("\n");
   return faulty;
}


bool catalog(di_t *di, bool *weak, char *filemask) {
   catalog_t c;
   char diskname[16 + 1];
   char diskid[]  = "  ";
   char diskdos[] = "  ";
   bool faulty = false;
   uint8_t *p;

   memset(&c, 0, sizeof c);

   // Get disk name, disk ID and DOS version
   switch(di->di.ID) {
      case 80:
      case 82:
         p = di->image + di->di.LBA(39,0) * 256 + 6;
         extract_name(diskname, p, 1);
         p = di->image + di->di.LBA(39,0) * 256 + 24;
        break;
      case 81:
         p = di->image + di->di.LBA(40,0) * 256 + 4;
         extract_name(diskname, p, 1);
         p = di->image + di->di.LBA(40,0) * 256 + 22;
         break;
      case 64:
      case 71:
         p = di->image + di->di.LBA(18,0) * 256 + 0x90;
         extract_name(diskname, p, 1);
         p = di->image + di->di.LBA(18,0) * 256 + 0xa2;
         break;
      default:
         log_error("Internal error: unknown disk ID %d\n", di->di.ID);
         return true;
   }
   diskid[0] = petscii_to_ascii(*p++);
   diskid[1] = petscii_to_ascii(*p++);
   p++;
   diskdos[0] = petscii_to_ascii(*p++);
   diskdos[1] = petscii_to_ascii(*p++);

   // Fill disk name with spaces up to length 16
   while (strlen(diskname) < 16) strcat(diskname, " ");

   printf("%s \"%s\" %s %s\n", di->filename, diskname, diskid, diskdos);

   faulty = dirwalk(di, weak, filemask, &c, catalog_file);
   if (c.blocks_used || c.number_of_files)
      printf("%d blocks used by %d files.\n\n", c.blocks_used, c.number_of_files);

   return faulty;
}


static bool scan_file(di_t *di, bool *weak, file_t *f, void *common) {
   catalog_t *c = common;
   c->number_of_files++;
   c->blocks_used += f->blocks;
   bool faulty = false;

   switch(f->filetype & 0x0f) {
      case 1: // SEQ
      case 2: // PRG
      case 3: // USR
         if(follow_link_chain(di, f->start_track, f->start_sector, TEST, weak, f->ascii_filename, f->blocks))
            faulty = true;
         break;
      case 4: // REL
         if(process_relfile(di, f->start_track, f->start_sector, f->ss_track, f->ss_sector,
                  f->reclen, true, false, weak, f->ascii_filename))
            faulty = true;
         break;
      default:
         log_error("Filetype %d ignored\n", f->filetype);
         faulty = true;
   }
   return faulty;
}

bool scan(di_t *di, bool *weak) {
   catalog_t c;

   memset(&c, 0, sizeof c);

   return(dirwalk(di, weak, "*", &c, scan_file));
}

static bool dump_file(di_t *di, bool *weak, file_t *f, void *common) {
   bool faulty = false;

   (void)common; // silence warning unused parameter;

   switch(f->filetype & 0x0f) {
      case 1: // SEQ
      case 2: // PRG
      case 3: // USR
         if(follow_link_chain(di, f->start_track, f->start_sector, DUMP, weak, f->ascii_filename, f->blocks))
            faulty = true;
         break;
      case 4: // REL
         if(process_relfile(di, f->start_track, f->start_sector, f->ss_track, f->ss_sector,
                  f->reclen, false, true, weak, f->ascii_filename))
            faulty = true;
         break;
      default:
         log_error("Filetype %d ignored\n", f->filetype);
         faulty = true;
   }
   return faulty;
}


bool dump(di_t *di, char *filemask) {
   return dirwalk(di, NULL, filemask, NULL, dump_file);
}


void show_version(void)
{
   printf("imgtool version 2017-01-06\n");
   exit(EXIT_SUCCESS);
}


void usage(void) {
   printf("usage:\n"
          "\timgtool [options] diskimage ...\n"
          "\n"
          "options:\n"
          "\n"
          "\t-c\t\tCatalog, show directory\n"
          "\t-d\t\tHexdump file contents\n"
          "\t-h\t\tThis help text\n"
          "\t-m\t\tMerge repair collects good blocks from images\n"
          "\t-M filemask\tProcess only files matching filemask\n"
          "\t-o diskimage\tfilename for output\n"
          "\t-p\t\tPreserve error table even if there are no errors left\n"
          "\t-R\t\tTreat blocks from images without error table\n"
          "\t\t\tas if they were all read\n"
          "\t-W\t\tMark weak blocks in error table with $FF\n"
          "\t-v\t\tMore verbose output\n"
          "\t-V\t\tShow version\n"
          );
}

int main (int argc, char* argv[]) {
   int8_t   option_error_table_default  = 0;
   bool     option_preserve_error_table = false;
   bool     option_merge_repair         = false;
   uint8_t  option_weak_block_entry     = 1;
   bool     option_catalog              = false;
   char *   outfilename                 = NULL;
   char *   filemask                    = "*";
   bool     option_dump                 = false;
   imgset_t imgs;
   bool     faulty_image                = false;
   di_t *   img                         = &imgs.di[0];

   memset(&imgs, 0, sizeof imgs);

   terminal_init();

   if (argc < 2) {
      usage();
      return 1;
   }

   imgs.number_of_images = 0;
   for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         // Process options
         if (strlen(argv[i]) < 2) {
            log_error("Expected option after '-'\n");
            return 1;
         }
         int len = strlen(argv[i]);
         for (int j = 1; j < len; j++) switch (argv[i][j]) {
            case 'c':
               option_catalog = true;
               break;

            case 'd':
               option_dump = true;
               break;

            case 'h':
               usage();
               return 0;

            case 'm':
               option_merge_repair = true;
               break;

            case 'M':
               if (len - 1 > j) {
                  log_error("When combining options, -M has to be the last one\n");
                  return 1;
               }
               if (i < argc - 1) {
                  filemask = argv[++i];
               } else {
                  log_error("No filemask given\n");
                  return 1;
               }
               break;

            case 'o':
               if (len - 1 > 1) {
                  log_error("When combining options, -o has to be the last one\n");
                  return 1;
               }
               if (i < argc - 1) {
                  outfilename = argv[++i];
               } else {
                  log_error("No outfilename given\n");
                  return 1;
               }
               break;

            case 'p':
               option_preserve_error_table = true;
               break;

            case 'R':
               option_error_table_default = true;
               break;

            case 'v':
               set_verbose(1);
               break;

            case 'V':
               show_version();
               break;


            case 'W':
               option_weak_block_entry = 0xff;
               break;

            default:
               log_error("Unknown option -%c\n", argv[i][j]);
               return 1;
         }
      } else {
         // Process images
         if (imgs.number_of_images == MAX_IMG) {
            log_error("Sorry, can't take more than %d disk images. Ignoring surplus %s.\n", MAX_IMG, argv[i]);
         } else {
            imgs.di[imgs.number_of_images++].filename = argv[i];
         }
      }
   }

   // Any disk images to process?
   if (!imgs.number_of_images) {
      log_error("No disk images given\n");
      return 2;
   }

   // FIXME: test images on read, return 1 if they are bad
   // exit with 2 if there were hard errors (file not found etc.)

   // CATALOG scans images later; scan now if no catalog requested
   if (read_images(&imgs, option_error_table_default, option_catalog ? false : true))
      return 2;

   // If images are merged, further operations act upon the merged image
   if (option_merge_repair) {
      if (merge_repair(&imgs, outfilename, option_preserve_error_table, option_weak_block_entry,
                       &img, &imgs.weak_block))
         faulty_image = true;
   }

   unsigned int current_image = 0;
   for(;;) {
      // Show directory
      if (option_catalog) catalog(img, imgs.weak_block, filemask);

      // Hexdump files
      if (option_dump) dump(img, filemask);

      // Grab next image if we didn't merge them
      if(option_merge_repair) break;
      if(++current_image == imgs.number_of_images) break;
      img = &imgs.di[current_image];
   }


   return (faulty_image ? 1 : 0);
}
