
/****************************************************************************

    commandline parsing
    Copyright (C) 2016,2017 Andre Fachat

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


#ifndef CMDLINE_H
#define CMDLINE_H

#include "errors.h"

// the different types of cmdline entries for the phases bitmap
#define CMDL_INIT       1       /* help, verbose */
#define CMDL_CFG        2       /* config file */
#define CMDL_PARAM      4       /* normal parameters */
#define CMDL_CMD        8       /* commands to be executed after params */
#define CMDL_UI         16      /* UI commands */

typedef cbm_errno_t err_t;

#define	E_OK	0
#define	E_ABORT	-1

// init the cmdline parser 
void cmdline_module_init();
// end
void cmdline_module_free();

// parse the options
err_t cmdline_parse(int *argc, char *argv[], int phase);
err_t cmdline_parse_cfg(char *arg, int phase);

typedef enum {
	// no parameter, sets a bool when present with/without "no-" prefix
	PARTYPE_FLAG,
	// with required parameter
	PARTYPE_PARAM,
	// with enumerated parameter
	PARTYPE_ENUM,
} param_type_t;

typedef struct {
	const char	*value;
	const char	*description;
} param_enum_t;

typedef struct {
	// name of cmdline param (long for "--" option)
	const char 	*name;		
	// name of cmdline param (short, i.e. single char for "-" option)
	const char 	*shortname;		

	// in which phase the parameter will be evaluated
	int		phase;

	// option has a parameter
	param_type_t	type;

	// function to call when parsed and type is PARTYPE_PARAM, PARTYPE_ENUM
	// ival is the index number of the option in case of PARTYPE_ENUM
	err_t 		(*setfunc)(const char *value, void *extra_param, int ival);

	// function to call when parsed and type is PARTYPE_FLAG
	err_t 		(*setflag)(int flag, void *extra_param);

	// extra param to be passed to set function
	void		*extra_param;

	// description
	const char	*description;

	// return optional enum values for enumerated parameter, last entry must be NULL
	param_enum_t*	(*values)();
} cmdline_t;

void cmdline_usage();

// template method where extra_param is the pointer to an int variable to set
err_t cmdline_set_flag(int flag, void *extra_param);
// template method where extra_param is the pointer to a string variable to set
err_t cmdline_set_param(const char *value, void *extra_param, int ival);
 
void cmdline_register(const cmdline_t *param);

// register a list of #num options from an array of options starting at param
void cmdline_register_mult(const cmdline_t *param, int num);

// allocate an array of param_enum_t structs for use as param option
param_enum_t *cmdline_pval_alloc(int n);

#endif

