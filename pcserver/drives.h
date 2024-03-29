/****************************************************************************

    Commodore filesystem server
    Copyright (C) 2012,2018,2022 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

#ifndef DRIVES_H
#define DRIVES_H

// -----------------------------------------------------------------

typedef struct {
        int             drive;
        endpoint_t      *ep;
        const char      *cdpath;
} drive_t;

void drives_init();

void drives_free();

drive_t *drive_find(int drive);

int drive_unassign(int drive);

void drive_assign(int drive, endpoint_t *newep);

void drives_dump(const char *prefix, const char *eppref);

#endif

