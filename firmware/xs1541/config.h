//-------------------------------------------------------------------------
// Titel:	 XS-1541 - Configuration
// Funktion: common configurations
//-------------------------------------------------------------------------
// Copyright (C) 2012  Andre Fachat <afachat@gmx.de>
// Copyright (C) 2008  Thomas Winkler <t.winkler@tirol.com>
//-------------------------------------------------------------------------
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
//-------------------------------------------------------------------------

#ifndef CONFIG_H
#define CONFIG_H

#define	VERSION_STR		"0.02.01"

#define F_CPU                 14745600UL

// LED configuration
#define LED_DDR               DDRC
#define LED_PORT              PORTC
#define LED_BIT               PC0

// buffer sizes
#define CONFIG_COMMAND_BUFFER_SIZE      120
#define CONFIG_ERROR_BUFFER_SIZE        46




#if 0
/*
//#define IEC_TEST
#define IEC_SEPARATE_OUT				// For separate Input/Output lines
#define XA1541							// For XA1541 adapter on IEC
//#define OLIMEX						// For Olimex 40 pin board (LED)
//#define OLIMEX2  						// negative logic for LED
#define EMBEDIT 						// Embedit Prototype Board (LED)
*/

//#define IEC_TEST
//#define IEC_SEPARATE_OUT				// For separate Input/Output lines
//#define XA1541							// For XA1541 adapter on IEC
//#define OLIMEX						// For Olimex 40 pin board (LED)
#define OLIMEX2  						// negative logic for LED
//#define EMBEDIT 						// Embedit Prototype Board (LED)

#endif

#if defined __AVR_ATmega644__ || defined __AVR_ATmega644P__ ||  \
			__AVR_ATmega324__ || defined __AVR_ATmega324P__ 
	#define SRQ_INT_VECT					// 644, 324 only!
#endif



#endif
