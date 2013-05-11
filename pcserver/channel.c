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

#include <stdio.h>

#include "log.h"
#include "provider.h"

/*
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "wireformat.h"
#include "fscmd.h"
#include "dir.h"
#include "xcmd.h"
*/


//------------------------------------------------------------------------------------
// Mapping from channel number for open files to endpoint providers
// These are set when the channel is opened

typedef struct {
       int             channo;
       endpoint_t      *ep;
} chan_t;

chan_t chantable[MAX_NUMBER_OF_ENDPOINTS];


void channel_init() {
       int i;
        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
          chantable[i].channo = -1;
        }
}

endpoint_t *channel_to_endpoint(int chan) {

       int i;
        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
               if (chantable[i].channo == chan) {
                       return chantable[i].ep;
               }
        }
       log_info("Did not find ep for channel %d\n", chan);
       return NULL;
}

void channel_free(int channo) {
       int i;
        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
               if (chantable[i].channo == channo) {
                       chantable[i].channo = -1;
                       chantable[i].ep = NULL;
               }
        }
}

void channel_set(int channo, endpoint_t *ep) {
       int i;
        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
               // we overwrite existing entries, to "heal" leftover cruft
               // just in case...
               if ((chantable[i].channo == -1) || (chantable[i].channo == channo)) {
                       chantable[i].channo = channo;
                       chantable[i].ep = ep;
                       return;
               }
        }
       log_error("Did not find free ep slot for channel %d\n", channo);
}


