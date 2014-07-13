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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include "myunit.h"

// needed for inlined logging code
mulog_t mu_logging_level = MULOG_DEBUG;
int mu_numerr = 0;

// internal

#define	MU_INITIAL_TESTLIST	20

typedef struct {
	void 		(*test)(void);
	const char 	*name;
} test_t;

static test_t *testlist;
static int cap_tests;
static int num_tests;

static int muargp;
static int muargn;
static const char **muargv;


void mu_usage(int rv) {
        printf("Options:\n"
                "   -E          Error level logging\n"
                "   -I          Info level logging\n"
                "   -D          Debug level logging\n"
                "   -q          Quiet - only print summary for each test\n"
                "   -?          gives you this help text\n"
		"Any further argument are test case names that are then only executed.\n"
		"If no test case name is given, all tests are executed\n"
        );
        exit(rv);
}

void mu_init(int argc, const char *argv[]) {

	cap_tests = MU_INITIAL_TESTLIST;
	num_tests = 0;
	testlist = malloc(cap_tests * sizeof(test_t));

	muargp = 1;
	muargn = argc;
	muargv = argv;
	while (muargp < muargn && muargv[muargp][0] == '-') {
	
		switch(muargv[muargp][1]) {
		case '?':
			mu_usage(1);
			muargp++;
			break;
		case 'E':
			mu_logging_level = MULOG_ERROR;
			muargp++;
			break;
		case 'I':
			mu_logging_level = MULOG_INFO;
			muargp++;
			break;
		case 'D':
			mu_logging_level = MULOG_DEBUG;
			muargp++;
			break;
		case 'q':
			mu_logging_level = MULOG_QUIET;
			muargp++;
			break;
		default:
			printf("Unknown option %s\n", muargv[muargp]);
			mu_usage(2);
			break;
		}
	}
}

void mu_add(const char *tname, void (*tfunc)(void)) {

	if (num_tests >= cap_tests) {
		cap_tests *= 2;
		testlist = realloc(testlist, cap_tests * sizeof(test_t));
		if (testlist == NULL) {
			fprintf(stderr, "Could not enlarge test list, failed to alloc %d elements\n",
					cap_tests);
			exit(1);
		}
	}
	testlist[num_tests].test = tfunc;
	testlist[num_tests].name = tname;

	num_tests++;
}

static void mu_exec(test_t *mutest) {
	mu_numerr = 0;
	mu_info("Executing test %s\n", mutest->name);

	mutest->test();

	if (mu_numerr == 0) {
		mu_log(MULOG_QUIET, "Test %s -> Ok\n", (char*)mutest->name);
	} else {
		mu_log(MULOG_QUIET, "Test %s -> %d errors\n", (char*)mutest->name, mu_numerr);
	}
}

void mu_run() {

	if (muargp >= muargn) {
		for (int i = 0; i < num_tests; i++) {
			test_t *mutest = &testlist[i];
			mu_exec(mutest);
		}	
	} 

	while (muargp < muargn) {
		// execute each test in the command line order

		for (int i = 0; i < num_tests; i++) {

			if (!strcmp(testlist[i].name, muargv[muargp])) {
				// found
				test_t *mutest = &testlist[i];
				mu_exec(mutest);
				
				muargp++;
				break;	// out of for, back to while
			}
		}
	}
}

void mulog_log(mulog_t level, const char *msg, va_list ap) {

	switch(level) {
	case MULOG_ERROR:
		printf("ERR: ");
		break;
	case MULOG_INFO:
		printf("INF: ");
		break;
	case MULOG_DEBUG:
		printf("DBG: ");
		break;
	case MULOG_QUIET:
		printf(">>>: ");
		break;
	}

	vprintf(msg, ap);
}

		

int mu_strcmp(const char *is, const char *shouldbe) {

        if (is == NULL && shouldbe == NULL) {
                return 0;       // OK
        }

        if (is == NULL) {
                mu_debug("String result is NULL, but should be '%s'\n", shouldbe);
                return 1;       // NOK
        }
        if (shouldbe == NULL) {
                mu_debug("String result is '%s', but should be NULL\n", is);
                return 1;       // NOK
        }

        int v = strcmp(is, shouldbe);
        if (v) {
                mu_debug("String result is '%s', but should be '%s'\n", is, shouldbe);
        }
        return v;
}


