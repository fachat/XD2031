/**************************************************************************

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

***************************************************************************/

// Compatibility layer
// - firmware / server
// - MCU architecture (only AVR up to now)

#ifndef ARCHCOMPAT_H
#define ARCHCOMPAT_H

// ---------- Test firmware code on PC -------------------------------------
#if defined(PCTEST) || defined(SERVER)
#include <inttypes.h>
#include <string.h>
#include "stdio.h"

static inline void debug_flush(void)
{
	fflush(stdout);
}

static inline void term_rom_puts(char *s)
{
	printf("%s", s);
}

static inline void debug_putcrlf(void)
{
	printf("\n");
}

#define nullstring "<NULL>"

#define IN_ROM
static inline uint8_t rom_read_byte(const uint8_t * a)
{
	return *a;
}

static inline size_t rom_strlen(const char *s)
{
	return strlen(s);
}

static inline void *rom_memcpy(void *dest, const void *src, size_t n)
{
	return memcpy(dest, src, n);
}

static inline const void *rom_read_pointer(const void *p)
{
	return *((void**)p);
}

#define rom_sprintf(s, f, ...) sprintf(s, f, __VA_ARGS__)

#endif

// ---------- PC socket server ------------------------------
#ifdef PC

#include <string.h>
#include <inttypes.h>

#define	IN_ROM
#define	IN_ROM_STR(s)	(s)

static inline size_t rom_strlen(const char *s)
{
	return strlen(s);
}

static inline void *rom_memcpy(void *dest, const void *src, size_t n)
{
	return memcpy(dest, src, n);
}

static inline char *rom_strcat(char *dest, const char *src)
{
	return strcat(dest, src);
}

#define rom_sprintf(s, f, ...) sprintf(s, f, __VA_ARGS__)
#define	rom_vsprintf(s, f, ...)	vsprintf(s, f, __VA_ARGS__)
#define	rom_vsnprintf(s, n, f, ...) vsnprintf(s, n, f, __VA_ARGS__)
#define	rom_read_byte(addr)	(*(addr))
#define rom_read_pointer(addr)	(*(addr))

int rom_snprintf(char *buf, int buflen, const char *format, ...);

#endif				// PC

// ---------- Atmel AVR 8 bit microcontroller ------------------------------
#ifdef __AVR__
#include <avr/pgmspace.h>

#define	IN_ROM		PROGMEM
#define	IN_ROM_STR(s)	PSTR(s)

static inline size_t rom_strlen(const char *s)
{
	return strlen_P(s);
}

static inline void *rom_memcpy(void *dest, const void *src, size_t n)
{
	return memcpy_P(dest, src, n);
}

static inline char *rom_strcat(char *dest, const char *src)
{
	return strcat_P(dest, src);
}

#define rom_sprintf(s, f, ...) sprintf_P(s, f, __VA_ARGS__)
#define	rom_vsprintf(s, f, ...)	vsprintf_P(s, f, __VA_ARGS__)
#define	rom_vsnprintf(s, n, f, ...) vsnprintf_P(s, n, f, __VA_ARGS__)
#define	rom_read_byte(addr)	pgm_read_byte((addr))
#define rom_read_pointer(addr)	pgm_read_word((addr))

int rom_snprintf(char *buf, int buflen, const char *format, ...);

#endif				// AVR

#endif				// ARCHCOMPAT_H
