
/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

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

/**
 * Hardware-independent IEEE layer 
 */

#include <ctype.h>

#include "ieee.h"
#include "ieeehw.h"
#include "bus.h"

// Prototypes

void talkloop(void);
void listenloop(void);

#define isListening()   ((par_status&0xf000)==0x2000)
#define isTalking()     ((par_status&0xf000)==0x4000)

static int16_t par_status = 0;


/***************************************************************************
 * Listen handling
 */

// read a byte from IEEE
int16_t liecin(int *c)
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

void listenloop() {
        int er, c;
        while(((er=liecin(&c))&E_ATN)!=E_ATN) {
            par_status = bus_sendbyte(c, er & E_EOI);
        }
	// if did not stop due to ATN, set to idle,
	// otherwise stay in rx mode
	if (er != E_ATN) {
	    setidle();
	}
        return;
}

/***************************************************************************/

void talkloop()
{
        int16_t er /*,sec*/;
        uint8_t c;

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
            par_status = bus_receivebyte(&c, 1);
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
            par_status = bus_receivebyte(&c, 0);

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

            par_status = bus_attention(cmd);

	    // wait until DAV goes up
            while(davislo());
        }

        /* ---------------------------------------------------------------*/
	// ATN is high now
	// parallelattention has set status what to do
	// now transfer the data
cmd:

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
        return;
}

/***************************************************************************
 * Init code
 */
void ieeehwi_init(uint8_t deviceno) {
        ieeehw_setup();
}


