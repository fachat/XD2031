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


#ifndef _WIN32

/*
 * POSIX RS232 interface handling
 */

#include "os.h"

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/dir.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fnmatch.h>
#include <string.h>

#include "serial.h"
#include "log.h"
#include "mem.h"

static char *serial_device_path = NULL;

/* open device
   Remember the path to the serial device, so that we can check later,
   if it's still there.
*/
int device_open(char *device) {
	int fdesc = open(device, O_RDWR | O_NOCTTY); // | O_NDELAY);
	mem_free(serial_device_path);
	serial_device_path = mem_alloc_str2(device, "devicename");
	return fdesc;
}

/* On Linux, read() does not return -1 (error), if the device is lost.
   It simply returns 0 with 0 bytes read, so we check here, if
   the device is still present. */
int device_still_present(void) {
	struct stat d;

	return (!stat(serial_device_path, &d));
}

/**
 * See http://en.wikibooks.org/wiki/Serial_Programming:Unix/termios
 */
int config_ser(int fd) {

	struct termios  config;

	if(!isatty(fd)) { 
		log_error("device is not a TTY!\n");
		return -1;
	 }

	if(tcgetattr(fd, &config) < 0) { 
		log_error("Could not get TTY attributes!\n");
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

        config.c_cc[VMIN]  = 0;
        config.c_cc[VTIME] = 1;

        // Communication speed (simple version, using the predefined
        // constants)

//        if(cfsetispeed(&config, B115200) < 0 || cfsetospeed(&config, B115200) < 0) {
        if(cfsetispeed(&config, B38400) < 0 || cfsetospeed(&config, B38400) < 0) {
//        if(cfsetispeed(&config, B4800) < 0 || cfsetospeed(&config, B4800) < 0) {
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

int guess_device(char** device) {
/* search /dev for a (virtual) serial port 
	Return pointer to device name, if exactly one found.
	If none found or more than one, exit() with error msg.
	FT232 (XS-1541, petSD) connects as ttyUSB0 (ttyUSB1 ...) on Linux,
	and as cu.usbserial-<SERIAL NUMBER> on OS X.
	ttyAMA0 is Raspberry Pi's serial port (3.1541) */

	DIR *dirptr;
	struct direct *entry;

	*device = NULL;
	dirptr = opendir("/dev/");
	while((entry=readdir(dirptr)) != NULL) {
		if ((!fnmatch("cu.usbserial-*", entry->d_name, FNM_NOESCAPE)) ||
		    (!fnmatch("ttyUSB*", entry->d_name, FNM_NOESCAPE)) ||
		    (!strcmp("ttyAMA0", entry->d_name))) {
			log_info("Serial device /dev/%s auto-detected\n", entry->d_name);
			if (*device) {
				log_error("Unable to decide which serial device it is. "
				                "Please pick one.\n");
				exit(EXIT_RESPAWN_NEVER);
			}
			*device = malloc(strlen(entry->d_name) + 6);
			strcpy(*device, "/dev/");
			strcat(*device, entry->d_name);
		}
	}
	closedir(dirptr); // free ressources claimed by opendir
	if(*device == NULL) {
		log_error("Could not auto-detect device: none found\n");
		return(EXIT_FAILURE);
	}
	return EXIT_SUCCESS;
}



#else

/*
 * WIN32 RS232 interface handling
 */

#include "os.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "serial.h"
#include "log.h"

serial_port_t device_open(char *device) {
	return (CreateFile(device, GENERIC_READ | GENERIC_WRITE,
			0,
			0,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			0));
}

int config_ser(serial_port_t h) {
	// DCB dcb = {0};
	// COMMTIMEOUTS timeouts = {0};
	DCB dcb;
	COMMTIMEOUTS timeouts;

	memset(&dcb, 0, sizeof dcb);
	memset(&timeouts, 0, sizeof timeouts);

	dcb.DCBlength = sizeof(DCB);

	if (!GetCommState(h, &dcb)) {
		log_error("Error getting serial state\n");
		return -1;
	}

	//dcb.BaudRate = CBR_115200;
	//dcb.BaudRate = CBR_76800;
	dcb.BaudRate = CBR_38400;
	dcb.ByteSize = 8;
	dcb.StopBits = ONESTOPBIT;
	dcb.Parity   = NOPARITY;
	dcb.fBinary  = TRUE;
	dcb.fParity  = FALSE;
	dcb.fOutxCtsFlow = FALSE; 	// no hardware handshake
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fDsrSensitivity = FALSE; 	// Ignore DSR
	dcb.fTXContinueOnXoff = TRUE;
	dcb.fOutX = FALSE; 		// no XON/XOFF
	dcb.fInX = FALSE;		// no XON/XOFF
	dcb.fNull = FALSE;		// receive null bytes too
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.fAbortOnError = FALSE;


	if (!SetCommState(h, &dcb)) {
		log_error("Error setting serial port state\n");
		return -1;
	}

	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;

	timeouts.WriteTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutMultiplier = 0;

	if (!SetCommTimeouts(h, &timeouts)) {
		log_error("Error setting serial timeouts\n");
		return -1;
	}

	if (!SetCommMask( h, EV_TXEMPTY | EV_RXCHAR )) {
		log_error("Error setting CommMask\n");
		return -1;
	}

	return 0;
}

int guess_device(char** device) {
	// just a very vague guess
	static char devicename[] = "COM5";
	log_info("Using default serial device %s\n", devicename);
	*device = devicename;
	
	return EXIT_SUCCESS;
}

/* just a dummy since Windows read() returns an error, if the device is lost */
int device_still_present(void) {
	return 1;
}
#endif


