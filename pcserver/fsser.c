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
 * This file is a server FSTCP filesystem implementation, to be
 * used with the FSTCP program on an OS/A65 computer. 
 *
 * usage: 
 *   fsser [options] exported_directory
 *
 *   options:
 * 	-ro	export read-only
 * 	-d <device>  determine device (if none, use stdin/stdout)
 *	-d auto      try to auto-detect device	
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/dir.h>
#include <fnmatch.h>
#include <string.h>
#include <pwd.h>

#include "fscmd.h"
#include "privs.h"
#include "log.h"
#include "provider.h"
#include "mem.h"

#define FALSE 0
#define TRUE 1

void usage(void) {
	printf("Usage: fsser [options] run_directory\n"
		" options=\n"
                //"   -ro               export read-only \n"    // does not currently work
                "   -A<drv>=<provider-string>\n"
                "               assign a provider to a drive\n"
                "               e.g. use '-A0=fs:.' to assign the current directory\n"
                "               to drive 0. Dirs are relative to the run_directory param\n"
		"   -d <device>	define serial device to use\n"
		"   -d auto     auto-detect serial device\n"
	);
	exit(1);
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
/* search /dev for a virtual serial port 
   Change "device" to it, if exactly one found
   If none found or more than one, exit(1) with error msg */

  DIR *dirptr;
  struct direct *entry;
  static char devicename[80] = "/dev/";
  int candidates = 0;

  dirptr = opendir(devicename);
  while((entry=readdir(dirptr))!=NULL) {
    if ((!fnmatch("cu.usbserial-*",entry->d_name,FNM_NOESCAPE)) ||
        (!fnmatch("ttyUSB*",entry->d_name,FNM_NOESCAPE))) 
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

char* get_home_dir (void) {
	char* dir = getenv("HOME");
	if(!dir) {
		struct passwd* pwd = getpwuid(getuid());
		if(pwd) dir = pwd->pw_dir;
		else { 
			fprintf(stderr, "Unable to determine home directory.\n");
			exit(1);
		}
	}
	return dir;
}

int main(int argc, char *argv[]) {
	int writefd, readfd;
	int fdesc;
	int i, ro=0;
	char *dir;
	char *device = NULL;	/* device name or NULL if stdin/out */
	char parameter_d_given = FALSE;

	mem_init();


	i=1;
	while(i<argc && argv[i][0]=='-') {
	  switch(argv[i][1]) {
	    case '?':
		usage();	/* usage() exits already */
		break;
	    case 'r':
		if(argv[i][2]=='o') {
		  ro=1;
		}
	    case 'd':
	    	parameter_d_given = TRUE;
		if (i < argc-2) {
		  i++;
		  device = argv[i];
		  if(!strcmp(device,"auto")) {
		    guess_device(&device);
		    /* exits on more or less than a single possibility */
		  }
		  if(!strcmp(device,"-")) device = NULL; 	// use stdin/out
		  printf("main: device = %s\n", device);
		}
 	     	break;
	    case 'A':
		// ignore that one, as it will be evaluated later by cmd_...
		break;
	    default:
		log_error("Unknown command line option %s\n", argv[i]);
		usage();
		break;
	  }
	  i++;
	}
	if(!parameter_d_given) guess_device(&device);

	if(argc == 1) {
		// Use default configuration if no parameters were given
		// Default assigns are made later
		dir = ".";
	} else
	{
		if(i!=argc-1) {
		  usage();
		}

		dir = argv[i++];
	}
	printf("dir=%s\n",dir);

	if(chdir(dir)<0) { 
	  fprintf(stderr, "Couldn't change to directory %s, errno=%d (%s)\n",
			dir, errno, strerror(errno));
	  exit(1);
	}

	if (device != NULL) {
		fdesc = open(device, O_RDWR | O_NOCTTY); // | O_NDELAY);
		if (fdesc < 0) {
		  /* error */
		  fprintf(stderr, "Could not open device %s, errno=%d (%s)\n", 
			device, errno, strerror(errno));
		  exit(1);
		}
		if(config_ser(fdesc) < 0) {
		  exit(1);
		}
		readfd = fdesc;
		writefd = readfd;
	}

	// we have the serial device open, now we can drop privileges
	drop_privileges();

	cmd_init();

	if(argc == 1) {
		// Default assigns
		char *fs_home;
		fs_home = (char*) malloc(strlen(get_home_dir()) + 4);
		strcpy(fs_home, "fs:");
		strcat(fs_home, get_home_dir());
		provider_assign(0, fs_home);
		provider_assign(1, "fs:/usr/local/xd2031/sample");
		provider_assign(2, "fs:/usr/local/xd2031/tools");
		provider_assign(3, "ftp:ftp.zimmers.net/pub/cbm");
		provider_assign(7, "http:www.zimmers.net/anonftp/pub/cbm/");
	} else cmd_assign_from_cmdline(argc, argv);

	cmd_loop(readfd, writefd);

	if (device != NULL) {
		close(fdesc);
	}
	return 0;	
}

