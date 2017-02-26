/***************************************************************************

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

***************************************************************************/

// TODO:
// - skip or skip not hidden files
// - allow wildcards for OPEN ("LOAD *")
// - allow wildcards for CD/RM etc.

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "packet.h"
#include "wireformat.h"
#include "dirconverter.h"
#include "charconvert.h"
#include "debug.h"
#include "fatfs/ff.h"
#include "sdcard.h"
#include "fat_provider.h"
#include "dir.h"
#include "config.h"
#include "petscii.h"
#include "errors.h"
#include "ff.h"
#include "errcompat.h"
#include "wildcard.h"

#define  DEBUG_FAT

// ----- Glue to firmware --------------------------------------------------

static uint8_t current_charset;

static void *prov_assign(uint8_t drive, const char *petscii_parameter);
static void prov_free(void *epdata);
static void fat_submit(void *epdata, packet_t *buf);
static void fat_submit_call_data(
   void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
   uint8_t (*callback)(int8_t channelno, int8_t errnum, packet_t *packet));
static void fat_submit_call_cmd(
   void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf, rtconfig_t *rtconfig,
   uint8_t (*callback)(int8_t channelno, int8_t errnum, packet_t *packet));

static charset_t charset(void *epdata) {
   return current_charset;
}

static void set_charset(void *epdata, charset_t new_charset) {
   current_charset = new_charset;
}

provider_t fat_provider  = {
   prov_assign,
   prov_free,
   charset,
   set_charset,
   fat_submit,
   fat_submit_call_data,
   fat_submit_call_cmd,
   directory_converter,
   NULL,                        // channel_get
   NULL                         // channel_put
};


// ----- Private provider data ---------------------------------------------

// fat_provider_init() called at first assign
static uint8_t fat_provider_initialized = false;

static FATFS Fatfs[_VOLUMES];   // File system object for each logical drive

#define AVAILABLE -1
enum enum_dir_state { DIR_INACTIVE, DIR_HEAD, DIR_FILES, DIR_FOOTER };

// Channel table (holds dir state/file data)
static struct {
   int8_t chan;          // entry used by channel # or AVAILABLE
   int8_t dir_state;     // DIR_INACTIVE for files
                         // or DIR_* when reading directories
   FIL    f;             // file data
} tbl[FAT_MAX_FILES];

// Each drive has a current directory
#ifndef FAT_MAX_ASSIGNS
#   define FAT_MAX_ASSIGNS 2
#endif
typedef struct {
   int8_t drive;                   // AVAILABLE when not assigned
   TCHAR cwd[_MAX_LFN + 1];        // current working directory
} fat_assign_t;
static fat_assign_t fat_assign[FAT_MAX_ASSIGNS];
static void* last_epdata = NULL;   // Used to detect if f_chdir needed,
                                   // points to an entry in fat_assign[]

// Used by fat_submit_call/FS_OPEN_DR to pass values to fs_read_dir()
// Don't try to read several directories at once
static struct {
   DIR    D;                       // work area for f_opendir()/f_readdir()
   uint8_t drive;                  // CBM drive number
   char   mask[_MAX_LFN + 1];      // search mask for files
   char   headline[16 + 1];        // headline of directory listing
} dir;


// ----- Prototypes --------------------------------------------------------

// helper functions
static cbm_errno_t fs_read_dir(void *epdata, int8_t channelno, bool advanced_wildcards, packet_t *packet);
static cbm_errno_t fs_move(char *buf);
static void fs_delete(char *path, packet_t *p, bool advanced_wildcards);

// Copy a filename/path limited to 16 characters
// If the path is too long, copy only the last characters preceeded by ".."
static void shortname(char* dest, const char* src) {
  uint8_t len = strlen(src);
  if(len  > 16) {
     memmove(dest + 2, src + len - 14, 14);
     dest[0] = '.';
     dest[1] = '.';
 } else strcpy(dest, src);
 dest[16] = 0;
}


// ----- File / Channel table ----------------------------------------------

static void tbl_init(void) {
   for(uint8_t i=0; i < FAT_MAX_FILES; i++) {
      tbl[i].chan = AVAILABLE;
      tbl[i].dir_state = DIR_INACTIVE;
   }
}

static int8_t tbl_chpos(uint8_t chan) {
   for(uint8_t i=0; i< FAT_MAX_FILES; i++) if(tbl[i].chan == chan) return i;
   return -1;
}

static int8_t get_dir_state(uint8_t chan) {
   int8_t pos = tbl_chpos(chan);
   if(pos < 0 ) return pos;
   return tbl[pos].dir_state;
}

static FIL *tbl_ins_file(uint8_t chan) {
   for(uint8_t i=0; i < FAT_MAX_FILES; i++) {
      if(tbl[i].chan == chan) {
         debug_printf("#%d already exists @%d\n", chan, i);
         tbl[i].dir_state = DIR_INACTIVE;
         return &tbl[i].f;
      }
      if(tbl[i].chan == AVAILABLE) {
         tbl[i].chan = chan;
         tbl[i].dir_state = DIR_INACTIVE;
         debug_printf("#%d found in file table @ %d\n", chan, i);
         return &tbl[i].f;
      }
   }
   debug_printf("tbl_ins_file: out of mem #%d\n", chan);
   return NULL;
}

static cbm_errno_t tbl_ins_dir(int8_t chan) {
   for(uint8_t i=0; i < FAT_MAX_FILES; i++) {
      if(tbl[i].chan == chan) {
         debug_printf("dir_state #%d := DIR_HEAD\n", i);
         tbl[i].dir_state = DIR_HEAD;
         return CBM_ERROR_OK;
      }
      if(tbl[i].chan == AVAILABLE) {
         debug_printf("@%d initialized with DIR_HEAD\n", i);
         tbl[i].chan = chan;
         tbl[i].dir_state = DIR_HEAD;
         return CBM_ERROR_OK;
      }
   }
   debug_printf("tbl_ins_dir: out of mem #%d\n", chan);
   return CBM_ERROR_NO_CHANNEL;

}
static FIL *tbl_find_file(uint8_t chan) {
   int8_t pos;

   if((pos = tbl_chpos(chan)) < 0) {
      debug_printf("tbl_find_file: #%d not found!\n", chan);
      return NULL;
   }
   debug_printf("#%d found @%d\n", chan, pos);
   return &tbl[pos].f;
}

static cbm_errno_t tbl_close_file(uint8_t chan) {
   int8_t pos;
   cbm_errno_t cres = CBM_ERROR_OK;
   FRESULT fres = FR_OK;

   if((pos = tbl_chpos(chan)) != AVAILABLE) {
      if(tbl[pos].dir_state == DIR_INACTIVE) {
         fres = f_close(&tbl[pos].f);
         debug_printf("f_close (#%d @%d): %d\n", chan, pos, fres);
      } else {
         fres = f_closedir(&dir.D);
         debug_printf("f_closedir: %d\n", fres);
      }
      tbl[pos].chan = AVAILABLE;
      tbl[pos].dir_state = DIR_INACTIVE;
   } else {
      debug_printf("f_close (#%d): nothing to do\n", chan);
   }
   return combine(cres, fres);
}

// ----- Provider routines -------------------------------------------------

static void fat_provider_init(void) {
   uint8_t res;

   debug_puts("fat_provider_init()\n");
   for(uint8_t i=0; i < FAT_MAX_ASSIGNS; i++) {
      fat_assign[i].drive = AVAILABLE;
      fat_assign[i].cwd[0] = 0;
   }
   res = disk_initialize(0);
   debug_printf("disk_initialize: %u", res); debug_putcrlf();
   res = f_mount(&Fatfs[0], "", 1);
   if(res) debug_printf("f_mount: %u\n", res);
   tbl_init();
   fat_provider_initialized = true;
}

static void *prov_assign(uint8_t drive, const char *petscii_parameter) {
   int8_t res;
   TCHAR cwd[_MAX_LFN + 1];
   char parameter[64];

   if(!fat_provider_initialized) fat_provider_init();

   // prov_assign receives PETSCII, translate to ASCII first
   strcpy(parameter, petscii_parameter);
   char *p = parameter;
   while(*p) {
      *p = petscii_to_ascii(*p);
      p++;
   }

   debug_printf("fat_prov_assign: drv=%u par=%s\n", drive, parameter);
   for(uint8_t i=0; i < FAT_MAX_ASSIGNS; i++) {
      if(fat_assign[i].drive == AVAILABLE || fat_assign[i].drive == drive) {
         // Check if parameter is something CHDIRable
         if((res = f_getcwd(cwd, sizeof(cwd) - 1))) {
            // Save current directory first
            debug_printf("f_getcwd: %d\n", res);
            return NULL;
         }
         if((res = f_chdir(parameter))) {
            debug_printf("f_chdir(%s): %d\n", parameter, res);
            return NULL;
         } else {
            // CHDIR success, restore current directory
            if((res = f_chdir(cwd))) {
               debug_printf("f_chdir(%s): %d\n", cwd, res);
               return NULL;
            }
         }
         strncpy(fat_assign[i].cwd, parameter, sizeof(fat_assign[i].cwd)-1);
         fat_assign[i].cwd[sizeof(fat_assign[i].cwd) - 1] = 0;
         fat_assign[i].drive = drive;
         debug_printf("fat_assign at %u p=%p\n", i, &fat_assign[i]);
         return &fat_assign[i];
      }
   }
   debug_puts("out of assign slots!\n");
   return NULL;
}

static void prov_free(void *epdata) {
   debug_printf("prov_free(%p)\n", epdata);
   // free the ASSIGN-related data structure
   fat_assign_t* p = (fat_assign_t*) epdata;
   p->drive = AVAILABLE;
   p->cwd[0] = 0;
   last_epdata = NULL; // force chdir
}

static void fat_submit(void *epdata, packet_t *buf) {
   // submit a fire-and-forget packet (log_*, terminal)
   // not applicable for storage ==> dummy
}

static void fat_submit_call_data(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf, 
   uint8_t (*callback)(int8_t channelno, int8_t errnum, packet_t *packet))
{
   // TODO: this breaks when reading the directory, as there rtconfig is needed for some reason
   fat_submit_call_cmd(epdata, channelno, txbuf, rxbuf, NULL, callback);
}

static void fat_submit_call_cmd(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf, rtconfig_t *fatfs_rtc,
   uint8_t (*callback)(int8_t channelno, int8_t errnum, packet_t *packet))
{
   // submit a request/response packet; call the callback function when the
   // response is received; If callback returns != 0 then the call is kept
   // open, and further responses can be received

   int8_t fres = FR_OK;
   int8_t cres = CBM_ERROR_OK;
   int8_t reply_as_usual = true;
   UINT transferred = 0;
   FIL *fp;
   char *path = (char *) (txbuf->buffer + 1);
   uint8_t len = txbuf->len - 1;

   FILINFO Finfo;  // holds file information returned by f_readdir/f_stat
                   // the long file name *lfname must be stored externally:
#  ifdef _USE_LFN
      char Lfname[_MAX_LFN+1];
      Finfo.lfname = Lfname;
      Finfo.lfsize = sizeof Lfname;
#  endif

   debug_printf("fat_submit_call epdata=%p\n", epdata);

   if(epdata != last_epdata) {
      if(last_epdata) {
         fat_assign_t* cur_epd = (fat_assign_t*) epdata;
         fat_assign_t* old_epd = (fat_assign_t*) last_epdata;
         // Save current directory for last used assign
         if((fres = f_getcwd(old_epd->cwd, sizeof(old_epd->cwd)))) {
            debug_printf("f_getcwd: %d\n", fres);
         } else debug_printf("cwd '%s' saved\n", old_epd->cwd);
         // Change into current directory for this assign
         if((fres = f_chdir(cur_epd->cwd))) {
            debug_printf("f_chdir: %d\n", fres);
         } else debug_printf("cwd '%s' restored\n", cur_epd->cwd);
      } else debug_puts("last_epdata == NULL\n");
      last_epdata = epdata;
   } else debug_puts("Same drive again.\n");

   switch(txbuf->type) {
      case FS_CHDIR:
         debug_printf("CHDIR into '%s'\n", path);
         fres = f_chdir(path);
         break;

      case FS_MKDIR:
         debug_printf("MKDIR '%s'\n", path);
         fres = f_mkdir(path);
         break;

      case FS_RMDIR:
         // FIXME: will unlink files as well.
         // Should I test first, if "path" is really a directory?
         debug_printf("RMDIR '%s'\n", path);
         fres = f_unlink(path);
         break;

      case FS_OPEN_RD:
         /* open file for reading (only) */
         fp = tbl_ins_file(channelno);
         if(fp) {
            fres = f_open(fp, path, FA_READ | FA_OPEN_EXISTING);
            debug_printf("FS_OPEN_RD '%s' #%d, res=%d\n",
                                    path, channelno, fres);
         } else {
            // too many files!
            cres = CBM_ERROR_NO_CHANNEL;
         }
         break;

      case FS_OPEN_WR:
         /* open file for writing (only); error if file exists */
         fp = tbl_ins_file(channelno);
         if(fp) {
            fres = f_open(fp, path, FA_WRITE | FA_CREATE_NEW);
            debug_printf("FS_OPEN_WR '%s' #%d, res=%d\n",
                                    path, channelno, fres);
         } else {
            // too many files!
            cres = CBM_ERROR_NO_CHANNEL;
         }
         break;

      case FS_OPEN_RW:
         /* open file for read/write access */
         fp = tbl_ins_file(channelno);
         if(fp) {
            fres = f_open(fp, path, FA_READ | FA_WRITE);
            debug_printf("FS_OPEN_RW '%s' #%d, res=%d\n",
                                    path, channelno, fres);
         } else {
            // too many files!
            cres = CBM_ERROR_NO_CHANNEL;
         }
         break;

      case FS_OPEN_OW:
         /* open file for write only, overwriting.  If the file exists it
            is truncated and writing starts at the beginning. If it does
            not exist, create the file
         */
         fp = tbl_ins_file(channelno);
         if(fp) {
            fres = f_open(fp, path, FA_WRITE | FA_CREATE_ALWAYS);
            debug_printf("FS_OPEN_OW '%s' #%d, res=%d\n",
                                    path, channelno, fres);
         } else {
            // too many files!
            cres = CBM_ERROR_NO_CHANNEL;
         }
         break;

      case FS_OPEN_AP:
         // open an existing file for appending data to it.
         // Returns an error if it does not exist
         fp = tbl_ins_file(channelno);
         if(fp) {
            fres = f_open(fp, path, FA_WRITE | FA_OPEN_EXISTING);
            debug_printf("FS_OPEN_AP '%s' #%d, res=%d\n",
                                    path, channelno, fres);
            // move to end of file to append data
            fres = f_lseek(fp, f_size(fp));
            debug_printf("Move to EOF to append data: %d\n", fres);
         } else {
            // too many files!
            cres = CBM_ERROR_NO_CHANNEL;
         }
         break;

      case FS_OPEN_DR:
         /* open a directory for reading, 3 scenarios:
            dirmask given, is a subdirectory  --> show its contents
            dirmask given, is not a directory --> show matching files
            no dirmask given                  --> show all directory entries
         */
         debug_printf("FS_OPEN_DIR for drive %d, ", txbuf->buffer[0]);
         char *b, *d;
         if (txbuf->len > 2) { // 2 bytes: drive number and zero terminator
            debug_printf("dirmask '%s'\n", txbuf->buffer + 1);
            // If path is a directory, list its contents
            if(f_stat(path, &Finfo) == FR_OK && Finfo.fattrib & AM_DIR) {
               debug_printf("'%s' is a directory\n", path);
               if((fres = f_opendir(&dir.D, path))) break;
               b = splitpath(path, &d);
               strcpy(dir.mask, "*");
            } else {
               b = splitpath(path, &d);
               debug_printf("DIR: %s NAME: %s\n", d, b);
               if((fres = f_opendir(&dir.D, d))) break;
               strncpy(dir.mask, b, sizeof dir.mask);
               dir.mask[sizeof(dir.mask) -1] = 0;
            }
            shortname(dir.headline, b);
         } else {
            debug_puts("no dirmask\n");
            if((fres = f_opendir(&dir.D, "."))) break;
            // Use dir.mask as temporary storage for current directory
            // because it is much larger (_MAX_LFN+1) than dir.headline (16+1)
            f_getcwd(dir.mask, sizeof dir.mask);
            shortname(dir.headline, dir.mask);
            strcpy(dir.mask, "*");
         }
         dir.drive = txbuf->buffer[0];
         fres = tbl_ins_dir(channelno);
         break;

      case FS_CLOSE:
         // close a file, ignored when not opened first
         debug_printf("FS_CLOSE #%d", channelno); debug_putcrlf();
         fres = tbl_close_file(channelno);
         break;

      case FS_MOVE:
         // rename / move a file
         fres = fs_move(path);
         break;

      case FS_DELETE:
         reply_as_usual = false;
         fs_delete(path, rxbuf, fatfs_rtc->advanced_wildcards); // replies via rxbuf
         break;

      case FS_READ:
         reply_as_usual = false;
         debug_puts("FS_READ\n");
         int8_t ds = get_dir_state(channelno);
         if(ds < 0 ) {
            // Nothing to read
            debug_printf("No channel for FS_READ #%d\n", channelno);
            cres = CBM_ERROR_FILE_NOT_OPEN;
         } else if(ds) {
            // Read directory
            cres = fs_read_dir(epdata, channelno, fatfs_rtc->advanced_wildcards, rxbuf);
         } else {
            // Read file
            fp = tbl_find_file(channelno);
            fres = f_read(fp, rxbuf->buffer, rxbuf->len, &transferred);
            debug_printf("%d/%d bytes read from #%d, fres=%d\n",
                         transferred, rxbuf->len, channelno, fres);
            if(fres != FR_OK) {
               // an error is sent as REPLY with error code
               rxbuf->type = FS_REPLY;
               packet_write_char(rxbuf, -conv_fresult(fres));
            } else {
               // a DATA packet mirrors the READ request when ok
               rxbuf->wp = transferred;
               if(fp->fptr == fp->fsize) {
                  rxbuf->type = FS_DATA_EOF;
               } else {
                  rxbuf->type = FS_DATA;
               }
            }
         }
         break;

      case FS_WRITE:
      case FS_WRITE_EOF:
         fp = tbl_find_file(channelno);
         if(fp) {
            if(txbuf->rp < txbuf->wp) {
               len = txbuf->wp - txbuf->rp;
               fres = f_write(fp, txbuf->buffer, len, &transferred);
               debug_printf("%d/%d bytes written to #%d, res=%d\n",
                            transferred, len, channelno, fres);
               if(fres == FR_OK) {
                  // a REPLY mirrors the WRITE request when ok
                  txbuf->type = FS_REPLY;
                  reply_as_usual = false;
               }
            }
         } else {
            debug_printf("No channel for FS_WRITE/FS_WRITE_EOF #%d\n", channelno);
            cres = CBM_ERROR_FILE_NOT_OPEN;
         }

         break;

      case FS_FORMAT:
      case FS_CHKDSK:
         debug_printf("Command %d unsupported", txbuf->type);
         cres = CBM_ERROR_SYNTAX_INVAL;
         break;

      default:
         debug_puts("### UNKNOWN CMD ###"); debug_putcrlf();
         debug_dump_packet(txbuf);
         break;
   }

   cres = combine(cres, fres);
   if(reply_as_usual) {
      rxbuf->type = FS_REPLY;   // return error code with FS_REPLY
      packet_write_char(rxbuf, cres);
   }
   callback(channelno, cres, rxbuf);
}


// ----- Directories -------------------------------------------------------

cbm_errno_t fs_read_dir(void *epdata, int8_t channelno, bool advanced_wildcards, packet_t *packet) {
   FRESULT fres;
   int8_t tblpos = tbl_chpos(channelno);
   char *p = (char *) packet->buffer;
   FILINFO Finfo;  // holds file information returned by f_readdir/f_stat
                   // the long file name *lfname must be stored externally:
#  ifdef _USE_LFN
      char Lfname[_MAX_LFN+1];
      Finfo.lfname = Lfname;
      Finfo.lfsize = sizeof Lfname;
#  endif

   switch(get_dir_state(channelno)) {
      case -1:
         // no channel
         debug_puts("fs_read_dir: no channel!"); debug_putcrlf();
         packet->type = FS_DATA_EOF;
         return CBM_ERROR_FILE_NOT_OPEN;
         break;

      case DIR_HEAD:
         // Disk name
         debug_puts("fs_read_dir/DIR_HEAD"); debug_putcrlf();
         p[FS_DIR_LEN+0] = dir.drive;
         p[FS_DIR_LEN+1] = 0;
         p[FS_DIR_LEN+2] = 0;
         p[FS_DIR_LEN+3] = 0;
         // TODO: add date
         p[FS_DIR_MODE]  = FS_DIR_MOD_NAM;
         memset(p + FS_DIR_NAME, ' ', 16); // init headline with 16 spaces
         strncpy(p+FS_DIR_NAME, dir.headline, 16);
         // replace zero terminator with space to get a 16 chars long name
         for(char *ptr = p + FS_DIR_NAME; ptr < p + FS_DIR_NAME + 16; ptr++)
            if(!*ptr) *ptr = ' ';
         p[FS_DIR_NAME + 16] = 0;

         tbl[tblpos].dir_state = DIR_FILES;
         packet_update_wp(packet, FS_DIR_NAME + 17);
         return 0;

      case DIR_FILES:
         // Files and directories
         debug_puts("fs_read_dir/DIR_FILES"); debug_putcrlf();
         for(;;) {
            fres = f_readdir(&dir.D, &Finfo);
            if(fres != FR_OK || !Finfo.fname[0]) {
               tbl[tblpos].dir_state = DIR_FOOTER;
               return CBM_ERROR_OK;
            }
            char *filename;
            filename = Finfo.fname;
#           if _USE_LFN
               if(Lfname[0]) filename = Lfname;
#           endif
            if(compare_pattern(filename, dir.mask, advanced_wildcards)) break;
         }

         // TODO: skip or skip not hidden files
         debug_printf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  '%s'",
               (Finfo.fattrib & AM_DIR) ? 'D' : '-',
               (Finfo.fattrib & AM_RDO) ? 'R' : '-',
               (Finfo.fattrib & AM_HID) ? 'H' : '-',
               (Finfo.fattrib & AM_SYS) ? 'S' : '-',
               (Finfo.fattrib & AM_ARC) ? 'A' : '-',
               (Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15,
                Finfo.fdate & 31,
               (Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63,
               Finfo.fsize, &(Finfo.fname[0]));
#        if _USE_LFN
            for (uint8_t i = strlen(Finfo.fname); i < 14; i++)
               debug_putc(' ');
            debug_printf("'%s'", Lfname);
#        endif
         debug_putcrlf();

         p[FS_DIR_LEN]   =  Finfo.fsize        & 255;
         p[FS_DIR_LEN+1] = (Finfo.fsize >>  8) & 255;
         p[FS_DIR_LEN+2] = (Finfo.fsize >> 16) & 255;
         p[FS_DIR_LEN+3] = (Finfo.fsize >> 24) & 255;

         p[FS_DIR_YEAR]  = (Finfo.fdate >> 9)  + 80;
         p[FS_DIR_MONTH] = (Finfo.fdate >> 5)  & 15;
         p[FS_DIR_DAY]   =  Finfo.fdate        & 31;
         p[FS_DIR_HOUR]  =  Finfo.ftime >> 11;
         p[FS_DIR_MIN]   = (Finfo.ftime >>  5) & 63;

         if(Finfo.fattrib & AM_DIR) {
            p[FS_DIR_MODE] = FS_DIR_MOD_DIR;    // DIR
         } else {
            p[FS_DIR_MODE] = FS_DIR_MOD_FIL;
            p[FS_DIR_ATTR] = FS_DIR_TYPE_PRG;   // PRG for all files
         }

         // write protected?
         if(Finfo.fattrib & AM_RDO) p[FS_DIR_ATTR] |= FS_DIR_ATTR_LOCKED;


#        ifdef _USE_LFN
            if((strlen(Lfname) > 16 ) || (!Lfname[0])) {
               // no LFN or too long LFN ==> use short name
               strcpy(p + FS_DIR_NAME, Finfo.fname);
            } else strcpy(p + FS_DIR_NAME, Lfname);
#        else
            strcpy(p + FS_DIR_NAME, Finfo.fname);
#        endif

         packet_update_wp(packet, FS_DIR_NAME + strlen(p+FS_DIR_NAME));
         return 0;

      case DIR_FOOTER:
      default:
         // number of free bytes / end of directory
         debug_puts("fs_read_dir/DIR_FOOTER"); debug_putcrlf();
         FATFS *fs = &Fatfs[0];
         DWORD free_clusters;
         DWORD free_bytes = 0;   // fallback default size
         fres = f_getfree("0:/", &free_clusters, &fs);
         if(fres == FR_OK) {
            // assuming 512 bytes/sector ==> * 512 ==> << 9
            free_bytes = (free_clusters * fs->csize) << 9;
         } else debug_printf("f_getfree: %d\n", fres);
         p[FS_DIR_LEN] = free_bytes & 255;
         p[FS_DIR_LEN+1] = (free_bytes >> 8) & 255;
         p[FS_DIR_LEN+2] = (free_bytes >> 16) & 255;
         p[FS_DIR_LEN+3] = (free_bytes >> 24) & 255;
         p[FS_DIR_MODE]  = FS_DIR_MOD_FRE;
         p[FS_DIR_NAME] = 0;
         packet_update_wp(packet, FS_DIR_NAME + strlen(p+FS_DIR_NAME));
         packet->type = FS_DATA_EOF;
         return CBM_ERROR_OK;
   }
}

// ----- Rename a file or directory ----------------------------------------

static cbm_errno_t fs_move(char *buf) {
   // Rename/move a file or directory
   // DO NOT RENAME/MOVE OPEN OBJECTS!
   FRESULT fres;
   uint8_t p = 0;
   char *from, *to;
   FILINFO fileinfo;

   // first find the two names separated by '='
   while (buf[p] != 0 && buf[p] != '=') p++;
   if (!buf[p]) return CBM_ERROR_SYNTAX_NONAME;

   buf[p] = 0;
   from = buf + p + 1;
   to = buf;

   debug_printf("FS_MOVE '%s' to '%s'", from, to); debug_putcrlf();

   if((fres = f_stat(to, &fileinfo)) == FR_OK)
      return CBM_ERROR_FILE_EXISTS;
   if(fres != FR_NO_FILE) return conv_fresult(fres);

   return conv_fresult(f_rename(from, to));
}


// ----- Delete files ------------------------------------------------------

cbm_errno_t _scratch(const char *path) {
   debug_printf("Scratching '%s'\n", path);
   FRESULT fres = f_unlink(path);
   if(fres) debug_printf("f_unlink: %d\n", fres);
   return conv_fresult(fres);
}

/* Deletes one or more file masks separated by commas
   Limits the reported number of scratched files to 99
   Returns CBM_ERROR_SCRATCHED plus number of scratched files
   Returns only the error but not the number of scratched files
   in case of any errors
*/
static void fs_delete(char *path, packet_t *packet, bool advanced_wildcards) {
   cbm_errno_t cres = CBM_ERROR_OK;
   uint16_t files_scratched = 0;
   char *pnext;

   while(path) {
      pnext = strchr(path, ',');
      if(pnext) *pnext = 0;
      debug_printf("Scratching '%s'...\n", path);

      cres = traverse(path,
         advanced_wildcards,
         0,                   // don't limit number of files to scratch
         &files_scratched,    // counts matches
         0,                   // no special file attributes required
         AM_DIR | AM_RDO,     // ignore directories and read-only-files
         _scratch);

      if(cres) break;

      path = pnext ? pnext + 1 : NULL;
   }

   if(cres) {
      packet_write_char(packet, conv_fresult(cres));
   } else {
      packet_write_char(packet, CBM_ERROR_SCRATCHED);
      packet_write_char(packet, (files_scratched > 99)?99:files_scratched);
   }
}
