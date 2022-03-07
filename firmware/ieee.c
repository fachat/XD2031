
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

#ifdef HAS_IEEE

/**
 * Hardware-independent IEEE layer 
 */

#include <ctype.h>

#include "archcompat.h"
#include "ieeehw.h"
#include "bus.h"

#include "debug.h"
#include "term.h"
#include "led.h"

#undef DEBUG_IEEE
#undef DEBUG_IEEE_DATA

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
static int16_t par_status = 0;


/***************************************************************************
 * Listen handling
 */

// read a byte from IEEE
static int16_t liecin(int *c)
{
        int er = 0;

	static int cnt = 0;

	cnt ++;

        //if (nrfdishi()) {
	//	// does not trigger
	//	debug_printf("%d: NRFD already hi! atna=%u, atn=%u, nrfd=%u\n", 
	//		cnt, is_atna, atnishi(), nrfdishi());
	//}

        ndaclo();

        //if (ndacishi()) {
	//	// does not trigger
	//	debug_printf("%d: NDAC already hi! atna=%u, atn=%u, ndac=%u\n", 
	//		cnt, is_atna, atnishi(), ndacishi());
	//}

	// this delay here seems to fix everything (either print or delay)
	// even  without double-sided ATN interrupt (i.e. setting NRFD/NDAC
	// low only on ATN going low)

	//debug_printf("atnishi=%u, nrfdishi==%u, ndacishi==%u, dav=%u", atnishi(), nrfdishi(), ndacishi(), davishi()); debug_putcrlf();

	//delayus(1000);


	// PET4 @ F11E checks NRFD and waits for hi
        nrfdhi_raw();

	// with or without, no effect
        //if (nrfdislo()) {
		// This happens at end of listen
		// as ATN is assigned and the interrupt kicks in
		// atna is low and atn is hi, so nrfd is not set hi
		//
		// this is actually sometimes missing, 
		// obviously without effect (no missing bytes)
		//
		//debug_printf("%d: NRFD lo, atna=%u, atn=%u, nrfd=%u\n", 
		//	cnt, is_atna, atnishi(), nrfdishi());
		//return E_ATN;
	//}

	// somehow here the interrupt (lo->hi) gets triggered
	// to do NRFD lo and therefore would lock because DAV would
	// never go lo.
	// confirmed, as double-sided ATN triggers this (i.e. enabling
	// nrfd->lo on atn going hi), ATN int on going low only is ok,
	// but looses chars except with the delay above

	// do...while to make sure to at least test ATN once
        do {
	    // this IRQ protection is only there to fix the 
	    // unexpected ATN lo->hi
	    //cli();
            if(atnislo()) {
		//sei();
		// ATN got low, exit
                goto atn;
            }
	    //if (nrfdislo()) {
	    //	nrfdhi();
	//	debug_printf("%d: Bummer: NRFD unexpectedly lo, while ATN hi: atn=%u, atna=%u\n", cnt, atnishi(), is_atna);
	    //}
	    //nrfdhi();
	    //sei();
        } while( davishi() );

        nrfdlo();

        if(!eoiishi())
                er|=E_EOI;
        *c=rdd();

	// PET4 @ F12D checks NDAC and waits for hi
	// disable interrupts
        ndachi_raw();

       	// wait DAV hi 

        while( davislo() );

	ndaclo();

        return(er);

atn:
	// we do not need to do anything,
	// as we already are in listen mode, and the
	// ACK hardware or software has just pulled 
	// NRFD and NDAC
        return E_ATN;
}

static void listenloop() {
#ifdef DEBUG_IEEE
	debug_putc('L');
#endif
        int er, c = 0; // silence warning maybe uninitialized
        while(((er=liecin(&c))&E_ATN)!=E_ATN) {
            par_status = bus_sendbyte(&bus, c, (er & E_EOI) ? BUS_FLUSH : 0);
        }
	// if did not stop due to ATN, set to idle,
	// otherwise stay in rx mode
	if (er != E_ATN) {
	    setidle_listen();
	}
        return;
}

/***************************************************************************/

static void talkloop()
{
        int16_t er /*,sec*/;
        uint8_t c;

#ifdef DEBUG_IEEE
	debug_putc('T'); debug_flush();
#endif
        settx();            /* enables sending */
        /* We're faster than the PET, so we have to wait for NDAC low first
         * to avoid running in DEVICE NOT PRESENT error */
        while(ndacishi() && atnishi());     // Wait for NDAC low

        er=0;
        /*sec=secadr&0x0f;*/

        while (!er) {

            /* wait nrfd hi */
            do {
                if( atnislo() ) {
		    goto atn;
		}
            } while( nrfdislo() );

            /* write data & eoi */
            par_status = bus_receivebyte(&bus, &c, BUS_PRELOAD);

#ifdef DEBUG_IEEE_DATA
		debug_printf(" %02x, (%04x)", c, par_status);
#endif

	    if (isReadTimeout(par_status)) {
		// we should create a read timeout, by not setting DAV low
		// in time. This happens on r/w channels, when no data is
		// available
#ifdef DEBUG_IEEE
		debug_putc('R');
#endif
		break;
	    }

            if(par_status & STAT_EOF)
            {
#ifdef DEBUG_IEEE
		debug_putc('E');
#endif
                eoilo();
                er|=E_EOI;
            }
            wrd(c);
            davlo();

            /* wait nrfd lo */
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

            /* wait ndac hi */
            do {
                if ( atnislo() ) {
		    // ATN got low
                    goto atn;
                }
            } while( ndacislo() );

            davhi();
            eoihi();
            par_status = bus_receivebyte(&bus, &c, 0);

            /* wait ndac lo */
            if( par_status & 0xff ) {
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
	//debug_puts("setrx()");debug_flush();
        setrx();
	// calling the next two breaks DIR on XS-1541
        //nrfdhi_raw();
        //ndachi_raw();
        return; //(er&(E_EOI));

idle:
	// after EOF we set bus to idle
	setidle();
        return; //(er&(E_EOI));
}

/***************************************************************************
 * main IEEE "loop"
 * The actual loop is outside this method to handle other functions outside
 * the IEEE stuff. The outside loop calls this one on each iteration.
 */

void ieee_mainloop_iteration(void)
{
        int cmd = 0;

	// only do something on ATN low
	if (atnishi()) {
		if (bus.active == 0) {
			// if ATN is hi, devices have been switched on
                	term_rom_puts(IN_ROM_STR("Enabling IEEE bus as ATN is inactive now\n"));
			bus.active = 1;
		}
		return;
	}

        // if bus not active (ATN was low on reset), do nothing
        if (bus.active == 0) {
                return;
        }

	// set receive mode
	//debug_puts("setrx()");debug_flush();
	setrx();

        par_status=0;

	// acknowledge ATN
        atnalo();

        /* Loop to get commands during ATN lo ----------------------------*/

        while(1)
        {
	    // PET @ F11E waits for NRFD hi
            nrfdhi_raw();

            /* wait for DAV lo */
            do {
                if(atnishi()) {
		    // ATN hi, end loop
                    goto cmd;
                }
            } while(davishi());

            nrfdlo();

	    // read data
            cmd=rdd();

	    // PET @ F12D waits for NDAC hi
	    // ack with ndac hi
	    // disable interrupts
            ndachi_raw();

	    // wait until DAV goes up
            while(davislo());

	    ndaclo();

            par_status = bus_attention(&bus, cmd);
        }

        /* ---------------------------------------------------------------*/
	// ATN is high now
	// parallelattention has set status what to do
	// now transfer the data
cmd:
#ifdef DEBUG_IEEE
	debug_printf("stat=%04x", par_status); debug_putcrlf();
#endif

	if(isListening(par_status))
        {
		// adding this seems to cure the hang from double-sided ATN int
		//debug_printf("nrfdishi==%u, ndacishi==%u, dav=%u", nrfdishi(), ndacishi(), davishi()); debug_putcrlf();

		// make sure nrfd stays lo...
		nrfdlo();
		ndaclo();
		// ... when we un-acknowlege the ATN
		// (which is already hi anyway)
                atnahi();

		if (atnislo()) {
			//debug_printf("Bummer: listen - ATN is lo (nrfd=%u, atna=%u)\n", nrfdishi(), is_atna);
		}

                listenloop();
        } else
        {
                nrfdhi();
		ndachi();
                atnahi();

                if(isTalking(par_status) && !isReadTimeout(par_status)) {
                    talkloop();
                }
        }

	ieeehw_setup();
#ifdef DEBUG_IEEE
	debug_putc('X'); debug_flush();
#endif	
        return;
}

/***************************************************************************
 * Init code
 */
void ieee_init(uint8_t deviceno) {
        ieeehw_setup();

	// register bus instance
	bus_init_bus("ieee", &bus);

	// ignore bus when ATN is constantly pulled low, e.g. by switched off devices
	if (atnishi()) {
		bus.active = 1;
        } else {
                term_rom_puts(IN_ROM_STR("Ignoring IEEE bus as ATN is constantly low\n"));
	}
#ifdef DEBUG_IEEE
	debug_printf("active=%d\n", bus.active);
#endif
}

#endif // HAS_IEEE
