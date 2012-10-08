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
 * provides the bus_sendbyte, bus_receivebyte and bus_attention
 * methods called by the IEEE loop, and translates these into 
 * calls to the channel framework, open calls etc.
 *
 * Uses a struct with state info, so can be called from both
 * IEEE488 as well as IEC bus
 */

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "channel.h"
#include "status.h"
#include "errormsg.h"
#include "cmd.h"
#include "cmd2.h"
#include "file.h"
#include "rtconfig.h"
#include "bus.h"

#include "led.h"
#include "debug.h"

#include "device.h"

#define	DEBUG_SERIAL
#undef	DEBUG_SERIAL_DATA

/* -------------------------------------------------------------------------
 *  Error and command channel handling
 */


static errormsg_t error;


/********************************************************************************/

static uint8_t secaddr_offset_counter;

void bus_init() {
	secaddr_offset_counter = 0;

	set_error(&error, ERROR_DOSVERSION);
}

/* Init IEEE bus */
void bus_init_bus(bus_t *bus) {

	bus->secaddr_offset = secaddr_offset_counter;
	secaddr_offset_counter += 16;

  	/* Read the hardware-set device address */
	//  device_address = device_hw_address();
  	uint8_t devaddr = 8;

  	/* Init vars and flags */
  	bus->command.command_length = 0;

	// this is copied over after an UNTALK/UNLISTEN
	bus->current_device_address = devaddr;
	rtconfig_init(&(bus->rtconf), devaddr);

	bus->channel = NULL;
}


/********************************************************************************
 * command buffer and command execution
 */

static volatile bus_t *bus_for_irq;

static void _cmd_callback(int8_t errnum, uint8_t *rxdata) {
    	bus_for_irq->errnum = errnum;
    	if (errnum == 1) {
		if (rxdata != NULL) {
			bus_for_irq->errparam = rxdata[1];
		} else {
			bus_for_irq->errparam = 0;
		}
    	}
    	bus_for_irq->cmd_done = 1;
}

static int16_t cmd_handler (bus_t *bus)
{

	int16_t st = 0;

	bus->cmd_done = 0;
   
	uint8_t secaddr = bus->secondary & 0x0f;

	int8_t rv = 0;

	// prepare for callback from interrupt
	bus_for_irq = bus;
	bus_for_irq->cmd_done = 0;

	// zero termination
	bus->command.command_buffer[bus->command.command_length++] = 0;

	if (secaddr == 0x0f) {
      		/* Handle commands */
		// zero termination
      		rv = command_execute(bus_secaddr_adjust(bus, secaddr), bus, &error, _cmd_callback);

		// change device address after command
		bus_for_irq->current_device_address = bus_for_irq->rtconf.device_address;
    	} else {
      		/* Handle filenames */

#ifdef DEBUG_SERIAL
      		debug_printf("Open file secaddr=%02x, name='%s'\n",
         		secaddr, bus->command.command_buffer);
#endif
      		rv = file_open(bus_secaddr_adjust(bus, secaddr), bus, &error, 
							_cmd_callback, secaddr == 1);
    	}

      	if (rv < 0) {
		// open ran into an error
		// -- errormsg should be already set, so nothing left to do here
		// TODO
		debug_printf("Received direct error number on open: %d\n", rv);
        	set_error(&error, ERROR_DRIVE_NOT_READY);
		st = 2;
      	} else {
		// as this code is not (yet?) prepared for async operation, we 
		// need to wait here until the response from the server comes
		// Note: use bus_for_irq here, as it is volatile
		while (bus_for_irq->cmd_done == 0) {
			// TODO this should be reworked more backend (serial) independent
			main_delay();
		}
#ifdef DEBUG_SERIAL
		debug_printf("Received callback error number on open: %d\n", bus_for_irq->errnum);
#endif
		// result of the open
		if (bus_for_irq->errnum == 1) {
			set_error_ts(&error, bus_for_irq->errnum, bus_for_irq->errparam, 0);
		} else {
                	set_error(&error, bus_for_irq->errnum);
		}

		// command may (or may not) open channel 15 for callback to the server
		// so close it here, as this is done separately
		if (secaddr == 15) {
			channel_close(bus_secaddr_adjust(bus, 15));
		}

        	if (bus_for_irq->errnum != 0) {
        	        channel_close(bus_secaddr_adjust(bus, secaddr));
        	} else {
                	// really only does something on read-only channels
                	channel_preload(bus_secaddr_adjust(bus, secaddr));
		}
      	}

    	bus_for_irq->command.command_length = 0;

    	return st;
}


/********************************************************************************
 * impedance match
 */

// called during listenloop to send bytes to the server
int16_t bus_sendbyte(bus_t *bus, uint8_t data, uint8_t with_eoi) {

    int16_t st = 0;
#ifdef DEBUG_SERIAL_DATA
    debug_printf("sendbyte: %02x (%c)\n", data, (isprint(data) ? data : '-'));
#endif

    if((bus->secondary & 0x0f) == 0x0f || (bus->secondary & 0xf0) == 0xf0) {
      if (bus->command.command_length < CONFIG_COMMAND_BUFFER_SIZE) {
        bus->command.command_buffer[bus->command.command_length++] = data;
      }
    } else {
      bus->channel = channel_put(bus->channel, data, with_eoi);
      if (bus->channel == NULL) {
	st = 0x83;	// TODO correct code
      }
    }
    return st + (bus->device << 8);
}


// called during talkloop to receive bytes from the server
// If the preload parameter is set, the data byte is set,
// but the read pointers are not advanced (used to be named "fake"...)
int16_t bus_receivebyte(bus_t *bus, uint8_t *data, uint8_t preload) {

	int16_t st = 0;

	uint8_t secaddr = bus->secondary & 0x0f;
	channel_t *channel = bus->channel;

	if (secaddr == 15) {

		*data = error.error_buffer[error.readp];

		if (error.error_buffer[error.readp+1] == 0) {
			// send EOF
			st |= 0x40;
		}
		if (!preload) {
			// the real thing
			error.readp++;
			if (error.error_buffer[error.readp] == 0) {
				// finished reading the error message
				// set OK
				set_error(&error, ERROR_OK);
			}
		}
	} else {
	    if (channel == NULL) {
		// if still NULL, error
		debug_printf("Setting file not open on secaddr %d\n", bus->secondary);

		set_error(&error, ERROR_FILE_NOT_OPEN);
		st = 0x83;
	    } else {
#ifdef DEBUG_SERIAL
		//debug_printf("rx: chan=%p, channo=%d\n", channel, channel->channel_no);
#endif
		// first fillup the buffers
		channel_preloadp(channel);

		*data = channel_current_byte(channel);
		if (channel_current_is_eof(channel)) {
#ifdef DEBUG_SERIAL
			debug_puts("EOF!\n");
#endif
			st |= 0x40;
		}

		if (!preload) {
			// make sure the next call does have a data byte
			if (!channel_next(channel)) {
				if (channel_has_more(channel)) {
					channel_refill(channel);
				} else {
      					if (secaddr == 15 || secaddr == 0) {
        					// autoclose when load is done, or after reading status channel
						channel_close(bus_secaddr_adjust(bus, secaddr));
						bus->channel = NULL;
					}
				}
			}
		}
	    }
	}
	return st + (bus->device << 8);
}

/* These routines work for IEEE488 emulation on both C64 and PET.  */
static int16_t bus_prepare(bus_t *bus)
{
    uint8_t b;
    int8_t secaddr;
    int st = 0;

#ifdef DEBUG_SERIAL
    debug_printf("***BusCommand %02x %02x\n",
         bus->device, bus->secondary);
#endif

    if ((bus->device & 0x0f) != bus->current_device_address) {
	return 0x80;	// device not present
    }

    secaddr = bus->secondary & 0x0f;

	  if (secaddr != 15) {
          	/* Open Channel */
	  	bus->channel = channel_find(bus_secaddr_adjust(bus, secaddr));
		if (bus->channel == NULL) {
			debug_puts("Did not find channel!\n");
			st |= 0x40;	// TODO correct code?
		}
	  }
          if ((bus->device & 0xf0) == 0x40) {
	      	// if we should TALK, prepare the first data byte
	      	if (!st) {
              		st = bus_receivebyte(bus, &b, 1) & 0xbf;   /* any error, except eof */
			//debug_printf("receive gets st=%04x\n", st);
		} else {
			// cause a FILE NOT FOUND
			debug_puts("FILE NOT FOUND\n");
			bus->device = 0;
		}
          }
	return 0;
}

static void bus_close(bus_t *bus) {
    	uint8_t secaddr = bus->secondary & 0x0f;
          /* Close File */
          if(secaddr == 15) {
	    // is this correct or only a convenience?
            channel_close_range(bus_secaddr_adjust(bus, 0), bus_secaddr_adjust(bus, 15));
          } else {
            /* Close a single buffer */
            channel_close(bus_secaddr_adjust(bus, secaddr));
          }
}

int16_t bus_attention(bus_t *bus, uint8_t b) {
    int16_t st = 0;

    // UNLISTEN and it is either open or the command channel
    if (b == 0x3f
        && (((bus->secondary & 0xf0) == 0xf0)
            || ((bus->secondary & 0x0f) == 0x0f))) {

        if ((bus->device & 0x0f) == bus->current_device_address) {
		// then process the command
        	st = cmd_handler(bus);
        }

    } else {

	// not open, not command:
        switch (b & 0xf0) {
          case 0x20:
          case 0x40:
	      // store device number plus LISTEN/TALK info
	      if ((b & 0x0f) == bus->current_device_address) {
              	bus->device = b;
	      }
              break;

          case 0x60:
	      // secondary address (open DATA channel)
  	      if ((bus->device & 0x0f) == bus->current_device_address) {

              	bus->secondary = b;
	      	// process a command if necessary
              	bus_prepare(bus);
	      }
              break;
          case 0xe0:
	      // secondary address (CLOSE)
  	      if ((bus->device & 0x0f) == bus->current_device_address) {
              	bus->secondary = b;
	      	// process a command if necessary
              	//st = bus_command(bus);
              	bus_close(bus);
	      }
              break;

          case 0xf0:            /* Open File needs the filename first */
  	      if ((bus->device & 0x0f) == bus->current_device_address) {
              	bus->secondary = b;
	      	// TODO: close previously opened file
	      }
              break;
	  default:
	      // falls through e.g. for unlisten/untalk
	      break;
        }
    }

    if ((b == 0x3f) || (b == 0x5f) || ((b & 0xf0) == 0xe0)) {
	// unlisten, untalk, close
        bus->device = 0;
        bus->secondary = 0;

	led_set(IDLE);
    } else {
    	if (bus->current_device_address != (bus->device & 0x0f)) {
		// not this device
        	st |= 0x80;
    	} else {
		led_set(ACTIVE);
	}
    }

#ifdef DEBUG_SERIAL
    debug_printf("BusAttention(%02x)->TrapDevice=%02x, st=%04x\n",
               b, bus->device, st | (bus->device << 8));
    debug_flush();
#endif

    st |= bus->device << 8;

    return st;
}


