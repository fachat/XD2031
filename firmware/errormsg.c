/* 
 * XD2031 - Serial line file server for CBMs
 * Copyright (C) 2012 Andre Fachat <afachat@gmx.de>
 * 
 * Taken over / derived from:
 * sd2iec - SD/MMC to Commodore serial bus interface/controller
   Copyright (C) 2007-2012  Ingo Korb <ingo@akana.de>
   Inspired by MMC2IEC by Lars Pontoppidan et al.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License only.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include <string.h>
#include <ctype.h>
#include <avr/pgmspace.h>
#include "errormsg.h"
#include "channel.h"
#include "version.h"		/* for LONGVERSION */
#include "device.h"		/* for HW_NAME */
#include "debug.h"

#define	DEBUG_ERROR

/// Version number string, will be added to message 73
const char PROGMEM versionstr[] = HW_NAME "/" SW_NAME " V" VERSION;

/// Long version string, used for message 9
const char PROGMEM longverstr[] = LONGVERSION;

//static uint8_t error_buffer[CONFIG_ERROR_BUFFER_SIZE];

#define EC(x) x+0x80

/// Abbreviations used in the main error strings
static const uint8_t PROGMEM abbrevs[] = {
  EC(0), 'F','I','L','E',
  EC(1), 'R','E','A','D',
  EC(2), 'W','R','I','T','E',
  EC(3), ' ','E','R','R','O','R',
  EC(4), ' ','N','O','T',' ',
  EC(5), 'D','I','S','K',' ',
  EC(6), 'O','P','E','N',
  EC(7), 'R','E','C','O','R','D',
  EC(8), 'P','A','R','T','I','T','I','O','N',' ',
  EC(9), 'S','E','L','E','C','T','E','D',
  EC(10), 'I','L','L','E','G','A','L',
  EC(11), ' ','T','O','O',' ',
  EC(12), 'N','O',' ',
  EC(127)
};

/// Error string table
static const uint8_t PROGMEM messages[] = {
  EC(00),
    ' ','O','K',
  EC(01),
    0,'S',' ','S','C','R','A','T','C','H','E','D',
  EC(02),
    8,9,
  EC(20), EC(21), EC(22), EC(23), EC(24), EC(27),
    1,3,
  EC(25), EC(28),
    2,3,
  EC(26),
    2,' ','P','R','O','T','E','C','T',' ','O','N',
  EC(29),
    5,'I','D',' ','M','I','S','M','A','T','C','H',
  EC(30), EC(31), EC(32), EC(33), EC(34),
    'S','Y','N','T','A','X',3,
  EC(39), EC(62),
    0,4,'F','O','U','N','D',
  EC(50),
    7,4,'P','R','E','S','E','N','T',
  EC(51),
    'O','V','E','R','F','L','O','W',' ','I','N',' ',7,
  EC(52),
    0,11,'L','A','R','G','E',
  EC(60),
    2,' ',0,' ',6,
  EC(61),
    0,4,6,
  EC(63),
    0,' ','E','X','I','S','T','S',
  EC(64),
    0,' ','T','Y','P','E',' ','M','I','S','M','A','T','C','H',
  EC(65),
    12,'B','L','O','C','K',
  EC(66), EC(67),
    10,' ','T','R','A','C','K',' ','O','R',' ','S','E','C','T','O','R',
  EC(70),
    12,'C','H','A','N','N','E','L',
  EC(71),
    'D','I','R',3,
  EC(72),
    5,'F','U','L','L',
  EC(74),
    'D','R','I','V','E',4,1,'Y',
  EC(77),
    9,' ',8,10,
  EC(78),
    'B','U','F','F','E','R',11,'S','M','A','L','L',
  EC(79),
    'I','M','A','G','E',' ',0,' ','I','N','V','A','L','I','D',
  EC(99),
    'C','L','O','C','K',' ','U','N','S','T','A','B','L','E',
  EC(127)
};

static uint8_t *appendmsg(uint8_t *msg, const uint8_t *table, const uint8_t entry) {
  uint8_t i,tmp;

  i = 0;
  do {
    tmp = pgm_read_byte(table+i++);
    if (tmp == EC(entry) || tmp == EC(127))
      break;
  } while (1);

  if (tmp == EC(127)) {
    /* Unknown error */
    *msg++ = '?';
  } else {
    /* Skip over remaining error numbers */
    while (pgm_read_byte(table+i) >= EC(0)) i++;

    /* Copy error string to buffer */
    do {
      tmp = pgm_read_byte(table+i++);

      if (tmp < 32) {
        /* Abbreviation found, handle by recursion */
        msg = appendmsg(msg,abbrevs,tmp);
        continue;
      }

      if (tmp < EC(0))
        /* Don't copy error numbers */
        *msg++ = tmp;
    } while (tmp < EC(0));
  }

  return msg;
}

/* Append a decimal number to a string */
uint8_t *appendnumber(uint8_t *msg, uint8_t value) {
  if (value >= 100) {
    *msg++ = '0' + value/100;
    value %= 100;
  }

  *msg++ = '0' + value/10;
  *msg++ = '0' + value%10;

  return msg;
}

#if 0 /* currently unused */
static uint8_t *appendbool(uint8_t *msg, uint8_t ch, uint8_t value) {
  if (ch)
    *msg++ = ch;

  if (value)
    *msg++ = '+';
  else
    *msg++ = '-';

  *msg++ = ':';

  return msg;
}
#endif

void set_error_ts(errormsg_t *err, uint8_t errornum, uint8_t track, uint8_t sector) {
  uint8_t *msg = err->error_buffer;
  uint8_t i = 0;

#if 0
  current_error = errornum;
  buffers[CONFIG_BUFFER_COUNT].data     = error_buffer;
  buffers[CONFIG_BUFFER_COUNT].lastused = 0;
  buffers[CONFIG_BUFFER_COUNT].position = 0;
#endif

  // this should be replaced by just appending a \0 at the end
  memset(err->error_buffer,0,sizeof(err->error_buffer));

  msg = appendnumber(msg,errornum);
  *msg++ = ',';

  if (errornum == ERROR_STATUS) {
    switch(sector) {
    case 0:
    default:
      *msg++ = 'E';
#if 0
      msg = appendnumber(msg, file_extension_mode);
      msg = appendbool(msg, 0, globalflags & EXTENSION_HIDING);

      msg = appendbool(msg, '*', globalflags & POSTMATCH);

      msg = appendbool(msg, 'B', globalflags & FAT32_FREEBLOCKS);

      *msg++ = 'I';
      msg = appendnumber(msg, image_as_dir);

      *msg++ = ':';
      *msg++ = 'R';
      ustrcpy(msg, rom_filename);
      msg += ustrlen(rom_filename);
#endif
      break;
    case 1: // Drive Config
      *msg++ = 'D';
#if 0
      while(i < 8) {
        if(map_drive(i) != 0x0f) {
          *msg++ = ':';
          msg = appendnumber(msg,i);
          *msg++ = '=';
          msg = appendnumber(msg,map_drive(i));
        }
        i++;
      }
#endif
      break;
    }

  } else if (errornum == ERROR_LONGVERSION || errornum == ERROR_DOSVERSION) {
    /* Start with the name and version number */
    while ((*msg++ = pgm_read_byte(versionstr+i++))) ;

    /* Append the long version if requested */
    if (errornum == ERROR_LONGVERSION) {
      i = 0;
      msg--;
      while ((*msg++ = toupper((int)pgm_read_byte(longverstr+i++)))) ;
    }

    msg--;
  } else {
    msg = appendmsg(msg,messages,errornum);
  }
  *msg++ = ',';

  msg = appendnumber(msg,track);
  *msg++ = ',';

  msg = appendnumber(msg,sector);
  *msg++ = 13;

  // end string marker
  *msg = 0;

#ifdef DEBUG_ERROR
debug_printf("Set status to: %s\n", err->error_buffer);
#endif

  //channel_status_set(err->error_buffer, msg-err->error_buffer);

#if 0
  if (errornum >= 20 && errornum != ERROR_DOSVERSION) {
    // FIXME: Compare to E648
    // NOTE: 1571 doesn't write the BAM and closes some buffers if an error occured
    led_state |= LED_ERROR;
  } else {
    led_state &= (uint8_t)~LED_ERROR;
    set_error_led(0);
  }
  // WTF?
  buffers[CONFIG_BUFFER_COUNT].lastused = msg - error_buffer;
  /* Send message without the final 0x0d */
  display_errorchannel(msg - error_buffer, error_buffer);
#endif
}

