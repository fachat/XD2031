
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

#define DEBUG_BUS

// Prototypes

static void talkloop(void);
static void listenloop(void);

#define isListening()   ((ser_status&0xf000)==0x2000)
#define isTalking()     ((ser_status&0xf000)==0x4000)

// bus state
static bus_t bus;

// TODO: make that ... different...
static int16_t ser_status = 0;


/***************************************************************************
 * Listen handling
 */

// check whether we got an ATN; this depends on whether we are 
// under ATN (when receiving commands) or not under ATN (when receiving data)
// returns !=0 when ATN has changed
static uint8_t checkatn(uint8_t underatn) 
{
	if (underatn) {
		return satnishi();
	} else {
		return satnislo();
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

	datahi();

	// set timer with 256 us
	timer_set_us(256);

	do {
		if (checkatn(underatn)) {
			return -1;
		}

		if (timer_is_timed_out()) { 
			// handle EOI condition
			datalo();
			delayus(50);
			datahi();

			do {
				if (checkatn(underatn)) {
					return -1;
				}
			} while (is_port_clkhi(read_debounced()));
			
			eoi = 1;

			break;
		}

	} while (is_port_clkhi(read_debounced()));

led_on();

	// shift in all bits
	do {
		do {
			port = read_debounced();
		} while (is_port_clklo(port));

		data >>= 1;
		if (is_port_datahi(port)) {
			data |= 128;
		}

		do {
			if (checkatn(underatn)) 
				return -1;
		} while (is_port_clkhi(read_debounced()));

		cnt--;
	} while (cnt > 0);

	datalo();

	return 0xff & data;
}

static void listenloop() {
#ifdef DEBUG_BUS
	debug_putc('L');
#endif
/*
        int er, c;
        while(((er=liecin(&c))&E_ATN)!=E_ATN) {
            ser_status = bus_sendbyte(&bus, c, er & E_EOI);
        }
	// if did not stop due to ATN, set to idle,
	// otherwise stay in rx mode
	if (er != E_ATN) {
	    setidle();
	}
*/
        return;
}

/***************************************************************************/

static void talkloop()
{
        int16_t er /*,sec*/;
        uint8_t c;
/*
#ifdef DEBUG_BUS
	debug_putc('T'); debug_flush();
#endif
        settx();            // enables sending 

        er=0;
        //sec=secadr&0x0f;

        while (!er) {

            // wait nrfd hi 
            do {
                if( atnislo() ) {
		    goto atn;
		}
            } while( nrfdislo() );

            // write data & eoi 
            ser_status = bus_receivebyte(&bus, &c, 1);
            if(ser_status & 0x40)
            {
                eoilo();
                er|=E_EOI;
            }
            wrd(c);
            davlo();

            // wait nrfd lo 
            do {
                if( atnislo() ) {
		    goto atn;
		}

                if( ndacishi() && nrfdishi() ) {
                    eoihi();
                    davhi();
		    er |= E_NODEV;
		    goto idle;
                }
            } while( nrfdishi() );

            // wait ndac hi 
            do {
                if ( atnislo() ) {
		    // ATN got low
                    goto atn;
                }
            } while( ndacislo() );

            davhi();
            eoihi();
            ser_status = bus_receivebyte(&bus, &c, 0);

            // wait ndac lo 
            if( ser_status & 0xff ) {
                break;
            } else {
                do {
                    if ( atnislo() ) {
                        goto atn;
                    }
                } while( ndacishi() );
            }
        }
	// no ATN, so set bus to idle
	setidle();
        return; //(er&(E_EOI));

atn:
	// sets IEEE488 back to receive mode
        setrx();
        return; //(er&(E_EOI));

idle:
	// after EOF we set bus to idle
	setidle();
*/
        return; //(er&(E_EOI));
}

/***************************************************************************
 * main IEEE "loop"
 * The actual loop is outside this method to handle other functions outside
 * the IEEE stuff. The outside loop calls this one on each iteration.
 */

void iec_mainloop_iteration(void)
{
        int16_t cmd = 0;

#if 0
	// debug
	timer_set_us(256);
	led_toggle();
	while(!timer_is_timed_out());
	led_toggle();
	// end debug
#endif

        ser_status=0;

	// only do something on ATN low
	if (satnishi()) {
		return;
	}

	clkhi();

	datalo();

	// acknowledge ATN
	satnalo();

	// on the C64, CLK is set high directly after setting ATN
	// but here we are possibly faster than that, so do a delay
	delayus(20);

        // Loop to get commands during ATN lo ----------------------------
#if 0 
	// this is also on top of liecin()
	do {
		if (satnishi()) {
			goto cmd;
		}
	} while (clkislo());
#endif

	do {
		// get byte (under ATN) - call to E9C9
		cmd = iecin(1);
		if (cmd < 0) {
debug_printf("cmd=%d", cmd);
			break;
		}
		ser_status = bus_attention(&bus, 0xff & cmd);

	} while (satnislo());
	
        // ---------------------------------------------------------------
	// ATN is high now
	// parallelattention has set status what to do
	// now transfer the data
cmd:
#ifdef DEBUG_BUS
	debug_printf("stat=%04x", ser_status); debug_putcrlf();
#endif
	
	// E8D7
	satnahi();

	if(isListening())
        {
        } else
        {
        }

	iechw_setup();
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
	bus_init_bus(&bus);
}


