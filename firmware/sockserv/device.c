/**************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat <afachat@gmx.de>
    Copyright (C) 2013 Nils Eilers <nils.eilers@gmx.de>

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

**************************************************************************/

/**
 * Device dependend init
 */

#include "debug.h"
#include "uarthw.h"
#include "sock488.h"

void device_init(void) {

	sock488_init();
}

/**
 * device setup, called before(!) device_init()
 * to parse and set the command line parameters
 */
void device_setup(int argc, const char *argv[]) {


	const char *socketname = NULL;
	int p = 1;

	while (argc > p && argv[p][0]=='-') {
		switch(argv[p][1]) {
		case 'S':
			// server socket
			if (p < argc - 1) {
			 	p++;
				socketname = argv[p];

				uarthw_set_socket(socketname);
			} else {
				printf("setup: parameter -S requires socket name\n");
			}
			break;
		case 'C':
			// client socket
			if (p < argc - 1) {
			 	p++;
				socketname = argv[p];

				sock488_set_socket(socketname);
			} else {
				printf("setup: parameter -C requires socket name\n");
			}
			break;
		}
		p++;
	}
}


void device_loop(void) {

	sock488_mainloop_iteration();
}

