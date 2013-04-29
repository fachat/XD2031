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

/*
 * rs232 interface handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/dir.h>
#include <fnmatch.h>
#include <string.h>

#include "serial.h"
#include "log.h"

/* open device */
int device_open(char *device) {
	int fdesc = open(device, O_RDWR | O_NOCTTY); // | O_NDELAY);
	return fdesc;
}

/**
 * See http://en.wikibooks.org/wiki/Serial_Programming:Unix/termios
 */
int config_ser(int fd) {

	struct termios  config;

	if(!isatty(fd)) { 
		log_error("device is not a TTY!");
		return -1;
	 }

	if(tcgetattr(fd, &config) < 0) { 
		log_error("Could not get TTY attributes!");
		return -1;
	}

	// Input flags - Turn off input processing
	// convert break to null byte, no CR to NL translation,
	// no NL to CR translation, don't mark parity errors or breaks
	// no input parity check, don't strip high bit off,
	// no XON/XOFF software flow control

	config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
                    INLCR | PARMRK | INPCK | ISTRIP | IXON);

        // Output flags - Turn off output processing
        // no CR to NL translation, no NL to CR-NL translation,
        // no NL to CR translation, no column 0 CR suppression,
        // no Ctrl-D suppression, no fill characters, no case mapping,
        // no local output processing

        //config.c_oflag &= ~(OCRNL | ONLCR | ONLRET |
        //            ONOCR | ONOEOT| OFILL | OLCUC | OPOST);
        config.c_oflag = 0;

        // No line processing:
        // echo off, echo newline off, canonical mode off, 
        // extended input processing off, signal chars off

        config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

        // Turn off character processing
        // clear current char size mask, no parity checking,
        // no output processing, force 8 bit input

        config.c_cflag &= ~(CSIZE | PARENB);
        config.c_cflag |= CS8;

        // One input byte is enough to return from read()
        // Inter-character timer off

        config.c_cc[VMIN]  = 1;
        config.c_cc[VTIME] = 0;

        // Communication speed (simple version, using the predefined
        // constants)

        if(cfsetispeed(&config, B115200) < 0 || cfsetospeed(&config, B115200) < 0) {
		log_error("Could not set required line speed!");
		return -1;
        }

        // Finally, apply the configuration

        if(tcsetattr(fd, TCSAFLUSH, &config) < 0) { 
		log_error("Could not apply configuration!");
		return -1;
	}

	return 0;
}

void guess_device(char** device) {
/* search /dev for a (virtual) serial port 
   Change "device" to it, if exactly one found
   If none found or more than one, exit(1) with error msg.
   FT232 (XS-1541, petSD) connects as ttyUSB0 (ttyUSB1 ...) on Linux,
   and as cu.usbserial-<SERIAL NUMBER> on OS X.
   ttyAMA0 is Raspberry Pi's serial port (3.1541) */

  DIR *dirptr;
  struct direct *entry;
  static char devicename[80] = "/dev/";
  int candidates = 0;

  dirptr = opendir(devicename);
  while((entry=readdir(dirptr))!=NULL) {
    if ((!fnmatch("cu.usbserial-*",entry->d_name,FNM_NOESCAPE)) ||
        (!fnmatch("ttyUSB*",entry->d_name,FNM_NOESCAPE)) ||
        (!strcmp("ttyAMA0",entry->d_name)))
    {
      strncpy(devicename + 5, entry->d_name, 80-5); 
      devicename[80] = 0; // paranoid... wish I had strncpy_s...
      candidates++;
    }
  }
  if(candidates == 1) *device = devicename;

  // return(candidates); someday the error handling could be outside this fn

  switch(candidates) {
    case 0:
      fprintf(stderr, "Could not auto-detect device: none found\n");
      exit(1);
    case 1:
      log_info("Serial device %s auto-detected\n", *device);
      break;
    default:
      fprintf(stderr, "Unable to decide which serial device it is. "
                      "Please pick one.\n");
      exit(1);
      break;
  }
}


