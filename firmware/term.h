/*
    Titel: XD-2031 - Debug Communication
    Funktion: handle terminal Communication
    Copyright (C) 2012  Andre Fachat <afachat@gmx.de>

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
*/

#ifndef TERM_H
#define TERM_H

#include "provider.h"

void term_init();
void term_set_endpoint(endpoint_t *ep);
void term_flush(void);
void term_putc(char c);
void term_putcrlf(void);
void term_puts(const char *);
void term_rom_puts(const char *);
void term_printf(const char *, ...);
void term_rom_printf(const char *, ...);


#endif
