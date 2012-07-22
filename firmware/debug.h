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
 * This file implements the debug print layer
 */

#ifndef DEBUG_H
#define DEBUG_H

#define	DEBUG 1
//#define	DEBUG 0

#if DEBUG 

#include "term.h"
#include "mem.h"

static inline void debug_putc(char c) { term_putc(c); }
static inline void debug_putcrlf() {  term_putcrlf(); term_flush(); }
#define	debug_puts(s)	term_puts(IN_ROM_STR(s))
//#define	debug_putps(s)	term_puts(PROGMEM s)	/*uartPutString(PSTR(s))*/

#define	debug_printf(format, ...)	term_printf(IN_ROM_STR(format), __VA_ARGS__)

static char hexnibs[] = {'0','1','2','3','4','5','6','7',
                         '8','9','a','b','c','d','e','f' };

static inline void debug_putnib(char c) {
	debug_putc(hexnibs[c&15]);
}

static inline void debug_puthex(char c) {
	debug_putnib(c>>4);
	debug_putnib(c);
}


#else

// those should be optimized away
static inline void debug_puts(char *c) {}
static inline void debug_putps(char *c) {}
static inline void debug_putc(char c) {}
static inline void debug_putcrlf() {}
static inline void debug_putputs(char *s) {}
static inline void debug_putputps(char *s) {}
static inline void debug_puthex(char c) {}
static inline void debug_printf(char *format, ...) {}

#endif

#endif
