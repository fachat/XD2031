
/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

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

****************************************************************************/

#ifdef HAS_IEC

/**
 * Hardware-independent IEC layer 
 */

#include <ctype.h>
#include <delay.h>

#include "iechw.h"
#include "bus.h"
#include "timer.h"

#include "debug.h"
#include "led.h"
#include "system.h"

#undef DEBUG_BUS

// Prototypes

static void talkloop(void);
static void listenloop(void);

// bus state
static bus_t bus;

// This status value has in its lower byte the status similar as it
// is used in the Commodore line of computers, mostly used for 0x40 as EOF.
// In the upper byte it contains the current "first" command byte, i.e.
// whether we are talking or listening. This is returned from the bus layer
// so we can react on it.
static int16_t ser_status = 0;

#define isListening()   ((ser_status&0xe000)==0x2000)
#define isTalking()     ((ser_status&0xe000)==0x4000)
#define waitAtnHi()     (ser_status&STAT_WAITEND)


/***************************************************************************
 * Listen handling
 */

// check whether we got an ATN; this depends on whether we are 
// under ATN (when receiving commands) or not under ATN (when receiving data)
// returns !=0 when ATN has changed
static uint8_t checkatn(uint8_t underatn) 
{
//	if (underatn) {
//		return satnishi();
//	} else {
//		return satnislo();
//	}
	if (underatn) {
		if (satnishi()) {
			//if (!satna()) {
				dataforcelo();
			//}
			return -1;
		} else {
			return 0;
		}
	} else {
		if (satnislo()) {
			//if (satna()) {
				dataforcelo();
			//}
			return -1;
		} else {
			return 0;
		}
	}
}

// read a byte from IEEE - E9C9 in VC1541
static int16_t iecin(uint8_t underatn)
{
	uint8_t eoi = 0;
	uint8_t port;
	uint8_t cnt = 8;
	uint8_t data = 0;


	do {
		if (checkatn(underatn)) 
			return -1;
	} while (is_port_clklo(read_debounced()));
	
	// unlikely race condition:
	// between the checkatn() and the is_port_clklo() the 
	// sectalk ends the previous atn sequence with a 
	// atn hi and clk hi - so we might fall through here;
	// thus check ATN here again to be sure
	if (checkatn(underatn)) {
		return -1;
	}
	
	datahi();

	// wait until data is really hi (other devices may delay this)
	// actualy there's a vc1541 patch in the wild from e9dc to ff20 that 
	// does exactly that but I found only afterwards...
	do {
		if (checkatn(underatn))
			return -1;
	} while (is_port_datalo(read_debounced()));

	// set timer with 256 us
	timer_set_us(256);

	do {
		// e9df (vc1541)

		if (checkatn(underatn)) {
			return -1;
		}

		if (timer_is_timed_out()) { 
			// handle EOI condition
			// e9f2
			datalo();
			// at least 23 cycles + 43+ for C64 bad video lines
			delayus(80);

			datahi();

			// e9fd
			do {
				if (checkatn(underatn)) {
					return -1;
				}
			} while (is_port_clkhi(read_debounced()));
			
			eoi = 1;

			break;
		}

	} while (is_port_clkhi(read_debounced()));

	// shift in all bits
	do {
		// ea0b
		do {
			port = read_debounced();
		} while (is_port_clklo(port));

		data >>= 1;
		if (is_port_datahi(port)) {
			data |= 128;
		}

		// ea1a
		do {
			if (checkatn(underatn)) {
				return -1;
			}
		} while (is_port_clkhi(read_debounced()));

		cnt--;

	} while (cnt > 0);

	datalo();

	return (0xff & data) | (eoi ? 0x4000 : 0);
}

static void listenloop() {
#ifdef DEBUG_BUS
	debug_putc('L');
#endif

	int16_t c;

	do {
		disable_interrupts();
		// read byte from IEC
		c = iecin(0);
		enable_interrupts();
		if (c < 0) {
			break;
		}
            	ser_status = bus_sendbyte(&bus, c, BUS_SYNC | ((c & 0x4000) ? BUS_FLUSH : 0));
        } while (1);

	return;
}

/***************************************************************************/

// more info see here: https://groups.google.com/forum/?hl=de&fromgroups=#!msg/comp.sys.cbm/e4qxrtt5RP0/0q1EVUkV8moJ

static int16_t iecout(uint8_t data, uint8_t witheoi) {

	uint8_t port;	
	uint8_t cnt = 8;

	if (checkatn(0)) {
		return -1;
	}

	// just in case, release the data line
	datahi();

	// make sure data is actually lo (done by controller)
	do {
		if (checkatn(0)) {
			return -1;
		}
	} while (is_port_datahi(read_debounced()));

	// e916 ff
	port = read_debounced();
	
	delayus(60);	// sd2iec
	
	// e91f
	clkhi();

	// wait for the listener to release data, signalling ready for data
	do {
		if (checkatn(0)) {
			return -1;
		}
	} while (is_port_datalo(read_debounced()));

	if (witheoi || is_port_datahi(port)) {
		// signal the EOI
		// wait for data low as acknowledge from the listener
		do {
			if (checkatn(0)) {
				return -1;
			}
		} while (is_port_datahi(read_debounced()));

		// and wait for DATA to go back up
		do {
			if (checkatn(0)) {
				return -1;
			}
		} while (is_port_datalo(read_debounced()));

		// done signalling EOI
	}

	// e94b
	clklo();
	
	// e958
	do {
		// e95c
		if (is_port_datalo(read_debounced())) {
			return -1;
		}

		if (data & 1) {
			datahi();
		} else {
			datalo();
		}
		data >>= 1;

		// here and in the next delay, maybe build a switch to 
		// support faster speeds for VIC-20
		delayus(80);

		clkhi();

		// C64: min about 15 cycles to detect it, plus 43+ bad line cycles
		// fef3
		delayus(70);

		// fefb
		clklo();
		datahi();

		cnt--;
	} while (cnt > 0);


	// wait for the host to set data lo
	do {
		if (checkatn(0)) {
			return -1;
		}
	} while (is_port_datahi(read_debounced()));	

	return 0;
}

static void talkloop()
{
        int16_t er;
        uint8_t c;

	do {
            	ser_status = bus_receivebyte(&bus, &c, BUS_PRELOAD | BUS_SYNC);
#ifdef BUS_DEBUG_DATA
		debug_printf("rx->iecout: %02\n", c); debug_flush();
#endif

		disable_interrupts();
		// send byte to IEC
		er = iecout(c, ser_status & 0x40);
		enable_interrupts();

		if (er >= 0) {
            		ser_status = bus_receivebyte(&bus, &c, BUS_SYNC);
		}
	} while (er >= 0 && ((ser_status & 0xff) == 0));
}

/***************************************************************************
 * main IEEE "loop"
 * The actual loop is outside this method to handle other functions outside
 * the IEEE stuff. The outside loop calls this one on each iteration.
 */

void iec_mainloop_iteration(void)
{
        int16_t cmd = 0;

        ser_status=0;

	// only do something on ATN low
	if (satnishi()) {
		return;
	}

#if 0 //def DEBUG_BUS
	debug_printf("start of cycle: stat=%04x, atn=%d, atna=%d, data=%d, clk=%d", 
		ser_status, satnishi(), satna(), dataishi(), clkishi()); 
	//debug_putcrlf();
#endif

	// This delay fixes a problem that would otherwise require an 11ms
	// wait at the end of this function when we don't do listenloop() or talkloo().
	// It looks as if in this case the C64 first does ATN low, but has not
	// correctly figured out the other lines yet, thus a fall-though into
	// iecin and after wiggling with clk once, the C64 hangs.
	delayus(60);

	disable_interrupts();

	clkhi();

	datalo();

	// acknowledge ATN
	satnalo();

	// E87B (vc1541)
	do {
		if (satnishi()) {
			dataforcelo();
			goto cmd;
		}
	} while (is_port_clklo(read_debounced()));

	// on the C64, CLK is set high directly after setting ATN
	// but here we are possibly faster than that, so do a delay
	delayus(20);

        // Loop to get commands during ATN lo ----------------------------

	do {
		disable_interrupts();
		// get byte (under ATN) - call to E9C9
		cmd = iecin(1);
		enable_interrupts();

		if (cmd >= 0) {
			ser_status = bus_attention(&bus, 0xff & cmd);

			if (waitAtnHi()) {
				// e902
				dataforcelo();
				// wait for ATN hi
				while (satnislo());
				// and exit loop
				goto cmd;
			}
		}

		// cmd might be <0 if iecin ran into an ATN hi condition
		// If ATN should have been re-asserted here, do we really
		// have a problem just staying in the loop?
		// TODO: check if cmd<0 condition is needed
	} while (satnislo());
	
        // ---------------------------------------------------------------
	// ATN is high now
	// bus_attention has set status what to do
	// now transfer the data
cmd:

#if 0 ///def DEBUG_BUS
	debug_printf("stat=%04x, atn=%d, atna=%d, data=%d, clk=%d", 
		ser_status, satnishi(), satna(), dataishi(), clkishi()); 
	debug_putcrlf();
#endif

	// E8D7
	satnahi();

	if(isListening())
        {
		listenloop();

		clkhi();
        } else
        {

		if (isTalking()) {
			// does not work without delay (why?)
			// but this is fast enough so I won't complain
			// Duration is a wild guess though, which seems to work
			delayus(60);

			datahi();
			clklo();
			talkloop();

			datahi();
		} else {
			clkhi();
			datahi();
		}
        }

#if 0 //def DEBUG_BUS
	debug_printf("end of cycle: stat=%04x, atn=%d, atna=%d, data=%d, clk=%d", 
		ser_status, satnishi(), satna(), dataishi(), clkishi()); 
	debug_putcrlf();
#endif

#ifdef DEBUG_BUS
	debug_putc('X'); debug_flush();
#endif	
        return;
}

/***************************************************************************
 * Init code
 */
void iec_init(uint8_t deviceno) {

        iechw_setup();

	// register bus instance
	bus_init_bus("iec", &bus);
}

#endif // HAS_IEC
