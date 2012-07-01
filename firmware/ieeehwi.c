
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
#include "ieee3.h"

// Prototypes

int talkloop(void);
int listenloop(void);

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

        while(davishi()) {
            if(atnislo()) {
		// ATN got low, exit
                goto atn;
            }
        }

        nrfdlo();

        if(!eoiishi())
                er|=E_EOI;
        *c=rdd();

        ndachi();

        while( davislo() ) {
            if(atnislo()) {
		// ATN got low, exit
                //break;
                goto atn;
            }
        }

        ndaclo();

        return(er);

atn:
        ieeehw_setup();
        return E_ATN;
}

int listenloop() {
        int er, c;
        while(((er=liecin(&c))&E_ATN)!=E_ATN) {
            par_status = parallelsendbyte(c, er & E_EOI);
        }
        return 0;
}

/***************************************************************************/

int talkloop()
{
        int16_t er /*,sec*/;
        uint8_t c;

        er=0;
        /*sec=secadr&0x0f;*/

        while (1 /*  (!er)
                && (!(f[sec].state&S_EOI2))
                && ((sec==0x0f)||f[sec].state) */
        ) {
            settx();            /* enables sending */

            /* wait nrfd hi */
            do {
                if(atnislo()) goto atn;
            } while( nrfdislo() );

            /* write data & eoi */
            par_status = parallelreceivebyte(&c, 1);
            if(par_status & 0x40 /*(f[sec].bf[0]==EOF)||(f[sec].bf[1]==EOF)*/)
            {
                eoilo();
                er|=E_EOI;
            }
            wrd(c /*f[sec].bf[0]*/);
            davlo();

            /* wait nrfd lo */
            do {
                if( atnislo() ) goto atn;

                if( ndacishi() && nrfdishi() ) {
                    eoihi();
                    davhi();
                    return E_NODEV;
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
            par_status = parallelreceivebyte(&c, 0);

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
atn:
        ieeehw_setup();

        return(er&(E_EOI|E_BRK));
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

        ieeehw_setup();

        par_status=0;

        /* Loop to get commands during ATN lo ----------------------------*/

        while(1)
        {
            ndaclo();
            setrx();
            atnalo();
            nrfdhi();

            /* wait for DAV lo */
            while(davishi()) {
                if(atnishi()) {
		    // ATN hi, end loop
                    goto cmd;
                }
            }
	
            nrfdlo();

	    // read data
            cmd=rdd();
	    // ack with ndac hi
            ndachi();

            par_status = parallelattention(cmd);

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
                nrfdlo();
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
void ieeehwi_init(void) {
}


