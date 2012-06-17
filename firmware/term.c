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

/*
 * needs to be converted to the provider-pointer approach
 */

#include <inttypes.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "packet.h"
#include "term.h"
#include "uart2.h"
#include "wireformat.h"

#define	TERM_BUFFER_LENGTH	129

static char buf[TERM_BUFFER_LENGTH];
static uint8_t nchars;

static volatile packet_t termpack;

static void send() {

	// tell the packet what's in the buffer to be read
	// nchars are in the buffer to be sent; channel is -1 (for terminal)
	packet_set_filled(&termpack, 0xff, FS_TERM, nchars);
	uart_submit(&termpack);
	nchars = 0;
}

void term_flush() {
	uart_flush();
}

void term_putc(char c) {

	// wait if buffer is still being sent
	packet_wait_done(&termpack);

	if (nchars < TERM_BUFFER_LENGTH) {
		buf[nchars] = c;
		nchars++;
	}

	if (nchars >= TERM_BUFFER_LENGTH) {
		send();
	}
}

void term_putcrlf() {
	term_putc('\r');
	term_putc('\n');

	if (nchars > 0) {
		// has not been sent with the last char
		// flush
		send();
	}
}

void term_puts(const char *str) {


	packet_wait_done(&termpack);

	uint8_t len = strlen(str);

	if ((nchars + len) >= TERM_BUFFER_LENGTH) {
		send();
	}
	packet_wait_done(&termpack);
	if (len >= TERM_BUFFER_LENGTH) {
		// cut
		len = TERM_BUFFER_LENGTH;
	}
	memcpy(buf+nchars, str, len);

	nchars += len;
}

void term_printf(const char *format, ...)
{
    va_list args;
    char    pbuf[TERM_BUFFER_LENGTH];

    va_start( args, format );
    vsprintf(pbuf, format, args );
    term_puts(pbuf);
}

void term_init(void) {
	nchars = 0;

	packet_init(&termpack, TERM_BUFFER_LENGTH, (uint8_t*)buf);
}

