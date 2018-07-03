
/****************************************************************************

    commandline parsing
    Copyright (C) 2016-2017 Andre Fachat

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

#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include "hashmap.h"
#include "array_list.h"
#include "cmdline.h"
#include "err.h"
#include "mem.h"

// hash from param name to cmdline_t sruct for quick find
static hash_t *params = NULL;
static hash_t *shorts = NULL;

// list of parameters for help output
static list_t *paramlist = NULL;

static const char *prg_name = NULL;

extern err_t usage(int flag, void* extra);

void cmdline_usage() {
	
	list_iterator_t *iter = list_iterator(paramlist);
	cmdline_t *param = NULL;
	while ( (param = list_iterator_next(iter)) ) {
		switch(param->type) {
		case PARTYPE_FLAG:
			if (param->name) {
				printf("  --%s\n", param->name);
			} 
			if (param->shortname) {
				printf("  -%s\n", param->shortname);
			}
			printf("\t%s\n", param->description);
			break;
		case PARTYPE_PARAM:
		case PARTYPE_ENUM:
			if (param->name) {
				printf("  --%s=<value>\n", param->name);
			} 
			if (param->shortname) {
				printf("  -%s<value>\n", param->shortname);
			}
			printf("\t%s\n", param->description);
			if (param->type == PARTYPE_ENUM) {
				param_enum_t *options = param->values();
				int i = 0;
				while (options[i].value) {
					printf("\t'%s'\t%s\n", options[i].value, options[i].description);
					i++;
				}
			}
			break;
		}
	}
	list_iterator_free(iter);
}

static const cmdline_t help[] = {
	{ "help", "?", PARTYPE_FLAG, NULL, usage, NULL, "Show this help", NULL },
};

static type_t param_memtype = {
	"param_enum_t",
	sizeof(param_enum_t),
	NULL
};

static const char *longkey_from_param(const void *entry) {
	return ((cmdline_t*)entry)->name;
}

static const char *shortkey_from_param(const void *entry) {
	return ((cmdline_t*)entry)->shortname;
}

// TODO init the cmdline parser with the name as which the program was called
// for example "a65k", but also "xa65"
void cmdline_module_init() {

	params = hash_init_stringkey(50, 25, longkey_from_param);
	shorts = hash_init_stringkey(50, 25, shortkey_from_param);
	paramlist = array_list_init(20);

	cmdline_register_mult(help, sizeof(help)/sizeof(cmdline_t));
}

void cmdline_module_free() {
	
	list_free(paramlist, NULL);
	hash_free(params, NULL);
	hash_free(shorts, NULL);
}

// allocate an array of param_enum_t structs for use as param option
param_enum_t *cmdline_pval_alloc(int n) {

	return mem_alloc_n(n, &param_memtype);
}


// template method where extra_param is the pointer to an int variable to set
err_t cmdline_set_flag(int flag, void *extra_param) {
	*((int*)extra_param) = flag;
	return E_OK;
}
// template method where extra_param is the pointer to an string variable to set
err_t cmdline_set_param(const char *param, void *extra_param, int ival) {
	(void) ival;
	*((const char**)extra_param) = param;
	return E_OK;
}

void cmdline_register_mult(const cmdline_t *param, int num ) {
	for (int i = 0; i < num; i++) {
		cmdline_register(&param[i]);
	}
}

void cmdline_register(const cmdline_t *param) {
	
	if (param->name && hash_put(params, (void*)param) != NULL) {
		// must not happen
		exit(1);
	}
	if (param->shortname && hash_put(shorts, (void*)param) != NULL) {
		// must not happen
		exit(1);
	}
	list_add(paramlist, (void*)param);
}

static err_t cmdline_parse_long(char *name, cmdline_t **opt, char **val, int *flag) {
	err_t rv = E_OK;
	*val = NULL;
	*flag = 1;
	char *end = index(name, '=');
	if (end != NULL) {
		(*val) = end + 1;
		end[0] = 0;
	}

	// lookup option
	*opt = hash_get(params, name);
	if (*opt == NULL) {
		// check "no-" flag option
		if (name == strstr(name, "no-")) {
			name = name+3;
			*flag = 0;
			*opt = hash_get(params, name);
			if (*opt != NULL && (*opt)->type != PARTYPE_FLAG) {
				*opt = NULL;
			}
		}
	}
	if (*opt == NULL) {
		log_error("unknown long cmdline parameter: %s", name);
		rv = E_ABORT;
	}
	return rv;
}

/*
 * parse short cmdline params; call flags in case multiple flag options
 * return param option in case of last one is option
 */
static err_t cmdline_parse_short(char *pname, cmdline_t **opt, char **val, int flag) {
	err_t rv = E_OK;
	*val = NULL;

	do {
		char *name = mem_alloc_strn(pname, 1);
		*opt = hash_get(shorts, name);
		mem_free(name);
		if (*opt != NULL) {
			if ((*opt)->type == PARTYPE_FLAG) {
				rv = (*opt)->setflag(flag, (*opt)->extra_param);
				*opt = NULL; // done with this one
			} else {
				*val = pname+1;
				return E_OK;
			}
		} else {
			log_error("unknown short cmdline parameter: %s", pname);
			rv = E_ABORT;
		}
	} while ((++pname)[0] != 0);

	return rv;
}

err_t cmdline_parse(int argc, char *argv[]) {

	err_t rv = E_OK;

	prg_name = argv[0];

        int i = 1;
        while (i < argc && !rv) {
       		char *val = NULL;
		cmdline_t *opt = NULL;
		int flag = 1;
         	if (argv[i][0] == '-') {
			if (argv[i][1] == '-') {
				rv = cmdline_parse_long(argv[i]+2, &opt, &val, &flag);
			} else {
				rv = cmdline_parse_short(argv[i]+1, &opt, &val, flag);
				if (val != NULL && *val == 0) {
					// option needed, but value not set
					if (i+1 < argc && argv[i+1][0] != '-') {
						val = argv[i+1];
						i++;
					} else {
						log_error("Missing parameter for option %s", opt->name ? opt->name : opt->shortname);
						rv = E_ABORT;
					}
				}
			}
		} else if (argv[i][0] == '+') {
			flag = 0;
			rv = cmdline_parse_short(argv[i]+1, &opt, &val, flag);
                }   

		param_enum_t *values = NULL;
		if (opt != NULL) {
			switch (opt->type) {
			case PARTYPE_FLAG:
				rv = opt->setflag(flag, opt->extra_param);
				break;
			case PARTYPE_PARAM:
				if (val == NULL) {
					log_error("Missing parameter for option %s", opt->name ? opt->name : opt->shortname);
					rv = E_ABORT;
					break;
				}
				rv = opt->setfunc(val, opt->extra_param, -1);
				if (rv) {
					log_error("Missing parameter for option %s", opt->name ? opt->name : opt->shortname);
				}
				break;
			case PARTYPE_ENUM:
				values = opt->values();
				int i = 0;
				while (values[i].value) {
					if (!strcmp(values[i].value, val)) {
						opt->setfunc(val, opt->extra_param, i);
						break;
					}
					i++;
				}
				if (!values[i].value) {
					log_error("Unknown or missing parameter for option %s", 
								opt->name ? opt->name : opt->shortname);
					rv = E_ABORT;
				}
				break;
			}
		}
		i++;
        }   
	return rv;
}




