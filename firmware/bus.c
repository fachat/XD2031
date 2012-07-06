/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    Parts of this file derived from ieee.c in
    sd2iec - SD/MMC to Commodore serial bus interface/controller
    Copyright (C) 2007-2012  Ingo Korb <ingo@akana.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; 
    version 2 of the License ONLY.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

/*
 * IEEE488 impedance layer
 *
 * provides the parallelsendbyte, parallelreceivebyte and parallelattention
 * methods called by the IEEE loop, and translates these into 
 * calls to the channel framework, open calls etc.
 */

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "channel.h"
#include "status.h"
#include "errormsg.h"
#include "ieee.h"
#include "bus.h"
#include "cmd.h"

#include "led.h"
#include "debug.h"

#include "xs1541.h"

#undef	DEBUG_SERIAL
#undef	DEBUG_SERIAL_DATA

/*
  Debug output:

  AXX   : ATN 0xXX
  c     : listen_handler cancelled
  C     : CLOSE
  l     : UNLISTEN
  L     : LISTEN
  D     : DATA 0x60
  O     : OPEN 0xfX
  ?XX   : unknown cmd 0xXX
  .     : timeout after ATN

*/

/* ------------------------------------------------------------------------- */
/*  Global variables                                                         */
/* ------------------------------------------------------------------------- */

uint8_t device_address;                 /* Current device address */

static channel_t 	*channel;

static uint8_t		device;		// primary command byte, includes dev addr and TALK/LISTEN/...
static uint8_t		secondary;	// secondary command byte, includes sec addr and OPEN/CLOSE/...

/**
 * struct ieeeflags_t - Bitfield of various flags, mostly IEEE-related
 * @eoi_recvd      : Received EOI with the last byte read
 * @command_recvd  : Command or filename received
 *
 * This is a bitfield for a number of boolean variables used
 */

volatile struct {

  int8_t errnum;		// from interrupt between BUS_CMDWAIT and BUS_CMDPROCESS

  uint8_t cmd;

} ieee_data;

/* -------------------------------------------------------------------------
 *  Error and command channel handling
 */

static void set_ieee_ok(void);

static errormsg_t error = {
	set_ieee_ok
};

static void set_ieee_ok(void) {
	set_error(&error, 0);
};

static cmd_t command = {
	0, "", &error
};

static void ieee_submit_status_refill(int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                void (*callback)(int8_t channelno, int8_t errnum)) {

	debug_puts("IEEE Status refill"); debug_putcrlf();

	if (packet_get_type(txbuf) != FS_READ) {
		// should not happen
		debug_printf("SNH: packet type is not FS_READ, but: %d\n", packet_get_type(txbuf));
		callback(channelno, ERROR_NO_CHANNEL);
		return;
	}

	uint8_t *buf = packet_get_buffer(rxbuf);
	uint8_t len = packet_get_capacity(rxbuf);

	strncpy(buf, error.error_buffer, len);
	buf[len-1] = 0;	// avoid buffer overflow

	// overwrite with actual length
	len = strlen(buf);

	// fixup packet	
	packet_set_filled(rxbuf, channelno, FS_EOF, len);

	// reset actual error channel until next read op
	set_error(&error, 0);

	// notify about packet ready
	callback(channelno, 0);
}

static provider_t ieee_status_provider = {
	NULL,
	ieee_submit_status_refill,
	NULL,
	NULL
};


/********************************************************************************/

/* Init IEEE bus */
void ieee_init(void) {

  	/* Read the hardware-set device address */
	//  device_address = device_hw_address();
  	device_address = 8;

  	/* Init vars and flags */
  	command.command_length = 0;

	channel = NULL;
}

/********************************************************************************
 * command buffer and command execution
 */

static uint8_t cmd_done = 0;

static void _cmd_callback(int8_t errnum) {
    ieee_data.errnum = errnum;
    cmd_done = 1;
}

static int16_t cmd_handler (void)
{
    cmd_done = 0;
   
    uint8_t secaddr = secondary & 0x0f;

//led_on();
    if (secaddr == 0x0f) {
      /* Handle commands */

      doscommand(&command);                   /* Command channel */
    } else {

      /* Handle filenames */

#ifdef DEBUG_SERIAL
    debug_printf("Open file secaddr=%02x, name='%s'\n",
         secaddr, command.command_buffer);
#endif


      int8_t rv = ieee_file_open(secaddr, &command, _cmd_callback);
      if (rv < 0) {
	// open ran into an error
	// -- errormsg should be already set, so nothing left to do here
	// TODO
      }	else {
	// as this code is not (yet?) prepared for async operation, we 
	// need to wait here until the response from the server comes
	//
	// TODO this should be reworked more backend (serial) independent
	while (cmd_done == 0) {
		serial_delay();
	}
	// result of the open
        if (ieee_data.errnum != 0) {
                set_error(&error, ieee_data.errnum);
                channel_close(ieee_secaddr_to_channel(secaddr));
        } else {
                // really only does something on read-only channels
                channel_preload(ieee_secaddr_to_channel(secaddr));
	}
      }
    }
    command.command_length = 0;
}


/********************************************************************************
 * impedance match
 */

// called during listenloop to send bytes to the server
int16_t parallelsendbyte(uint8_t data, uint8_t with_eoi) {

    int16_t st = 0;
//led_on();
#ifdef DEBUG_SERIAL_DATA
    debug_printf("sendbyte: %02x (%c)\n", data, (isprint(data) ? data : '-'));
#endif
delayus(45);

    if((secondary & 0x0f) == 0x0f || (secondary & 0xf0) == 0xf0) {
      if (command.command_length < CONFIG_COMMAND_BUFFER_SIZE) {
        command.command_buffer[command.command_length++] = data;
      }
    } else {
      channel = channel_put(channel, data, with_eoi);
      if (channel == NULL) {
	st = 0x83;	// TODO correct code
      }
    }
    return st + (device << 8);
}


// called during talkloop to receive bytes from the server
// If the preload parameter is set, the data byte is set,
// but the read pointers are not advanced (used to be named "fake"...)
int16_t parallelreceivebyte(uint8_t *data, uint8_t preload) {

	int16_t st = 0;

	uint8_t secaddr = secondary & 0x0f;

	if (channel == NULL) {
		st = 0x83;
	} else {
#ifdef DEBUG_SERIAL
		//debug_printf("rx: chan=%p, channo=%d\n", channel, channel->channel_no);
#endif
		// first fillup the buffers
		channel_preloadp(channel);

		*data = channel_current_byte(channel);
		if (channel_current_is_eof(channel)) {
			debug_printf("EOF!\n");
			st |= 0x40;
		}

		if (!preload) {
			if (!channel_next(channel)) {
				if (channel_has_more(channel)) {
					channel_refill(channel);
				} else {
      					if (secaddr == 15 || secaddr == 0) {
        					// autoclose when load is done, or after reading status channel
						channel_close(ieee_secaddr_to_channel(secaddr));
						channel = NULL;
					}
				}
			}
		}
	}
	return st + (device << 8);
}

/* These routines work for IEEE488 emulation on both C64 and PET.  */
static int parallelcommand(void)
{
    uint8_t b;
    int8_t secaddr;
    int i, st = 0;

#ifdef DEBUG_SERIAL
    debug_printf("***ParallelCommand %02x %02x\n",
         device, secondary);
#endif

    /* which device ? */
    //p = &serialdevices[TrapDevice & 0x0f];

    secaddr = secondary & 0x0f;

    /* if command on a channel, reset output buffer... */
    if ((secondary & 0xf0) != 0x60) {
	// nothing to do here
    }
    switch (secondary & 0xf0) {
      case 0x60:
          /* Open Channel */
	  channel = channel_find(ieee_secaddr_to_channel(secaddr));
	  if (channel == NULL) {
		debug_printf("Did not find channel!\n");
		st |= 0x40;	// TODO correct code?
	  }
          if ((!st) && ((device & 0xf0) == 0x40)) {
	      	// if we should TALK, prepare the first data byte
              	st = parallelreceivebyte(&b, 1) & 0xbf;   /* any error, except eof */
          }
          break;
      case 0xE0:
          /* Close File */
          if(secaddr == 15) {
	    // is this correct or only a convenience?
            ieee_channel_close_all();
          } else {
            /* Close a single buffer */
            channel_close(ieee_secaddr_to_channel(secaddr));
          }
          break;

      case 0xF0:
          /* Open File */
	  st = cmd_handler();
          break;

      default:
          debug_printf("Unknown command %02X\n\n", secondary & 0xff);
	  break;
    }
    return (st);
}

int16_t parallelattention(uint8_t b) {
    int16_t st = 0;

    // UNLISTEN and it is either open or the command channel
    if (b == 0x3f
        && (((secondary & 0xf0) == 0xf0)
            || ((secondary & 0x0f) == 0x0f))) {
	// then process the command
        st = parallelcommand();
    } else
	// not open, not command:
        switch (b & 0xf0) {
          case 0x20:
          case 0x40:
	      // store device number plus LISTEN/TALK info
              device = b;
              break;

          case 0x60:
          case 0xe0:
	      // secondary address (open DATA channel, or CLOSE)
              secondary = b;
	      // process a command if necessary
              st |= parallelcommand();
              break;

          case 0xf0:            /* Open File needs the filename first */
              secondary = b;
	      // TODO: close previously opened file
              break;
        }

    if (device_address != (device & 0x0f)) {
	// not this device
        st |= 0x80;
    }

    if ((b == 0x3f) || (b == 0x5f)) {
	// unlisten, untalk
        device = 0;
        secondary = 0;
    }

#ifdef DEBUG_SERIAL
    debug_printf("ParallelAttention(%02x)->TrapDevice=%02x, st=%04x\n",
               b, device, st + (device << 8));
#endif

    st |= device << 8;

    return st;
}


