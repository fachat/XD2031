/**************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat, Nils Eilers

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

***************************************************************************/

/** @brief Terminal functions to set colors */

// ----- WINDOWS -----------------------------------------------------------

#ifdef _WIN32

#include <windows.h>
#include <stdbool.h>
#include "log.h"

static HANDLE hStdout;
static CONSOLE_SCREEN_BUFFER_INFO default_settings;
static WORD default_attributes;
static bool usecolors = false;

void color_default (void) {
	SetConsoleTextAttribute (hStdout, default_attributes);
}

int terminal_init(void) {
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdout == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		log_error("Error %d while getting stdout handle\n", err);
		return 1;
	}

	// Save current text colors as default
	if(!GetConsoleScreenBufferInfo(hStdout, &default_settings)) return 1;

	default_attributes = default_settings.wAttributes;
        usecolors = true;

	atexit(color_default);
	return 0;
}

void color_textcolor(WORD attr) {
	if (usecolors) SetConsoleTextAttribute (hStdout, attr);
}

// Set foreground color
static inline void color_textcolor_red (void) {
	color_textcolor(FOREGROUND_RED | FOREGROUND_INTENSITY);
}

static inline void color_textcolor_green (void) {
	color_textcolor(FOREGROUND_GREEN);
}

static inline void color_textcolor_yellow (void) {
	color_textcolor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
}

static inline void color_textcolor_blue (void) {
	color_textcolor(FOREGROUND_BLUE | FOREGROUND_INTENSITY);
}

static inline void color_textcolor_magenta (void) {
	color_textcolor(FOREGROUND_BLUE | FOREGROUND_RED);
}

static inline void color_textcolor_white (void) {
	color_textcolor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

static inline void color_textcolor_cyan (void) {
	color_textcolor(FOREGROUND_BLUE | FOREGROUND_GREEN);
}

#else

// ----- POSIX -------------------------------------------------------------

#ifdef __STRICT_ANSI__		/* enable strdup() */
#undef __STRICT_ANSI__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <term.h>
#include <ncurses.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "terminal.h"
#include "log.h"


static int colors_available = 0;
static char *default_color, *red, *green, *yellow, *blue, *magenta, *white, *cyan;

// Reset terminal
void terminal_reset (void) {
	if (colors_available) printf("%s", default_color);
}

static void color_set_monochrome (void) {
	default_color =  red = green = yellow = blue = magenta = white = cyan = "";
	colors_available = 0;
}


// Init terminal and determine color command strings
int terminal_init (void) {
	color_set_monochrome();
	if (!isatty(STDOUT_FILENO)) return 1;

	if (setupterm (NULL, STDOUT_FILENO, NULL)) {
		log_error("Unable to configure terminal\n");
		return 1;
	}

        char *boldstr = NULL;
        if(tigetstr("bold")) boldstr = strdup(tigetstr("bold"));

	if (tigetstr("setaf")) {
		log_debug("Using setaf color definitions\n");
		red	= strdup(tparm(tigetstr("setaf"), 1, 0, 0, 0, 0, 0, 0, 0, 0));
		green	= strdup(tparm(tigetstr("setaf"), 2, 0, 0, 0, 0, 0, 0, 0, 0));
		yellow	= strdup(tparm(tigetstr("setaf"), 3, 0, 0, 0, 0, 0, 0, 0, 0));
                if(boldstr) {
		   size_t blue_len = strlen(boldstr) +
                      strlen(tparm(tigetstr("setaf"), 4, 0, 0, 0, 0, 0, 0, 0, 0)) + 1;
		   blue	= malloc(blue_len);
                   if(!blue) {
                      log_error("malloc failed!\n");
                      exit(1);
                   }
                   strncpy(blue, boldstr, blue_len);
                   strncat(blue, tparm(tigetstr("setaf"), 4, 0, 0, 0, 0, 0, 0, 0, 0), blue_len - strlen(blue));
                } else blue = strdup(tparm(tigetstr("setaf"), 4, 0, 0, 0, 0, 0, 0, 0, 0));
		magenta = strdup(tparm(tigetstr("setaf"), 5, 0, 0, 0, 0, 0, 0, 0, 0));
		cyan	= strdup(tparm(tigetstr("setaf"), 6, 0, 0, 0, 0, 0, 0, 0, 0));
		white	= strdup(tparm(tigetstr("setaf"), 7, 0, 0, 0, 0, 0, 0, 0, 0));
		colors_available = 1;
		atexit(terminal_reset);
	} else if (tigetstr("setf")) {
		log_debug("Using setf color definitions\n");
		red	= strdup(tparm(tigetstr("setf"), 4, 0, 0, 0, 0, 0, 0, 0, 0));
		green	= strdup(tparm(tigetstr("setf"), 2, 0, 0, 0, 0, 0, 0, 0, 0));
		yellow	= strdup(tparm(tigetstr("setf"), 6, 0, 0, 0, 0, 0, 0, 0, 0));
		blue	= strdup(tparm(tigetstr("setf"), 1, 0, 0, 0, 0, 0, 0, 0, 0));
		magenta = strdup(tparm(tigetstr("setf"), 5, 0, 0, 0, 0, 0, 0, 0, 0));
		cyan	= strdup(tparm(tigetstr("setf"), 3, 0, 0, 0, 0, 0, 0, 0, 0));
		white	= strdup(tparm(tigetstr("setf"), 7, 0, 0, 0, 0, 0, 0, 0, 0));
		colors_available = 1;
		atexit(terminal_reset);
	} else {
		log_warn("Unable to determine color commands for your terminal\n");
		return 1;
	}

	char *r = tigetstr("sgr0");
	if (r) default_color = strdup(r);
	else {
		color_set_monochrome();
		log_error("Unable to get default color string\n");
		return 1;
	}

	return 0;
}

static void color_textcolor(char *c) {
	if (colors_available) printf("%s", c);
}

void color_default(void){
	if (colors_available) terminal_reset();
}

// Set foreground color
static inline void color_textcolor_red     (void) { color_textcolor(red    ); }
static inline void color_textcolor_yellow  (void) { color_textcolor(yellow ); }
static inline void color_textcolor_blue    (void) { color_textcolor(blue   ); }
static inline void color_textcolor_magenta (void) { color_textcolor(magenta); }
static inline void color_textcolor_cyan    (void) { color_textcolor(cyan   ); }
//static inline void color_textcolor_green (void) { color_textcolor(green  ); }
//static inline void color_textcolor_white (void) { color_textcolor(white  ); }

#endif // _WIN32

// ----- COMMON ------------------------------------------------------------

// Logical colors
void color_log_term(void)		{ color_textcolor_cyan();	}
void color_log_error(void)		{ color_textcolor_red();	}
void color_log_warn(void)		{ color_textcolor_yellow();	}
void color_log_info(void)		{ color_textcolor_blue();	}
void color_log_debug(void)		{ color_textcolor_magenta();	}
