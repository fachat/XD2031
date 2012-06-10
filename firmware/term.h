//-------------------------------------------------------------------------
// Titel: XD-2031 - Debug Communication
// Funktion: handle terminal Communication
//-------------------------------------------------------------------------
// Copyright (C) 2012  Andre Fachat <afachat@gmx.de>
//-------------------------------------------------------------------------
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation;
// version 2 of the License ONLY.
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

#ifndef TERM_H
#define TERM_H

void term_init(void);
void term_flush(void);
void term_putc(char c);
void term_putcrlf(void);
void term_puts(const char *);
void term_printf(const char *, ...);


#endif
