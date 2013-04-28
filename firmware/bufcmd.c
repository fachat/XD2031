/* 
    XD-2031 - Serial line file server for CBMs
    Copyright (C) 2012  Andre Fachat

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

*/

#include <errno.h>
#include <stdlib.h>

#include "bus.h"
#include "errormsg.h"
#include "channel.h"
#include "provider.h"

#include "debug.h"


#define   DEBUG_USER
#define   DEBUG_BLOCK
#define   CMD_BUFFER_LENGTH   4

static char buf[CMD_BUFFER_LENGTH];
static packet_t cmdpack;
static uint8_t cbstat;
static int8_t cberr;

/**
 * command callback
 */
static uint8_t callback(int8_t channelno, int8_t errnum) {
   cberr = errnum;
   cbstat = 1;
   return 0;
}

/**
 * user commands
 *
 * U1, U2
 *
 * cmdbuf pointer includes the full command, i.e. with the "U" in front.
 * 
 * returns ERROR_OK/ERROR_* on direct ok/error, or -1 if a callback has been submitted
 */

uint8_t cmd_user(bus_t *bus, char *cmdbuf, errormsg_t *error)
{ 
   uint8_t cmd = cmdbuf[1] & 15;  // U1 == UA,  U2 == UB
   uint8_t rv;
   int     channel,drive,track,sector;

#ifdef DEBUG_USER
   debug_printf("cmd_user (%02x): %s\n", cmd, cmdbuf);
#endif

   rv = sscanf(cmdbuf+2,"%*[^0-9]%d%*[^0-9]%d%*[^0-9]%d%*[^0-9]%d",
        &channel,&drive,&track,&sector);
   if (rv != 4) return ERROR_SYNTAX_INVAL;

        if (cmd == 1) buf[0] = FS_BLOCK_U1;
   else if (cmd == 2) buf[0] = FS_BLOCK_U2;
   else return ERROR_SYNTAX_UNKNOWN;

   channel = bus_secaddr_adjust(bus, channel);
   channel_flush(channel);

   buf[1] = drive;
   buf[2] = track;
   buf[3] = sector;
   packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
   packet_set_filled(&cmdpack, channel, FS_BLOCK, 4);

   endpoint_t *endpoint = provider_lookup(drive, NULL);
   if (endpoint)
   {   
      cbstat = 0;
      endpoint->provider->submit_call(NULL, channel,&cmdpack,
                                         &cmdpack, callback);
      while (cbstat == 0)
      {
         delayms(1);
         main_delay();
      }
      return cberr;
   }
   else
   {
      return ERROR_DRIVE_NOT_READY;
   }
}

/**
 * block commands
 *
 * B-R, B-W, B-P
 *
 * cmdbuf pointer includes the full command, i.e. with the "U" in front.
 * 
 * returns ERROR_OK/ERROR_* on direct ok/error, or -1 if a callback has been submitted
 */
uint8_t cmd_block(bus_t *bus, char *cmdbuf, errormsg_t *error) {

   return ERROR_SYNTAX_UNKNOWN;
}

