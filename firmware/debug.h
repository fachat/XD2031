/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat, Nils Eilers

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

#ifndef DEBUG
#define       DEBUG 1
#endif


// do this always(?)



#if DEBUG 

#include "term.h"
#include "archcompat.h"


#define	debug_flush()	term_flush()

extern const char nullstring[7];

void debug_putc(char c);
void debug_putcrlf();
 
#define	debug_puts(s)	term_rom_puts(IN_ROM_STR(s))

#define	debug_printf(format, ...)	term_rom_printf(IN_ROM_STR(format), __VA_ARGS__)

void debug_puthex(char c);
void debug_hexdump(uint8_t * p, uint16_t len, uint8_t petscii);
void debug_dump_packet(packet_t * p);
 

#else				// no DEBUG

#include "packet.h"

// those should be optimized away
static inline void debug_puts(char *c) { } 
static inline void debug_putps(char *c) { } 
static inline void debug_putc(char c) { } 
static inline void debug_putcrlf() { } 
static inline void debug_putputs(char *s) { } 
static inline void debug_putputps(char *s) { } 
static inline void debug_puthex(char c) { } 
static inline void debug_printf(char *format, ...) { } 
static inline void debug_hexdump(uint8_t * p, uint16_t len, uint8_t petscii) { } 
static inline void debug_flush() { } 
static inline void debug_dump_packet(packet_t * p) { }

#endif				// DEBUG
#endif				// def DEBUG_H

