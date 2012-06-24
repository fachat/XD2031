/* 
 * XD-2031 - A Commodore PET drive simulator using a serial-over-USB connected PC as host
 * Copyright (C) 2012  Andre Fachat <afachat@gmx.de>
 *
 * This file derived from ieee.c in
 * sd2iec - SD/MMC to Commodore serial bus interface/controller
   Copyright (C) 2007-2012  Ingo Korb <ingo@akana.de>

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


   ieee.c: IEEE-488 handling code by Nils Eilers <nils.eilers@gmx.de>

*/

#include "config.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "timer.h"
#include "debug.h"
#include "channel.h"
#include "status.h"
#include "errormsg.h"
#include "ieee.h"
#include "cmd.h"
#include "led.h"

#include "xs1541.h"

#define	DEBUG_IEEE	0
#define	DEBUG_IEEEX	1
#define	DEBUG_IEEE_DATA	0

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

#define FLAG_EOI 256
#define FLAG_ATN 512

#define IEEE_TIMEOUT_MS 64

#define EOI_RECVD       (1<<0)
#define COMMAND_RECVD   (1<<1)
#define ATN_POLLED      0xfffd	// -3      // 0xfd
#define TIMEOUT_ABORT   0xfffc	//-4      // 0xfc

/* ------------------------------------------------------------------------- */
/*  Global variables                                                         */
/* ------------------------------------------------------------------------- */

//uint8_t detected_loader = FL_NONE;      /* Workaround serial fastloader */
uint8_t device_address;                 /* Current device address */
static tick_t timeout;                  /* timeout on getticks()=timeout */

// error message handling
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

/**
 * struct ieeeflags_t - Bitfield of various flags, mostly IEEE-related
 * @eoi_recvd      : Received EOI with the last byte read
 * @command_recvd  : Command or filename received
 *
 * This is a bitfield for a number of boolean variables used
 */

volatile struct {
  uint8_t ieeeflags;
  enum {    BUS_IDLE = 0,
            BUS_FOUNDATN,
            BUS_ATNPROCESS,
            BUS_CMDWAIT,
            BUS_CMDPROCESS,
            BUS_SLEEP
  } bus_state;

  int8_t errnum;		// from interrupt between BUS_CMDWAIT and BUS_CMDPROCESS

  enum {    DEVICE_IDLE = 0,
            DEVICE_LISTEN,
            DEVICE_TALK
  } device_state;
  uint8_t secondary_address;
} ieee_data;

/* ------------------------------------------------------------------------- */
/*  Error channel handling                                                   */
/* ------------------------------------------------------------------------- */


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

/* ------------------------------------------------------------------------- */
/*  Interrupt handling                                                       */
/* ------------------------------------------------------------------------- */

static inline void ieee_interrupts_init(void)  {
  /* clear interrupt flag */
  PCIFR |= _BV(PCIF3);

  /* enable ATN in pin change enable mask 
  * translates to 
  *   PCMSK3 |= _BV(PCINT27)
  * which is ok for PD3
  */
  IEEE_PCMSK |= _BV(IEEE_PCINT);

  /* Enable pin change interrupt 3 (PCINT31..24) */
  PCICR |= _BV(PCIE3);

  //debug_putps("Done init ieee ints"); debug_putcrlf();
}

/* IEEE-488 ATN interrupt using PCINT */
static inline void set_atn_irq(uint8_t x) {
#if DEBUG
  debug_putps("ATN_IRQ:"); debug_puthex(x); debug_putcrlf();
#endif
  if (x)
    IEEE_PCMSK |= _BV(IEEE_PCINT);
  else
    IEEE_PCMSK &= (uint8_t) ~_BV(IEEE_PCINT);
}

/* ------------------------------------------------------------------------- */
/*  Initialization and very low-level bus handling                           */
/* ------------------------------------------------------------------------- */

static inline void set_eoi_state(uint8_t x)
{
  if(x) {                                           // Set EOI high
    IEEE_DDR_EOI &= (uint8_t)~_BV(IEEE_PIN_EOI);    // EOI as input
    IEEE_PORT_EOI |= (uint8_t)_BV(IEEE_PIN_EOI);    // Enable pull-up
  } else {                                          // Set EOI low
    IEEE_PORT_EOI &= (uint8_t)~_BV(IEEE_PIN_EOI);   // EOI low
    IEEE_DDR_EOI |= (uint8_t) _BV(IEEE_PIN_EOI);    // EOI as output
  }
}

/* Read port bits */
# define IEEE_ATN        (IEEE_INPUT_ATN  & _BV(IEEE_PIN_ATN))
# define IEEE_NDAC       (IEEE_INPUT_NDAC & _BV(IEEE_PIN_NDAC))
# define IEEE_NRFD       (IEEE_INPUT_NRFD & _BV(IEEE_PIN_NRFD))
# define IEEE_DAV        (IEEE_INPUT_DAV  & _BV(IEEE_PIN_DAV))
# define IEEE_EOI        (IEEE_INPUT_EOI  & _BV(IEEE_PIN_EOI))

#ifdef HAVE_7516X
# define ddr_change_by_atn() \
    if(ieee_data.device_state == DEVICE_TALK) ieee_ports_listen()
# define set_ieee_data(data) IEEE_D_PORT = (uint8_t) ~ data

  static inline void set_te_state(uint8_t x)
  {
    if(x) IEEE_PORT_TE |= _BV(IEEE_PIN_TE);
    else  IEEE_PORT_TE &= ~_BV(IEEE_PIN_TE);
  }

  static inline void set_ndac_state(uint8_t x)
  {
    if (x) IEEE_PORT_NDAC |= _BV(IEEE_PIN_NDAC);
    else   IEEE_PORT_NDAC &= ~_BV(IEEE_PIN_NDAC);
  }

  static inline void set_nrfd_state(uint8_t x)
  {
    if(x) IEEE_PORT_NRFD |= _BV(IEEE_PIN_NRFD);
    else  IEEE_PORT_NRFD &= ~_BV(IEEE_PIN_NRFD);
  }

  static inline void set_dav_state(uint8_t x)
  {
    if(x) IEEE_PORT_DAV |= _BV(IEEE_PIN_DAV);
    else  IEEE_PORT_DAV &= ~_BV(IEEE_PIN_DAV);
  }

  // Configure bus to passive/listen or talk
  // Toogle direction of I/O pins and safely avoid connecting two outputs

  static inline void ieee_ports_listen (void)
  {
    IEEE_D_DDR = 0;                                 // data ports as inputs
    IEEE_D_PORT = 0xff;                     // enable pull-ups for data lines
    IEEE_DDR_DAV &= (uint8_t) ~_BV(IEEE_PIN_DAV);   // DAV as input
    IEEE_DDR_EOI &= (uint8_t) ~_BV(IEEE_PIN_EOI);   // EOI as input
    set_te_state(0);                                // 7516x listen
    IEEE_DDR_NDAC |= _BV(IEEE_PIN_NDAC);            // NDAC as output
    IEEE_DDR_NRFD |= _BV(IEEE_PIN_NRFD);            // NRFD as output
    IEEE_PORT_DAV |= _BV(IEEE_PIN_DAV);             // Enable pull-up for DAV
    IEEE_PORT_EOI |= _BV(IEEE_PIN_EOI);             // Enable pull-up for EOI
  }

  static inline void ieee_ports_talk (void)
  {
    IEEE_DDR_NDAC &= (uint8_t)~_BV(IEEE_PIN_NDAC);  // NDAC as input
    IEEE_DDR_NRFD &= (uint8_t)~_BV(IEEE_PIN_NRFD);  // NRFD as input
    IEEE_PORT_NDAC |= _BV(IEEE_PIN_NDAC);           // Enable pull-up for NDAC
    IEEE_PORT_NRFD |= _BV(IEEE_PIN_NRFD);           // Enable pull-up for NRFD
    set_te_state(1);                                // 7516x talk enable
    IEEE_D_PORT = 0xff;                             // all data lines high
    IEEE_D_DDR = 0xff;                              // data ports as outputs
    set_dav_state(1);                               // Set DAV high
    IEEE_DDR_DAV |= _BV(IEEE_PIN_DAV);              // DAV as output
    set_eoi_state(1);                               // Set EOI high
    IEEE_DDR_EOI |= _BV(IEEE_PIN_EOI);              // EOI as output
  }

  static void inline ieee_bus_idle (void)
  {
    ieee_ports_listen();
    set_ndac_state(1);
    set_nrfd_state(1);
  }

#else   /* HAVE_7516X */
  /* ----------------------------------------------------------------------- */
  /*  Poor men's variant without IEEE bus drivers                            */
  /* ----------------------------------------------------------------------- */

  static inline void set_ndac_state (uint8_t x)
  {
    if(x) {                                             // Set NDAC high
      IEEE_DDR_NDAC &= (uint8_t)~_BV(IEEE_PIN_NDAC);    // NDAC as input
      IEEE_PORT_NDAC |= _BV(IEEE_PIN_NDAC);             // Enable pull-up
    } else {                                            // Set NDAC low
      IEEE_PORT_NDAC &= (uint8_t)~_BV(IEEE_PIN_NDAC);   // NDAC low
      IEEE_DDR_NDAC |= _BV(IEEE_PIN_NDAC);              // NDAC as output
    }
  }

  static inline void set_nrfd_state (uint8_t x)
  {
    if(x) {                                             // Set NRFD high
      IEEE_DDR_NRFD &= (uint8_t)~_BV(IEEE_PIN_NRFD);    // NRFD as input
      IEEE_PORT_NRFD |= _BV(IEEE_PIN_NRFD);             // Enable pull-up
    } else {                                            // Set NRFD low
      IEEE_PORT_NRFD &= (uint8_t)~_BV(IEEE_PIN_NRFD);   // NRFD low
      IEEE_DDR_NRFD |= (uint8_t) _BV(IEEE_PIN_NRFD);    // NRFD as output
    }
  }

  static inline void set_dav_state (uint8_t x)
  {
    if(x) {                                             // Set DAV high
      IEEE_DDR_DAV &= (uint8_t)~_BV(IEEE_PIN_DAV);      // DAV as input
      IEEE_PORT_DAV |= _BV(IEEE_PIN_DAV);               // Enable pull-up
    } else {                                            // Set DAV low
      IEEE_PORT_DAV &= (uint8_t)~_BV(IEEE_PIN_DAV);     // DAV low
      IEEE_DDR_DAV |= _BV(IEEE_PIN_DAV);                // DAV as output
    }
  }

# define set_te_state(dummy) do { } while (0)       // ignore TE

  static inline void set_ieee_data (uint8_t data)
  {
    IEEE_D_DDR = data;
    IEEE_D_PORT = (uint8_t) ~ data;
  }

  static inline void ieee_bus_idle (void)
  {
    IEEE_D_DDR = 0;                 // Data ports as input
    IEEE_D_PORT = 0xff;             // Enable pull-ups for data lines

    IEEE_DDR_DAV  &= (uint8_t) ~_BV(IEEE_PIN_DAV);  // DAV as input
    IEEE_PORT_DAV |= _BV(IEEE_PIN_DAV);             // Enable pull-up for DAV

    IEEE_DDR_EOI  &= (uint8_t) ~_BV(IEEE_PIN_EOI);  // EOI as input
    IEEE_PORT_EOI |= _BV(IEEE_PIN_EOI);             // Enable pull-up for EOI

    IEEE_DDR_NDAC &= (uint8_t) ~_BV(IEEE_PIN_NDAC); // NDAC as input
    IEEE_PORT_NDAC |= _BV(IEEE_PIN_NDAC);           // Enable pull-up for NDAC

    IEEE_DDR_NRFD &= (uint8_t) ~_BV(IEEE_PIN_NRFD); // NRFD as input
    IEEE_PORT_NRFD |= _BV(IEEE_PIN_NRFD);           // Enable pull-up for NRFD

    IEEE_DDR_ATN  &= (uint8_t) ~_BV(IEEE_PIN_ATN);  // ATN as input
    IEEE_PORT_ATN |= _BV(IEEE_PIN_ATN);             // Enable pull-up for ATN
  }

//  static void ieee_ports_listen (void) {
//    ieee_bus_idle();
//  }

  static void ieee_ports_talk (void) {
    ieee_bus_idle();
  }

# define ddr_change_by_atn() do { } while (0)

#endif  /* HAVE_7516X */

/********************************************************************************/

/* Init IEEE bus */
void ieee_init(void) {
  ieee_bus_idle();

  /* Prepare IEEE interrupts */
  ieee_interrupts_init();

  /* Read the hardware-set device address */
//  device_hw_address_init();
  delayms(1);
//  device_address = device_hw_address();
  device_address = 8;

  /* Init vars and flags */
  command.command_length = 0;
  ieee_data.ieeeflags &= (uint8_t) ~ (COMMAND_RECVD || EOI_RECVD);
}
//void bus_init(void) __attribute__((weak, alias("ieee_init")));

/* Interrupt routine that simulates the hardware-auto-acknowledge of ATN
   at falling edge of ATN. If pin change interrupts are used, we have to
   check for rising or falling edge in software first! */
// Note: translates to ISR(PCINT3_vect)
IEEE_ATN_HANDLER {
#ifdef IEEE_PCMSK
  if(!IEEE_ATN) {
#else
  {
#endif
    ddr_change_by_atn();        /* Switch NDAC+NRFD to outputs */
    set_ndac_state(0);          /* Set NDAC and NRFD low */
    set_nrfd_state(0);
  }
}

/* ------------------------------------------------------------------------- */
/*  Byte transfer routines                                                   */
/* ------------------------------------------------------------------------- */

/**
 * returns true if the ATN value is as expected.
 */
static inline uint8_t ieee_is_atn(uint8_t value) {
	if (value) {
		return !IEEE_ATN;
	} else {
		return IEEE_ATN;
	}
}


/**
 * ieee_getc - receive one byte from the IEEE-488 bus
 *
 * This function tries receives one byte from the IEEE-488 bus and returns it
 * if successful. Flags (EOI, ATN) are passed in the more significant byte.
 * Returns TIMEOUT_ABORT if a timeout occured
 */

int16_t ieee_getc(int under_atn) {
  int c = 0;

  // wait until computer has completed previous transfer cycle
  do {              /* wait for controller to remove data from bus */
	// check ATN but only when not under ATN
	if ((!under_atn) && (!IEEE_ATN)) {
		// yes, then exit
		return ATN_POLLED;
	}
  } while (!IEEE_DAV);

  set_ndac_state(0);            /* data not yet accepted */
  set_nrfd_state(1);            /* ready for new data */

  // PET waits for NRFD high 
  // wait for other devices to be ready
  // before we start the timeout 
  while (!IEEE_NRFD) {
	// wait interrupted by ATN?
	if (!ieee_is_atn(under_atn)) {
		// yes, then exit
		return ATN_POLLED;
	}
  }		

  /* Wait for DAV low */
//  timeout = getticks() + MS_TO_TICKS(IEEE_TIMEOUT_MS);
  /* wait for data valid */
  do {   
	// check timeout                       
//    	if(time_after(getticks(), timeout)) {
//		return TIMEOUT_ABORT;
//	}
	// check ATN
	if (!ieee_is_atn(under_atn)) {
		// yes, then exit
		return ATN_POLLED;
	}
  } while (IEEE_DAV);

  set_nrfd_state(0);    /* not ready for new data, data not yet read */

  c = (uint8_t) ~ IEEE_D_PIN;   /* read data */

  if(!IEEE_EOI) c |= FLAG_EOI;  /* end of transmission? */

  set_ndac_state(1);            /* data accepted, read complete */

  // In the CBM drives here an ATN command is actually interpreted
  // and action is taken. 
  //
  // For now we just hope we're fast enough

  if (!under_atn) {
    /* Wait for DAV high, check timeout */
//  timeout = getticks() + MS_TO_TICKS(IEEE_TIMEOUT_MS);
    do {              /* wait for controller to remove data from bus */
//    if(time_after(getticks(), timeout)) {
//	return TIMEOUT_ABORT;
//    }
	// check ATN but only when not under ATN
	if ((!under_atn) && (!IEEE_ATN)) {
		// yes, then exit
		return ATN_POLLED;
	}
    } while (!IEEE_DAV);

    // set NDAC low
    set_ndac_state(0);            /* next data not yet accepted */
  }

  return c;
}


/**
 * ieee_putc - send a byte
 * @data    : byte to be sent
 * @with_eoi: Flags if the byte should be send with an EOI condition
 *
 * This function sends the byte data over the IEEE-488 bus and pulls
 * EOI if it is the last byte.
 * Returns
 *  0 normally,
 * ATN_POLLED if ATN was set or
 * TIMEOUT_ABORT if a timeout occured
 * On negative returns, the caller should return to the IEEE main loop.
 */

static uint16_t ieee_putc(uint8_t data, const uint8_t with_eoi, volatile channel_t *chan) {

//debug_puts("with_eoi="); debug_puthex(with_eoi); debug_putcrlf();

  // make sure those are high
  set_dav_state (1);
  set_eoi_state (1);

  // set data
  set_ieee_data (data);

  // check if ATN has been activated
  if(!IEEE_ATN) return ATN_POLLED;
  delayus(11);    /* Allow data to settle */
  if(!IEEE_ATN) return ATN_POLLED;

  /* Wait for NRFD high , check timeout */
//  timeout = getticks() + MS_TO_TICKS(IEEE_TIMEOUT_MS);
  do {
    if(!IEEE_ATN) {
	return ATN_POLLED;
    }
//    if(time_after(getticks(), timeout)) {
//	return TIMEOUT_ABORT;
//    }
  } while (!IEEE_NRFD);

  // set EOI state
  set_eoi_state (!with_eoi);

  // signal data available
  set_dav_state(0);

  delayus(11);    /* Allow data to settle */

  // NOTE: As the PET timeout handling is BROKEN, we actually should 
  // make sure _here_ that we have enough data for the next byte
  // and get the data if not.
  // The only place where the PET reliably waits without a timeout is
  // when it waits for DAV to go high, i.e. here...
  channel_preloadp(chan);
  
  /* Wait for NRFD low, check timeout *
  timeout = getticks() + MS_TO_TICKS(IEEE_TIMEOUT_MS);
  do {
    if(!IEEE_ATN) {
	return ATN_POLLED;
    }
    if(time_after(getticks(), timeout)) {
	return TIMEOUT_ABORT;
    }
  } while (IEEE_NRFD);
*/

  /* Wait for NDAC high , check timeout */
//  timeout = getticks() + MS_TO_TICKS(IEEE_TIMEOUT_MS);
  do {
    if(!IEEE_ATN) {
	return ATN_POLLED;
    }
//    if(time_after(getticks(), timeout)) {
//	return TIMEOUT_ABORT;
//    }
  } while (!IEEE_NDAC);

  // clear dav/eoi lines
  set_dav_state(1);
  set_eoi_state(1);

  // let the lines settle
  delayus(11);

  /* Wait for NDAC lo , check timeout */
//  timeout = getticks() + MS_TO_TICKS(IEEE_TIMEOUT_MS);
  do {
    if(!IEEE_ATN) {
	return ATN_POLLED;
    }
//    if(time_after(getticks(), timeout)) {
//	return TIMEOUT_ABORT;
//    }
  } while (IEEE_NDAC);

  return 0;
}

/* ------------------------------------------------------------------------- */
/*  Listen+Talk-Handling                                                     */
/* ------------------------------------------------------------------------- */

static int16_t ieee_listen_handler (uint8_t cmd)
/* Receive characters from IEEE-bus and write them to the
   listen buffer adressed by ieee_data.secondary_address.
   If ATN is polled, return with ATN_POLLED
*/
{
  volatile channel_t *chan;
  int16_t c;

  ieee_data.secondary_address = cmd & 0x0f;
  chan = channel_find(ieee_secaddr_to_channel(ieee_data.secondary_address));

  /* Abort if there is no buffer or it's not open for writing */
  /* and it isn't an OPEN command                             */
  /* Note: this could be handled by a command channel similar to the talk_handler (AF) */
  if ((chan == NULL || !channel_is_writable(chan)) && (cmd & 0xf0) != 0xf0) {
#if DEBUG
    debug_putc('c');
#endif
    return -1;
  }

#if DEBUG_IEEE
  switch(cmd & 0xf0) {
    case 0x60:
      debug_putps("DATA L ");
      break;
    case 0xf0:
      debug_putps("OPEN ");
      break;
    default:
      debug_putps("Unknown LH! ");
      break;
  }
  debug_puthex(ieee_data.secondary_address);
  debug_putcrlf();
#endif

  c = -1;
  for(;;) {
    /* Get a character ignoring timeout but but watching ATN */
    c = ieee_getc(0);
    //while((c = ieee_getc(0)) < 0);
    // break out of the loop on ATN
    if (c == ATN_POLLED || c < 0) {
	return c;
    }

    debug_putc('<');
    if (c & FLAG_EOI) {
#if DEBUG_IEEE
      debug_putps("EOI ");
#endif
//_delay_ms(10);
//term_putcrlf(); 
      ieee_data.ieeeflags |= EOI_RECVD;
    } else {
      ieee_data.ieeeflags &= ~EOI_RECVD;
    }

#if DEBUG_IEEE_DATA
    debug_puthex(c); debug_putc(' ');
#endif
    c &= 0xff; /* needed for isprint */
#if DEBUG_IEEE_DATA
    if(isprint(c)) debug_putc(c); else debug_putc('?');
    debug_putcrlf();
#endif
//_delay_ms(10);
//term_putcrlf();

    if((cmd & 0x0f) == 0x0f || (cmd & 0xf0) == 0xf0) {
      if (command.command_length < CONFIG_COMMAND_BUFFER_SIZE) {
        command.command_buffer[command.command_length++] = c;
      }
      if (ieee_data.ieeeflags & EOI_RECVD) {
        /* Filenames are just a special type of command =) */
        ieee_data.ieeeflags |= COMMAND_RECVD;
      }
    } else {
      /* REL files must be syncronized on EOI */
      /* Note: this should be done simply by using read/write channels for REL files.*/
      chan = channel_put(chan, c, ieee_data.ieeeflags & EOI_RECVD);
      if (chan == NULL) {
	return -2;
      }
#if 0
      /* Flush buffer if full */
      if (buf->mustflush) {
        if (buf->refill(buf)) return -2;
        /* Search the buffer again,                     */
        /* it can change when using large buffers       */
        buf = find_buffer(ieee_data.secondary_address);
      }

      buf->data[buf->position] = c;
      mark_buffer_dirty(buf);

      if (buf->lastused < buf->position) {
	buf->lastused = buf->position;
      }
      buf->position++;

      /* Mark buffer for flushing if position wrapped */
      if (buf->position == 0) {
	buf->mustflush = 1;
      }

      /* REL files must be syncronized on EOI */
      if(buf->recordlen && (ieee_data.ieeeflags & EOI_RECVD)) {
        if (buf->refill(buf)) return -2;
      }
#endif
    }   /* else-buffer */
  }     /* for(;;) */
}

static uint8_t ieee_talk_handler (void)
{
  volatile channel_t *chan;
  //uint8_t finalbyte;
  uint8_t c;
  uint16_t res;	// result from a ieee_putc, i.e. either ATN_POLLED or TIMEOUT_ABORT
  uint8_t eof;

  // switch ports to talk
  ieee_ports_talk();

  chan = channel_find(ieee_secaddr_to_channel(ieee_data.secondary_address));
  if(chan == NULL) {
    if (ieee_data.secondary_address == 15) {
      // open error channel
      int8_t e = channel_open(ieee_secaddr_to_channel(15), WTYPE_READONLY, &ieee_status_provider, NULL);

      debug_printf("open status channel -> %d\n", e);

      chan = channel_find(ieee_secaddr_to_channel(ieee_data.secondary_address));
    }
    if (chan == NULL) {
      return -1;
    }
  }

  //channel_preloadp(chan);

  while (channel_has_more(chan)) {

//led_on();
    // send all bytes that are currently in the channel buffer
    do {
      c = channel_current_byte(chan);
      eof = channel_current_is_eof(chan);
#if DEBUG_IEEE_DATA
debug_puthex(c);
#endif
      res = ieee_putc(c, eof, chan);
#if 0
      finalbyte = (buf->position == buf->lastused);
      c = buf->data[buf->position];
      if (finalbyte && buf->sendeoi) {
        /* Send with EOI */
        res = ieee_putc(c, 1);
        if(!res) { 
	  debug_puts("EOI: ");
	}
      } else {
        /* Send without EOI */
        res = ieee_putc(c, 0);
      }
#endif
      if(res) {
#if DEBUG_IEEE
        if(res==TIMEOUT_ABORT) {
          debug_putps("*** TIMEOUT ABORT***"); debug_putcrlf();
        }
        if(res!=ATN_POLLED) {
          debug_putc('c'); debug_puthex(res);
        }
#endif
        return 1;
      } else {
#if DEBUG_IEEE_DATA
        debug_putc(eof ? '-' : '>');
        debug_puthex(c); debug_putc(' ');
        if(isprint(c)) debug_putc(c); else debug_putc('?');
        debug_putcrlf();
#endif
//_delay_us(200);
/*
term_putc(eof ? '-' : '>');
term_putc(' ');
term_putc(' ');
term_puts("   "); term_puts("___"); 
term_putcrlf();
*/
      }
      // channel_next() commits the byte that has been transferred
      // advances to the next byte and returns -1 if there is no next
      // byte in the current buffer. (there may be further bytes in a
      // further buffer)
    } while (channel_next(chan));
    // now channel buffer is empty
#if DEBUG_IEEE    
    debug_puts("channel next false"); debug_putcrlf();
#endif

    // stop sending if channel is closed after EOI
    if (!channel_has_more(chan)) {
      if (/*ieee_data.secondary_address == 0 ||*/  ieee_data.secondary_address == 15) {
	// autoclose when load is done, or after reading status channel
	channel_close(ieee_secaddr_to_channel(ieee_data.secondary_address));
      }
      break;
    }

    chan = channel_refill(chan);
    if (chan == NULL) {
      return -1;
    }
  }
  return 0;
}

static void _cmd_callback(int8_t errnum) {
    ieee_data.bus_state = BUS_CMDPROCESS;
    ieee_data.errnum = errnum;
//led_toggle();
}

static void cmd_handler (void)
{
  /* Handle commands and filenames */
  if (ieee_data.ieeeflags & COMMAND_RECVD) {
# if 0	/* def HAVE_HOTPLUG */
    /* This seems to be a nice point to handle card changes */
    /* No. */
    if (disk_state != DISK_OK) {
      set_busy_led(1);
      /* If the disk was changed the buffer contents are useless */
      if (disk_state == DISK_CHANGED || disk_state == DISK_REMOVED) {
        free_multiple_buffers(FMB_ALL);
        change_init();
        fatops_init(0);
      } else {
        /* Disk state indicated an error, try to recover by initialising */
        fatops_init(1);
      }
      update_leds();
    }
# endif
    if (ieee_data.secondary_address == 0x0f) {
      doscommand(&command);                   /* Command channel */

      // open/prepare the status channel
      //int8_t e = channel_open(15, 0, &ieee_status_provider, NULL);

    } else {
      // to avoid data races, we need to set bus_state here
      ieee_data.bus_state = BUS_CMDWAIT;
      int8_t rv = ieee_file_open(ieee_data.secondary_address, &command, _cmd_callback);
      if (rv < 0) {
	// open ran into an error
	// -- errormsg should be already set, so nothing left to do here
	// but we need to reset bus_state so command loop continues processing
        ieee_data.bus_state = BUS_ATNPROCESS;
      }	
    }
    command.command_length = 0;
    ieee_data.ieeeflags &= (uint8_t) ~COMMAND_RECVD;
  } /* COMMAND_RECVD */

  /* We're done, clean up unused buffers */
  // should be done in doscommand / file_open
  //free_multiple_buffers(FMB_UNSTICKY);
  /* oops.
  d64_bam_commit();
  */
}


/* ------------------------------------------------------------------------- */
/*  Main loop                                                                */
/* ------------------------------------------------------------------------- */

static int16_t cmd = 0;

void ieee_mainloop_init(void) {
  cmd = 0;

  set_error(&error, ERROR_DOSVERSION);

  ieee_data.bus_state = BUS_IDLE;
  ieee_data.device_state = DEVICE_IDLE;
}

/**
 * (local) method to set bus to idle state
 */
static void ieee_set_idle() {
        ieee_bus_idle();
        //update_leds();
        ieee_data.bus_state = BUS_IDLE;
	status_ieee_set(STATUS_IDLE);
}

/**
 * called from main() to set bus offline
 */
void ieee_set_offline() {
        set_atn_irq(0);
        ieee_bus_idle();
        set_error(&error, ERROR_OK);
        //set_busy_led(0);
#if DEBUG_IEEE
        debug_putps("ieee.c/sleep "); //set_dirty_led(1);
#endif
	status_ieee_set(STATUS_OFFLINE);

        /* Wait until the sleep key is used again 
        while (!key_pressed(KEY_SLEEP))
          system_sleep();
        reset_key(KEY_SLEEP);
	*/
        ieee_data.bus_state = BUS_SLEEP;
}

static inline void ieee_wait_atnhi() {
          while(!IEEE_ATN);
}

/**
 * called from main() to wake up bus from offline
 */
void ieee_set_online() {
        set_atn_irq(1);
	ieee_set_idle();
}

void ieee_mainloop_iteration(void) {

    int newcmd = 0;

    switch(ieee_data.bus_state) {
      case BUS_SLEEP:                               /* BUS_SLEEP */
	// do nothing
        break;

      case BUS_IDLE:                                /* BUS_IDLE */
        if(IEEE_ATN) {   ;               /* wait for ATN */
          //debug_putc('x');
	  break;
 	}
#if DEBUG_IEEE
        debug_putc('!');
#endif
        ieee_data.bus_state = BUS_FOUNDATN;
	// fall-through on ATN
      case BUS_FOUNDATN:                            /* BUS_FOUNDATN */
        ieee_data.bus_state = BUS_ATNPROCESS;
        newcmd = ieee_getc(1);
        if(newcmd < 0) {
#if DEBUG_IEEE
          debug_putc('c');
#endif
	  ieee_set_idle();
          break;
        } else {
	  cmd = newcmd & 0xFF;
        }
	// fall-through
      case BUS_ATNPROCESS:                          /* BUS_ATNPROCESS */
#if DEBUG_IEEE
	if (1 || cmd == 0xf0) {
		debug_printf("ATN %x\n", cmd);
	}
#endif
        if (cmd == 0x3f) {                           /* UNLISTEN */
          if(ieee_data.device_state == DEVICE_LISTEN) {
            ieee_data.device_state = DEVICE_IDLE;
#if DEBUG_IEEE
            debug_putps("UNLISTEN");
	    debug_putcrlf();
#endif
          }
          ieee_data.bus_state = BUS_IDLE;
          break;
        } else 
        if (cmd == 0x5f) {                           /* UNTALK */
          if(ieee_data.device_state == DEVICE_TALK) {
            ieee_data.device_state = DEVICE_IDLE;
#if DEBUG_IEEE
            debug_putps("UNTALK");
	    debug_putcrlf();
#endif
          }
          ieee_data.bus_state = BUS_IDLE;
          break;
        } else 
        if (cmd == (0x40 + device_address)) {        /* TALK */
#if DEBUG_IEEE
          debug_putps("TALK ");
          debug_puthex(device_address); debug_putcrlf();
#endif
          ieee_data.device_state = DEVICE_TALK;
          /* disk drives never talk immediatly after TALK, so stay idle
             and wait for a secondary address given by 0x60-0x6f DATA */
          ieee_data.bus_state = BUS_IDLE;
          break;
        } else 
        if (cmd == (0x20 + device_address)) {        /* LISTEN */
          ieee_data.device_state = DEVICE_LISTEN;
#if DEBUG_IEEE
          debug_putps("LISTEN ");
          debug_puthex(device_address); debug_putcrlf();
#endif
          ieee_data.bus_state = BUS_IDLE;
          break;
        } else 
        if ((cmd & 0xf0) == 0x60) {                  /* DATA */

          /* 8250LP sends data while ATN is still active, so wait
             for bus controller to release ATN or we will misinterpret
             data as a command */
          while(!IEEE_ATN);

          if(ieee_data.device_state == DEVICE_LISTEN) {
            uint16_t v = ieee_listen_handler(cmd);
	    if (v == ATN_POLLED) {
          	ieee_data.bus_state = BUS_FOUNDATN;
            	cmd_handler();
	    }
            break;
          } else 
          if (ieee_data.device_state == DEVICE_TALK) {
            ieee_data.secondary_address = cmd & 0x0f;
#if DEBUG_IEEE
            debug_putps("DATA T ");
            debug_puthex(ieee_data.secondary_address);
            debug_putcrlf();
#endif
            while(!IEEE_ATN);

	    uint16_t t = ieee_talk_handler();
	    //debug_puthex(t); debug_putcrlf();
            if(t == TIMEOUT_ABORT) {
              ieee_data.device_state = DEVICE_IDLE;
            }
	    ieee_set_idle();
            //ieee_data.bus_state = BUS_IDLE;
            break;
          } else {
	    ieee_set_idle();
            //ieee_data.bus_state = BUS_IDLE;
            break;
          }
        } else 
        if (ieee_data.device_state == DEVICE_IDLE) {
	  ieee_set_idle();
          //ieee_data.bus_state = BUS_IDLE;
          break;
          /* ----- if we reach this, we're LISTENer or TALKer ----- */
        } else if ((cmd & 0xf0) == 0xe0) {                  /* CLOSE */
          ieee_data.secondary_address = cmd & 0x0f;
#if DEBUG_IEEE
	  debug_printf("CLOSE %x\n", ieee_data.secondary_address);
//          debug_putps("CLOSE ");
//          debug_puthex(ieee_data.secondary_address);
//          debug_putcrlf();
#endif
          /* Close all buffers if sec. 15 is closed */
          if(ieee_data.secondary_address == 15) {
	    ieee_channel_close_all();
          } else {
            /* Close a single buffer */
	    channel_close(ieee_secaddr_to_channel(ieee_data.secondary_address));
          }
          ieee_data.bus_state = BUS_IDLE;
          break;
        } else if ((cmd & 0xf0) == 0xf0) {                  /* OPEN */

          while(!IEEE_ATN);

          uint16_t v = ieee_listen_handler(cmd);
	  if (v == ATN_POLLED) {
		ieee_data.bus_state = BUS_FOUNDATN;
          	cmd_handler();
	  }
          break;
        } else {
          /* Command for other device or unknown command */
          ieee_data.bus_state = BUS_IDLE;
        }
      break;
      case BUS_CMDWAIT:
	// wait until the command got a reply from the server
	// which changes the state during interrupt callback
	// and sets it back to BUS_CMDPROCESS
	break;
      case BUS_CMDPROCESS:
//debug_puts("cmdprocess error: "); debug_puthex(ieee_data.errnum); debug_putcrlf();
	if (ieee_data.errnum != 0) {
		set_error(&error, ieee_data.errnum);
		channel_close(ieee_secaddr_to_channel(ieee_data.secondary_address));
	} else {
		// really only does something on read-only channels
		channel_preload(ieee_secaddr_to_channel(ieee_data.secondary_address));
	}
	ieee_data.bus_state = BUS_IDLE; //BUS_ATNPROCESS;
	break;
    }   /* switch   */
//  }     /* for()    */
}
//void bus_mainloop(void) __attribute__ ((weak, alias("ieee_mainloop")));
