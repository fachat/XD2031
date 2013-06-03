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

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>	// printf
#include <errno.h>	// errno
#include <stdlib.h>	// exit()

#include "privs.h"
#include "log.h"

#if defined(UNPRIVILEGED) || defined(_WIN32)
void drop_privileges() { 
	log_info("Compiled with unprivileged rights\n");
}
#else

static void drop_to_uid(uid_t ruid);

/**
 * drops privilege to the real used id
 * to be used after opening e.g. the serial device (which may be root-only)
 */
void drop_privileges() {

	uid_t ruid = getuid();
	uid_t euid = geteuid();

	log_info("Real user ID=%d\n", ruid);
	log_info("Effective user ID=%d\n", euid);

	if (ruid != 0) {
		// not root
		drop_to_uid(ruid);
	}
}

static void drop_to_uid(uid_t ruid) {

	// in Linux, this releases all privileges, i.e.
	// makes real uid = saved uid = effective uid
	int e = setuid(ruid);

	if (e < 0) {
		fprintf(stderr, "Problem dropping privileges %d!\n", errno);
		exit(-1);
	}

	uid_t new_ruid = getuid();

	if (ruid != new_ruid) {
		fprintf(stderr, "Problem dropping privileges - new ruid not set: %d!\n", new_ruid);
		exit(-1);
	}

	log_info("New effective user ID=%d\n", new_ruid);

	e = setuid(0);

	if (e == 0) {
		fprintf(stderr, "Detected problem: was able to regain privileges to uid=%d, euid=%d!\n", 
			getuid(), geteuid());
		exit(-1);
	}

}

#endif /* __APPLE__ */
