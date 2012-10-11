
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

#include "ieeehw.h"
#include "bus.h"

#include "debug.h"
#include "led.h"

#undef DEBUG_BUS

// Prototypes

static void talkloop(void);
static void listenloop(void);

#define isListening()   ((par_status&0xe000)==0x2000)
#define isTalking()     ((par_status&0xe000)==0x4000)

// bus state
static bus_t bus;

// TODO: make that ... different...
static int16_t par_status = 0;


/***************************************************************************
 * Listen handling
 */

// read a byte from IEEE
static int16_t liecin(int *c)
{
        int er = 0;

        nrfdhi();

	// do...while to make sure to at least test ATN once
        do {
            if(atnislo()) {
		// ATN got low, exit
                goto atn;
            }
        } while( davishi() );

        nrfdlo();

        if(!eoiishi())
                er|=E_EOI;
        *c=rdd();

        ndachi();

        do {
            if(atnislo()) {
		// ATN got low, exit
                //break;
                goto atn;
            }
        } while( davislo() );

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
#ifdef DEBUG_BUS
	debug_putc('L');
#endif
        int er, c;
        while(((er=liecin(&c))&E_ATN)!=E_ATN) {
            par_status = bus_sendbyte(&bus, c, er & E_EOI);
        }
	// if did not stop due to ATN, set to idle,
	// otherwise stay in rx mode
	if (er != E_ATN) {
	    setidle();
	}
        return;
}

/***************************************************************************/

static void talkloop()
{
        int16_t er /*,sec*/;
        uint8_t c;

#ifdef DEBUG_BUS
	debug_putc('T'); debug_flush();
#endif
        settx();            /* enables sending */

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
            par_status = bus_receivebyte(&bus, &c, 1);
            if(par_status & 0x40)
            {
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
        setrx();
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
		return;
	}

	// set receive mode
	setrx();

        par_status=0;

        /* Loop to get commands during ATN lo ----------------------------*/

        while(1)
        {
            ndaclo();
	    // acknowledge ATN
            atnalo();
            nrfdhi();

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
	    // ack with ndac hi
            ndachi();

            par_status = bus_attention(&bus, cmd);

	    // wait until DAV goes up
//	    do {
//		if (atnishi()) {
//		    // ATN hi, end loop
//		    goto cmd;
//		}
//	    }
            while(davislo());
        }

        /* ---------------------------------------------------------------*/
	// ATN is high now
	// parallelattention has set status what to do
	// now transfer the data
cmd:
#ifdef DEBUG_BUS
	debug_printf("stat=%04x", par_status); debug_putcrlf();
#endif

	if(isListening())
        {
		// make sure nrfd stays lo...
		nrfdlo();
		// ... when we un-acknowlege the ATN
		// (which is already hi anyway)
                atnahi();
                listenloop();
        } else
        {
                atnahi();
                nrfdhi();

                if(isTalking()) {
                    talkloop();
                }
        }

	ieeehw_setup();
#ifdef DEBUG_BUS
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
	bus_init_bus(&bus);
}

#endif // HAS_IEEE
