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
#include "errormsg.h"
#include "cmd.h"
#include "cmd2.h"
#include "file.h"
#include "rtconfig.h"
#include "rtconfig2.h"
#include "bus.h"

#include "led.h"
#include "debug.h"
#include "term.h"

#include "device.h"

#undef	DEBUG_BUS
#undef	DEBUG_BUS_DATA

#define	DEVICE_MASK	0x1f
#define	SECADDR_MASK	0x0f

#define	BUSCMD_MASK	0xe0	// ~DEVICE_MASK
#define	BUSSEC_MASK	0xf0	// ~SECADDR_MASK

#define	BUSCMD_TALK	0x40
#define	BUSCMD_UNTALK	0x5f
#define	BUSCMD_LISTEN	0x20
#define	BUSCMD_UNLISTEN	0x3f
#define	BUSCMD_DATA	0x60

#define	LOAD_SECADDR	0	// load channel

#define	BUSSEC_OPEN	0xf0	// secaddr for open
#define	BUSSEC_CLOSE	0xe0	// secaddr for close


/* -------------------------------------------------------------------------
 *  Error and command channel handling
 */


static errormsg_t error;


/* 
 * send a RESET request and open a receive channel for setopt (X command line
 * options packets 
 */

/********************************************************************************/

static uint8_t secaddr_offset_counter;

void bus_init() {
	secaddr_offset_counter = 0;

	set_error(&error, CBM_ERROR_DOSVERSION);
}

uint8_t get_default_device_address(void) {
  	uint8_t devaddr = 8;
#	ifdef DEV_ADDR
		devaddr = DEV_ADDR;
#	endif

  	/* Read the hardware-set device address */
	//  device_address = device_hw_address();

	return devaddr;
}

/* Init IEEE bus */
void bus_init_bus(const char *name, bus_t *bus) {

	bus->active = 0;

	bus->secaddr_offset = secaddr_offset_counter;
	secaddr_offset_counter += 16;

	/* Init vars and flags */
  	bus->command.command_length = 0;

	bus->rtconf.name = name;
	rtconfig_init_rtc(&(bus->rtconf), get_default_device_address());

	bus->channel = NULL;
}


/********************************************************************************
 * command buffer and command execution
 */

static volatile bus_t *bus_for_irq;

static void _cmd_callback(int8_t errnum, uint8_t *rxdata) {
    	bus_for_irq->errnum = errnum;
    	if (errnum == CBM_ERROR_SCRATCHED) {
		// files deleted
		if (rxdata != NULL) {
			// this is the "number of files deleted" parameter
			// rxdata[0] is the actual error code
			bus_for_irq->errparam = rxdata[1];
		} else {
			bus_for_irq->errparam = 0;
		}
    	} else 
	if (errnum == CBM_ERROR_DISK_FULL) {
		// TODO parameters for DISK FULL from close()
		if (rxdata != NULL) {
			bus_for_irq->errparam = rxdata[1];
			bus_for_irq->errparam2 = rxdata[2];
		} else {
			bus_for_irq->errparam = 0;
			bus_for_irq->errparam2 = 0;
		}
	}
    	bus_for_irq->cmd_done = 1;
}

static int16_t cmd_handler (bus_t *bus)
{

	int16_t st = 0;

	bus->cmd_done = 0;
   
	uint8_t secaddr = bus->secondary & SECADDR_MASK;

	int8_t rv = 0;

	// prepare for callback from interrupt
	bus_for_irq = bus;
	bus_for_irq->cmd_done = 0;

	uint8_t errdrive = bus->rtconf.errmsg_with_drive ? 0 : -1;

	// empty command (JiffyDOS sends a lot of them)
	if (bus->command.command_length == 0) {
		bus->cmd_done = 1;
		bus_for_irq = bus;
		bus_for_irq->cmd_done = 1;
		return 0;
	}

	if (secaddr == CMD_SECADDR) {
      		/* Handle commands */
		// zero termination
      		rv = command_execute(bus_secaddr_adjust(bus, secaddr), bus, &error, _cmd_callback);

		// change device address after command
		//bus_for_irq->current_device_address = bus_for_irq->rtconf.device_address;
    	} else {
      		/* Handle filenames */
		uint8_t openflag = 0;
		if (secaddr == 1) {
			openflag = OPENFLAG_SAVE;
		} else
		if (secaddr == 0) {
			openflag = OPENFLAG_LOAD;
		}
#ifdef DEBUG_BUS
      		debug_printf("Open file secaddr=%02x, name='%s'\n",
         		secaddr, bus->command.command_buffer);
#endif
      		rv = file_open(bus_secaddr_adjust(bus, secaddr), bus, &error, 
							_cmd_callback, openflag);
    	}

      	if (rv < 0) {
		// open ran into an error
		// -- errormsg should be already set, so nothing left to do here
		debug_puts("Received error on open/cmd!\n");
		st = STAT_RDTIMEOUT;
      	} else {
		// as this code is not (yet?) prepared for async operation, we 
		// need to wait here until the response from the server comes
		// Note: use bus_for_irq here, as it is volatile
#ifdef DEBUG_BUS
		debug_puts("Waiting for callback\n"); debug_flush();
#endif
		while (bus_for_irq->cmd_done == 0) {
			main_delay();
		}
#ifdef DEBUG_BUS
		debug_printf("Received callback error number on open: %d\n", bus_for_irq->errnum);
#endif
		// result of the open
		if (bus_for_irq->errnum == CBM_ERROR_SCRATCHED) {
			set_error_tsd(&error, bus_for_irq->errnum, bus_for_irq->errparam, 0, errdrive);
		} else {
                	set_error_tsd(&error, bus_for_irq->errnum, 0, 0, errdrive);
		}

#ifdef DEBUG_BUS
		debug_printf("after open: secaddr=%d\n", secaddr);
#endif

		if (secaddr != CMD_SECADDR) {
			uint8_t channel_no = bus_secaddr_adjust(bus, secaddr);
			// only open, not for commands

	        	if (bus_for_irq->errnum != 0) {
				// close channel on error. Is this ok?
	        	        channel_close(channel_no, NULL);
	        	} else {
#ifdef DEBUG_BUS
				debug_printf("preload channel %d\n", channel_no);
#endif
                		channel_t *chan = channel_find(channel_no);
                		// really only does something on read-only channels
				if (chan == NULL) {
			                term_printf("DID NOT FIND CHANNEL FOR CHAN=%d TO PRELOAD\n", 
						channel_no);
				} else {
					uint8_t data, iseof;
					int8_t err;
					channel_get(chan, &data, &iseof, &err, GET_PRELOAD);
                			//channel_preloadp(chan);
				}
			}
		}
      	}

	// flush debug output
	debug_flush();

    	bus_for_irq->command.command_length = 0;

    	return st;
}


/********************************************************************************
 * impedance match
 */

// called during listenloop to send bytes to the server
int16_t bus_sendbyte(bus_t *bus, uint8_t data, uint8_t with_eoi) {

    int16_t st = 0;
#ifdef DEBUG_BUS_DATA
    debug_printf("Bus: sendbyte: %02x (%c)\n", data, (isprint(data) ? data : '-'));
#endif

    if(((bus->secondary & SECADDR_MASK) == CMD_SECADDR) || ((bus->secondary & BUSSEC_MASK) == BUSSEC_OPEN)) {
      if (bus->command.command_length < CONFIG_COMMAND_BUFFER_SIZE) {
        bus->command.command_buffer[bus->command.command_length++] = data;
      }
    } else {
      if (bus->channel != NULL) {
	int8_t err = channel_put(bus->channel, data, with_eoi);
	if (err) debug_printf("last_push_error: %d (ch=%p)\n", err, bus->channel);
	if (err != CBM_ERROR_OK) {
	  int8_t errdrive = bus->rtconf.errmsg_with_drive ? bus->channel->drive : -1;
	  set_error_tsd(&error, err, 0, 0, errdrive);
	  st |= STAT_WRTIMEOUT;
	  bus->channel = NULL;
	}
      } else {
	st = STAT_NODEV | STAT_WRTIMEOUT;	// correct code?
      }
    }
    return st + (bus->device << 8);
}


// called during talkloop to receive bytes from the server
// If the preload parameter is set, the data byte is set,
// but the read pointers are not advanced (used to be named "fake"...)
int16_t bus_receivebyte(bus_t *bus, uint8_t *data, uint8_t preload) {

	int16_t st = 0;
	uint8_t iseof = 0;

	uint8_t secaddr = bus->secondary & SECADDR_MASK;
	channel_t *channel = bus->channel;

	int8_t errdrive = bus->rtconf.errmsg_with_drive ? (channel == NULL ? 0 : channel->drive) : -1;

	if (secaddr == CMD_SECADDR) {

		*data = error.error_buffer[error.readp];

		if (error.error_buffer[error.readp+1] == 0) {
			// send EOF
			st |= STAT_EOF;
		}
		if (!(preload & BUS_PRELOAD)) {
			// the real thing
			error.readp++;
			if (error.error_buffer[error.readp] == 0) {
				// finished reading the error message
				// set OK
				set_error_tsd(&error, CBM_ERROR_OK, 0, 0, errdrive);
			}
		}
	} else {
	    if (channel == NULL) {
		// if still NULL, error
		debug_printf("Bus: Setting file not open on secaddr %d\n", bus->secondary & 0x1f);

		set_error_tsd(&error, CBM_ERROR_FILE_NOT_OPEN, 0, 0, errdrive);
		st = STAT_NODEV | STAT_RDTIMEOUT;
	    } else {
		int8_t err;

		if (channel_get(channel, data, &iseof, &err, preload) < 0) {
			// could not get any data
			st |= STAT_RDTIMEOUT;
#ifdef DEBUG_BUS
			debug_printf("Bus: preload on chan %p (%d) gives no data (st=%04x)", channel, 
				channel->channel_no, st);
#endif
		} else {
			// got a data byte
			if (iseof) {
				st |= STAT_EOF;
			}
		}
		if (err != CBM_ERROR_OK) {
			set_error_tsd(&error, err, 0, 0, errdrive);
		}
	    }
	}
#ifdef DEBUG_BUS_DATA
	if (!(preload & BUS_PRELOAD)) {
	        debug_printf("receivebyte: %02x (%c)\n", *data, (isprint(*data) ? *data : '-'));
	}
#endif

	return st + (bus->device << 8);
}

/* These routines work for IEEE488 emulation on both C64 and PET.  */
static int16_t bus_prepare(bus_t *bus)
{
    uint8_t b;
    int8_t secaddr;
    int st = 0;

    uint8_t device = bus->device & DEVICE_MASK;

#ifdef DEBUG_BUS
    debug_printf("***BusCommand %02x %02x\n",
         bus->device, bus->secondary);
#endif

    if (device != bus->rtconf.device_address) {
	return 0x80;	// device not present
    }

    secaddr = bus->secondary & SECADDR_MASK;

    if (secaddr != CMD_SECADDR) {
          	/* Open Channel */
	  	bus->channel = channel_find(bus_secaddr_adjust(bus, secaddr));
		if (bus->channel == NULL) {
			debug_printf("DID NOT FIND CHANNEL FOR CHAN=%d!\n", bus_secaddr_adjust(bus,secaddr)); 
			st |= STAT_EOF | STAT_RDTIMEOUT | STAT_WRTIMEOUT;	// correct code?
		}
    }
    if ((bus->device & BUSCMD_MASK) == BUSCMD_TALK) {
	      	// if we should TALK, prepare the first data byte
	      	if (!st) {
              		st = bus_receivebyte(bus, &b, BUS_PRELOAD | BUS_SYNC) & 0xbf;   /* any error, except eof */
			//debug_printf("receive gets st=%04x\n", st);
		} else {
			// cause a FILE NOT FOUND
			debug_puts("FILE NOT FOUND\n");
			debug_flush();
			// Note: this would prevent IEC from actually doing the TALK
			// and returning timeout with it - which indicates to the C64
			// that a file was not found.
			// Not setting this does not seem to disturb IEEE also, so 
			// we do not reset the device.
			//bus->device = 0;
		}
    }
    return st;
}


void bus_close(bus_t *bus) {
    	uint8_t secaddr = bus->secondary & SECADDR_MASK;

	// prepare for callback from interrupt
	bus->cmd_done = 0;
	bus_for_irq = bus;
	bus_for_irq->cmd_done = 0;

#ifdef DEBUG_BUS
	debug_printf("bus_close: secaddr=%d\n",secaddr);
#endif
        /* Close File */
        if(secaddr == CMD_SECADDR) {
            // closing all files when closing the command channel is in fact correct, see discussion of
	    // issue #151, tested on 1541, 2031, 4040 and 1001 drives (under VICE truedrive emulation).
            channel_close_range(bus_secaddr_adjust(bus, 0), bus_secaddr_adjust(bus, CMD_SECADDR));
        } else {
       		/* Close a single buffer */
		// callback sets error for DISK FULL on close (when creating a 664 blocks large file
		// on a D64, DOS emits a DISK FULL on close only... 
            	if (channel_close(bus_secaddr_adjust(bus, secaddr), _cmd_callback) == CBM_ERROR_OK) {

#ifdef DEBUG_BUS
			debug_printf("bus_close: Waiting for callback (%p) on channel %d\n", _cmd_callback,
				bus_secaddr_adjust(bus, secaddr)); debug_flush();
#endif
	    		while (bus_for_irq->cmd_done == 0) {
				main_delay();
			}
#ifdef DEBUG_BUS
			debug_printf("bus_close: Received callback error number: %d\n", bus_for_irq->errnum);
#endif

			if (bus_for_irq->errnum != CBM_ERROR_OK) {
				channel_t *chan = channel_find(bus_secaddr_adjust(bus, secaddr));
				int8_t errdrive = bus->rtconf.errmsg_with_drive ? (chan ? chan->drive : 0)  : -1;
				set_error_tsd(&error, bus_for_irq->errnum, bus_for_irq->errparam, bus_for_irq->errparam2, errdrive);
			}
		}
        }
}

int16_t bus_attention(bus_t *bus, uint8_t b) {
    int16_t st = 0;

    uint8_t is_config_device = ( (bus->device & DEVICE_MASK) == bus->rtconf.device_address );

#ifdef DEBUG_BUS
    debug_printf("BusAttention(%02x)\n", b);
    debug_flush();
#endif


    // UNLISTEN and it is either open or the command channel
    if (b == BUSCMD_UNLISTEN
        && (((bus->secondary & BUSSEC_MASK) == BUSSEC_OPEN)
            || ((bus->secondary & SECADDR_MASK) == CMD_SECADDR))) {

        if (is_config_device) {
		// then process the command
		// note: may change bus->rtconf.device_address!
#ifdef DEBUG_BUS
		debug_printf("Bus: cmd_handler(%s)\n", bus->command.command_buffer);
#endif
        	st = cmd_handler(bus);
        }
	// as it's unlisten, we need to wait for ATN end
	st |= STAT_WAITEND;
    } else {

	// not unlisten, or not open, not command:
	// first handle the "outer" commands that use 3 command bits (mask is 0xe0)
        switch (b & BUSCMD_MASK) {
          case BUSCMD_LISTEN:		// 0x20
          case BUSCMD_TALK:		// 0x40
	      // store device number plus LISTEN/TALK info
	      // untalk / unlisten fall through here (there b & DEVICEMASK would be 0x1f)
	      if ((b & DEVICE_MASK) == bus->rtconf.device_address) {
#ifdef DEBUG_BUS
		debug_printf("Bus: LISTEN/TALK (%02)\n", b);
#endif
              	bus->device = b;
		is_config_device = 1;	// true
	      }
              break;

          case BUSCMD_DATA:		// 0x60
	      // secondary address (open DATA channel)
  	      if (is_config_device) {

              	bus->secondary = b;
	      	// process a command if necessary
              	st |= bus_prepare(bus);
	      }
              break;
	  default:
		// in the default case handle all the
		// others that use 4 command bits (mask is 0xf0)
		switch(b & BUSSEC_MASK) {
          	  case BUSSEC_CLOSE:
	      		// secondary address (CLOSE)
  	      		if (is_config_device) {
              			bus->secondary = b;
              			bus_close(bus);
	      		}
              		break;
          	  case BUSSEC_OPEN:            
	      		// for open, which needs the filename first before it can
	      		// actually do something (done in unlisten handling at top of
			// function)
  	      		if (is_config_device) {
              			bus->secondary = b;
	      			// TODO: close previously opened file
				
				// on OPEN we need to wait for ATN end to get the file name
				st |= STAT_WAITEND;
	      		}
              		break;
		  default:
			// others are ignored
			break;
		}
	      	break;
        }
    }

    if ((b == BUSCMD_UNLISTEN) || (b == BUSCMD_UNTALK) || ((b & BUSSEC_MASK) == BUSSEC_OPEN)) {
	st |= STAT_WAITEND;
    }

    if ((b == BUSCMD_UNLISTEN) || (b == BUSCMD_UNTALK) || ((b & BUSSEC_MASK) == BUSSEC_CLOSE)) {
	// unlisten, untalk, close
        bus->device = 0;
        bus->secondary = 0;

	led_set(IDLE);
    } else {
    	if (!is_config_device) {
		// not this device
        	st |= STAT_NODEV;
    	} else {
		led_set(ACTIVE);
	}
    }

#ifdef DEBUG_BUS
    debug_printf("BusAttention(%02x)->TrapDevice=%02x, st=%04x\n",
               b, bus->device, st | (bus->device << 8));
    debug_flush();
#endif

    st |= bus->device << 8;

    return st;
}


