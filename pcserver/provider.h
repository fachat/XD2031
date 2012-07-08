
/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

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

#ifndef PROVIDER_H
#define PROVIDER_H

//
// Endpoint providers communicate with the outside world with their 
// own protocol. Examples are local filesystem, or TCP/IP, or HTTP
// providers.
//
// Each provider can have multiple active endpoints. For example 
// different directories, or different hosts in case of internet
// access. Thus there is a provider definition, and a provider 
// state struct definition
//

typedef struct {
	struct provider_t	*ptype;
} endpoint_t;

typedef struct {
	const char	*name;			// provider name, used in ASSIGN as ID
	void		(*init)(void);			// initialization routine
	endpoint_t* 	(*newep)(const char *par);	// create a new endpoint instance
	void 		(*freeep)(endpoint_t *ep);	// free an endpoint instance

	// file-related	
	void		(*close)(endpoint_t *ep, int chan);	// close a channel
	int		(*open)(endpoint_t *ep, int chan, const char *name, const char *mode);	// open a file
	int		(*opendir)(endpoint_t *ep, int chan, const char *name);	// open a directory for reading
	int		(*readfile)(endpoint_t *ep, int chan, char *retbuf, int len, int *eof);	// read file data
	int		(*writefile)(endpoint_t *ep, int chan, char *buf, int len, int is_eof);	// write a file

	// command channel
	int		(*scratch)(endpoint_t *ep, char *name, int *outdeleted);	// delete

	
} provider_t;

#endif

