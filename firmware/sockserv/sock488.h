/*
    Titel:	 Socket server 
    Copyright (C) 2014  Andre Fachat <afachat@gmx.de>

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

#ifndef SOCK4488_H
#define SOCK4488_H

#define	S488_ATN	0x01	/* M->D send a byte with ATN */
#define	S488_SEND	0x02	/* M->D send a byte to device */
#define	S488_REQ	0x03	/* M->D request a byte from device */
#define	S488_OFFER	0x04	/* D->M offer a byte for a receive */

#define	S488_TIMEOUT	0x20	/* Read timeout */
#define	S488_ACK	0x40	/* ACKnowledge a byte to receiver as part of a REQ */
#define	S488_EOF	0x80	/* when set on SEND or OFFER, transfer with EOF */

void sock488_init(void);

void sock488_set_socket(const char *socket_name);

void sock488_mainloop_iteration();

#endif
