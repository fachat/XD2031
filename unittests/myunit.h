/****************************************************************************

    Minimal unit testing framework 
    Copyright (C) 2014 Andre Fachat

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

#ifndef _MYUNIT_H
#define _MYUNIT_H


// for the inlined asserts

typedef enum {
	MULOG_QUIET = 0,
	MULOG_ERROR = 1,
	MULOG_INFO = 2,
	MULOG_DEBUG = 3
} mulog_t;

extern mulog_t mu_logging_level;
extern int mu_numerr;

void mu_init(int argc, const char *argv[]);
void mu_add(const char *name, void (*test)(void));
void mu_run(void);

// generic functions
void mulog_log(mulog_t level, const char *msg, va_list ap);
int mu_strcmp(const char *is, const char *shouldbe);


static inline void mu_error(const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	if (MULOG_ERROR <= mu_logging_level) {
		mulog_log(MULOG_ERROR, msg, args);
	}
}
static inline void mu_info(const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	if (MULOG_INFO <= mu_logging_level) {
		mulog_log(MULOG_INFO, msg, args);
	}
}
static inline void mu_debug(const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	if (MULOG_DEBUG <= mu_logging_level) {
		mulog_log(MULOG_DEBUG, msg, args);
	}
}
static inline void mu_log(mulog_t level, const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	if (level <= mu_logging_level) {
		mulog_log(level, msg, args);
	}
}

static inline void mu_assert_err(const char *name, bool istrue) {
	if (!istrue) {
		mu_error("Assert %s failed!\n", name);
		mu_numerr++;
	}
}
static inline void mu_assert_info(const char *name, bool istrue) {
	if (!istrue) {
		mu_info("Assert %s failed!\n", name);
		mu_numerr++;
	}
}
static inline void mu_assert_debug(const char *name, bool istrue) {
	if (!istrue) {
		mu_debug("Assert %s failed!\n", name);
		mu_numerr++;
	}
}

static inline void mu_assert_err_str_eq(const char *name, const char *is, const char *shouldbe) {
	if (mu_strcmp(is, shouldbe)) {
		mu_error("Assert %s failed - is '%s', should be '%s'!\n", name, is, shouldbe);
		mu_numerr++;
	}
}
static inline void mu_assert_info_str_eq(const char *name, const char *is, const char *shouldbe) {
	if (mu_strcmp(is, shouldbe)) {
		mu_info("Assert %s failed - is '%s', should be '%s'!\n", name, is, shouldbe);
		mu_numerr++;
	}
}
static inline void mu_assert_debug_str_eq(const char *name, const char *is, const char *shouldbe) {
	if (mu_strcmp(is, shouldbe)) {
		mu_debug("Assert %s failed - is '%s', should be '%s'!\n", name, is, shouldbe);
		mu_numerr++;
	}
}

 
#endif
