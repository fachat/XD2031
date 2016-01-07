/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2013 Andre Fachat

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

#include "os.h"

#include <sys/types.h>
#include <string.h>
#include <stdbool.h>

#include "registry.h"
#include "handler.h"
#include "errors.h"
#include "log.h"
#include "mem.h"
#include "provider.h"
#include "wireformat.h"
#include "wildcard.h"
#include "openpars.h"



// This file defines the file type handler interface. This is used
// to "wrap" files so that for example x00 (like P00, R00, ...) files 
// or compressed files can be handled.
//
// In the long run the di_provider will become a handler as well - so
// you can CD into a D64 file stored in a D81 image read from an FTP 
// server...

static registry_t handlers;

/*
 * register a new provider, usually called at startup
 */
void handler_register(handler_t *handler) {
	
	reg_append(&handlers, handler);
}

/*
 * initialize the provider registry
 */
void handler_init(void) {

	reg_init(&handlers, "handlers", 10);
}


void path_append(char **path, const char *filename) {
	// construct path
	if (strcmp(".", filename) != 0) {
		// not the "same directory"
		if (strcmp("..", filename) == 0) {
			// "up"
			char *p = strrchr(*path, dir_separator_char());
			if (p != NULL) {
				// not on root yet - shorten path
				p[0] = 0;
			}
		} else {
			// mem_append_str2 re-allocs the path
			mem_append_str2(path, dir_separator_string(), filename);
		}
	}
	log_debug("path_append(%s) -> %s\n", filename, *path);
}

//----------------------------------------------------------------------------


int handler_next(file_t *infile, uint8_t type, const char *pattern,  
		const char **outpattern, file_t **outfile) {

	log_debug("handler_next(infile=%s, pattern=%s)\n", infile->filename, pattern);

	int err = CBM_ERROR_FILE_NOT_FOUND;
	*outfile = NULL;

	for (int i = 0; ; i++) {
		handler_t *handler = reg_get(&handlers, i);
		if (handler == NULL) {
			// no handler found
			break;
		}
		// outpattern then points into the pattern string
		if ( handler->resolve(infile, outfile, type, pattern, outpattern) == CBM_ERROR_OK) {
			// worked ok.
			if (*outfile != NULL) {
				// found a handler
				err = CBM_ERROR_OK;
				break;
			}
		} else {
			log_error("Got %d as error from handler %s for %s\n", 
				err, handler->name, pattern);
			return err;
		}
	}

	if (*outfile == NULL) {
		// no wrapper has matched
		// check original name
		// outpattern then points into the pattern string
		if (compare_dirpattern(infile->filename, pattern, outpattern) != 0) {
			// match of original 
			err = CBM_ERROR_OK;
			*outfile = infile;
		}
	}

	return err;
}
	

/*
 * open a file/dir, internally
 *
 * This function takes the file name from an OPEN, and the endpoint defined for the 
 * drive as given in the OPEN, and recursively walks the path parts of that file name.
 *
 * Each path part is checked against the list of handlers for a match
 *
 * It returns the directory, as well as the first match in that directory, both as file_t 
 * objects.
 *
 * Note that in case of an empty directory, *outfile may be null.
 * 
 * TODO:
 * - handling of ".."
 * - handling of "."
 */

static int handler_resolve(endpoint_t *ep, file_t **outdir, file_t **outfile, 
		const char *inname, const char **outdirpath, const char **outpattern, openpars_t *pars) {

	log_entry("handler_resolve");

	int readflag;
	const char *outname = NULL;
	int err = CBM_ERROR_FAULT;
	file_t *file = NULL;
	file_t *direntry = NULL;

	char *path = NULL;

	int inlen = (inname == NULL) ? 0 : strlen(inname);
	char *name = NULL;
	// canonicalize the name
	if (inlen == 0) {
		// no name given - replace with pattern
		name = mem_alloc_str("*");
	} else
	if (inname[inlen-1] == CBM_PATH_SEPARATOR_CHAR) {
		// name ends with a path separator, add pattern
		name = mem_alloc_c(inlen + 2, "search path");
		strcpy(name, inname);
		strcat(name, "*");
	} else {
		// it's either a single pattern, or a path with a pattern
		name = mem_alloc_str(inname);
	}

	// runtime
	const char *namep = name;
	file_t *current_dir = NULL;
	file_t *wrapped_direntry = NULL;

	// handle root dir case
	// initial file struct, corresponds to root directory of endpoint
	//err = ep_create(ep, chan, outfile, inname, &outname);
	if (*namep == '/') {
		namep++;
	}

	path = mem_alloc_str("");
	current_dir = ep->ptype->root(ep);


	log_debug("current_dir (i.e. root) is %p, namep=%s\n", current_dir, namep);

	// through name canonicalization, we know namep does now not point to a 0

	current_dir->pattern = mem_alloc_str(namep);

	// loop as long as we have filename parts left
	// i.e. this is the depth loop in resolving a file path, one 
	// iteration in the loop per depth level.
	while (current_dir != NULL && namep != NULL && *namep != 0) {

		// loop over directory entries within a directory level,
		// i.e. match the file name within a directory
		//
		// note: loops over all directory entries, as we cannot simply match the name
		// due to the x00 pattern matching madness
		// note: may return NULL in direntry and still err== CBM_ERROR_OK, in case
		// we have an empty directory
		direntry = NULL;
		if (current_dir->handler->direntry != NULL 
			&& ((err = current_dir->handler->direntry(current_dir, &direntry, 1, &readflag, &outname)) 
				== CBM_ERROR_OK)
			) {

			log_debug("got direntry %p (%s)(current_dir is %p (%s))\n", direntry, 
				(direntry == NULL)?NULL:direntry->filename, current_dir,
				current_dir->filename);

			if (direntry != NULL) {
				file = direntry;
			}
		}

		log_debug("Found entry - err=%d, outname=%s, file=%p\n", err, outname, file);

		if (err != CBM_ERROR_OK || file == NULL) {
			break;
		}


		// found our directory entry and wrapped it in file
		if (*outname == 0) {
			// no more pattern left, so file should be what was required
			// i.e. we have raw access to container files like D64 or ZIP
			// (which is similar to the "$" file on non-load secondary addresses)
			break;
		}

		// outname points to the path separator trailing the matched file name pattern
		// From name canonicalization that is followed by at least a '*', if not further
		// patterns
		while (*outname == dir_separator_char()) {
			outname++;
		}
		// save rest of pattern in sub directory
		file->pattern = mem_alloc_str(outname);

		// append path, reduce "." and ".." in the process
		path_append(&path, file->filename);


		current_dir = file;
		namep = outname;
		file = NULL;

		// check with the container providers (i.e. endpoint providers)
		// whether to wrap it. Here e.g. d64 or zip files are wrapped into 
		// directory file_t handlers 
		wrapped_direntry = provider_wrap(current_dir);
		if (wrapped_direntry != NULL) {
			current_dir = wrapped_direntry;
		}
	}

	if (pars->filetype != FS_DIR_TYPE_UNKNOWN 
		&& file != NULL
		&& file->type != FS_DIR_TYPE_UNKNOWN
		&& file->type != pars->filetype) {
		err = CBM_ERROR_FILE_TYPE_MISMATCH;
	}

	// here we have:
	// - current_dir as the current directory
	// - file, the unwrapped first pattern match in that directory, maybe NULL
	// - a possibly large list of opened files, reachable through current_dir->parent etc
	// - namep pointing to the directory match pattern for the current directory
	log_debug("current_dir=%p, file=%p, namep=%s, path=%s\n", current_dir, file, namep, path);

	if (strchr(namep, dir_separator_char()) != NULL) {
		err = CBM_ERROR_DIR_NOT_FOUND;
	}

	*outdir = NULL;
	*outfile = NULL;
	if (outpattern != NULL) {
		*outpattern = NULL;
	}
	if (outdirpath != NULL) {
		*outdirpath = NULL;
	}

	if (err == CBM_ERROR_OK) {
		*outdir = current_dir;
		*outfile = file;
		if (outpattern != NULL) {
			*outpattern = mem_alloc_str(namep);
		}
		if (outdirpath != NULL) {
			*outdirpath = path;
			// prevent it from being freed
			path = NULL;
		}
	} else {
		// this should close all parents as well
		if (file != NULL) {
			file->handler->close(file, 1);
		} else
		if (current_dir != NULL) {
			current_dir->handler->close(current_dir, 1);
		}
	}
	mem_free(name);
	mem_free(path);

	log_debug("outdir=%p, outfile=%p, outpattern=%s, outdirpath=%s\n", 
				*outdir, *outfile, *outpattern, (outdirpath == NULL) ? "" :*outdirpath);
	log_exitr(err);	
	return err;
}


static void loose_parent(file_t *file, file_t *parent) {

	while (file != NULL) {
		if (file->parent == parent) {
			file->parent = NULL;
			return;
		}
		file = file->parent;
	}
}


/*
 * recursively resolve a dir from an endpoint using the given inname as path
 * and creating an endpoint for an assign from it
 */
int handler_resolve_assign(endpoint_t *ep, endpoint_t **outep, const char *resolve_path) {

	int err = CBM_ERROR_FAULT;
	file_t *dir = NULL;
	file_t *file = NULL;
	const char *pattern = NULL;
	openpars_t pars;
	*outep = NULL;

	openpars_init_options(&pars);

	int inlen = (resolve_path == NULL) ? 0 : strlen(resolve_path);
	char *name = NULL;
	// canonicalize the name
	if (inlen == 0) {
		// no name given 
		return CBM_ERROR_FAULT;
	}

	if (resolve_path[inlen-1] != CBM_PATH_SEPARATOR_CHAR) {
		// name ends with a path separator, add pattern
		name = mem_alloc_c(inlen + 3, "search path");
		strcpy(name, resolve_path);
		strcat(name, CBM_PATH_SEPARATOR_STR "*");
	} else {
		// it's either a single pattern, or a path with a pattern
		name = mem_alloc_str(resolve_path);
	}

	err = handler_resolve(ep, &dir, &file, resolve_path, NULL, &pattern, &pars);

	log_debug("handler_resolve_assign: resolve gave err=%d, file=%p (%s), dir=%p (%s), "
			"parent=%p, pattern=%s\n", 
			err, file, (file==NULL)?"":file->filename, 
			dir, (dir==NULL)?"":dir->filename, (dir==NULL)?NULL:dir->parent,
			(pattern==NULL)?"":pattern);

	if (err == CBM_ERROR_OK) {
		if (file != NULL) {
			file_t *wrapped_direntry = provider_wrap(file);

			if (wrapped_direntry != NULL) {
				// we want the file here, so we can close the dir and its parents
				dir = NULL;

				file = wrapped_direntry;
			}

			if (file->isdir) {
				if (file->endpoint->ptype->to_endpoint != NULL) {
					// to_endpoint must take care of parent dir(s)
					err = file->endpoint->ptype->to_endpoint(file, outep);
					//if (*outep != NULL) {
					//	(*outep)->is_assigned ++;
					//}
					dir = NULL;
				} else {
					log_warn("Endpoint %s does not support assign\n", 
									file->endpoint->ptype->name);
					err = CBM_ERROR_FAULT;
				}
			} else {
				err = CBM_ERROR_FILE_TYPE_MISMATCH;
			}
		} else {
			err = CBM_ERROR_FAULT;
		}	
	}

	if (dir != NULL) {
		// we want the file here, so we can close the dir and its parents
		dir->handler->close(dir, 1);
		loose_parent(file, dir);
	}
	if (err != CBM_ERROR_OK) {
		*outep = NULL;
		// on error
		if (file != NULL) {
			file->handler->close(file, 0);
		}
	}

	mem_free(name);

	return err;
}
	
/*
 * resolve a path, for CHDIR
 *
 * Uses handler_resolve() from above to do the bulk work
 */
int handler_resolve_path(endpoint_t *ep, const char *inname, const char **outpath) {

	int err = CBM_ERROR_FAULT;
	file_t *dir = NULL;
	file_t *file = NULL;
	const char *pattern = NULL;
	openpars_t pars;
	char *path = NULL;

	int inlen = strlen(inname);
	int ends_with_sep = (inlen != 0) && (inname[inlen - 1] == dir_separator_char());

	openpars_process_options(NULL, &pars);

	err = handler_resolve(ep, &dir, &file, inname, (const char**)&path, &pattern, &pars);

	if (err == CBM_ERROR_OK && file != NULL) {
	
		log_info("Path resolve gave %d\n", err);
		log_debug("Path resolve gave file=%p, path=%s\n", file, path);

		if (!ends_with_sep) {
			path_append(&path, file->filename);
		}

		*outpath = path;
	} else {
		if (path != NULL) {
			mem_free(path);
		}
		err = CBM_ERROR_FILE_NOT_FOUND;
	}

	if (file != NULL) {
		// we want the file here, so we can close the dir and its parents
		file->handler->close(file, 1);
	} else
	if (dir != NULL) {
		// we want the file here, so we can close the dir and its parents
		dir->handler->close(dir, 1);
	}

	if (pattern != NULL) {
		mem_free((char*)pattern);
	}

	return err;
}

/*
 * open a file, from fscmd
 *
 * Uses handler_resolve() from above to do the bulk work
 */

int handler_resolve_file(endpoint_t *ep, file_t **outfile, 
		const char *inname, const char *opts, uint8_t type) {

	int err = CBM_ERROR_FAULT;
	file_t *dir = NULL;
	file_t *file = NULL;
	const char *pattern = NULL;
	openpars_t pars;

	openpars_process_options((uint8_t*)opts, &pars);

	err = handler_resolve(ep, &dir, &file, inname, NULL, &pattern, &pars);

	if (err == CBM_ERROR_OK) {
	

		log_info("File resolve gave %d\n", err);
		log_debug("File resolve gave file=%p\n", file);
	
		switch (type) {
		case FS_OPEN_RD:
			if (file == NULL) {
				err = CBM_ERROR_FILE_NOT_FOUND;
			} else {
				err = file->handler->open(file, &pars, type);
			}
			break;
		case FS_OPEN_WR:
			if (file != NULL) {
				err = CBM_ERROR_FILE_EXISTS;
				break;
			}
			// fall-through
		case FS_OPEN_AP:
		case FS_OPEN_OW:
		case FS_OPEN_RW:
			if (file == NULL) {
				if (pars.filetype == FS_DIR_TYPE_REL && pars.recordlen == 0) {
					err = CBM_ERROR_FILE_NOT_FOUND;
					break;
				}
				err = dir->handler->create(dir, &file, pattern, &pars, type);
				if (err != CBM_ERROR_OK) {
					break;
				}
			} else {
				err = file->handler->open(file, &pars, type);
			}
			if (pars.filetype != FS_DIR_TYPE_UNKNOWN 
				&& file->type != pars.filetype) {
				err = CBM_ERROR_FILE_TYPE_MISMATCH;
				break; 
			}
			if (!file->writable) {
				err = CBM_ERROR_WRITE_PROTECT;
				break;
			}
			break;
		case FS_MKDIR:
			if (file != NULL) {
				err = CBM_ERROR_FILE_EXISTS;
			} else  {
				if (dir->handler->mkdir != NULL) {
					err = dir->handler->mkdir(dir, pattern, &pars);
				} else {
					err = CBM_ERROR_DIR_NOT_SUPPORTED;
				}
			}
			break;
		case FS_MOVE:
			// just check for existence
			if (file == NULL) {
				err = CBM_ERROR_FILE_NOT_FOUND;
			} else {
				*outfile = file;
			}
			break;
		}

		if (err == CBM_ERROR_OK) {
			if (pars.recordlen != 0) {
				if (file->recordlen != pars.recordlen
					|| file->type != FS_DIR_TYPE_REL) {
					err = CBM_ERROR_RECORD_NOT_PRESENT;
				}
			}
		}

		if (file != NULL) {
			if (type == FS_OPEN_AP) {
				err = file->handler->seek(file, 0, SEEKFLAG_END);
			}

			*outfile = file;

			// and loose the pointer to the parent
			loose_parent(file, dir);
		}
	}

	log_info("File open gave %d\n", err);

	if (err != CBM_ERROR_OK && err != CBM_ERROR_OPEN_REL) {
		// on error
		if (file != NULL) {
			file->handler->close(file, 0);
		}
		*outfile = NULL;
	}
	if (dir != NULL) {
		// we want the file here, so we can close the dir and its parents
		dir->handler->close(dir, 1);
	}

	if (pattern != NULL) {
		mem_free((char*)pattern);
	}

	return err;
}


/*
 * open a directory, from fscmd
 *
 * Uses handler_resolve() from above to do the bulk work
 */

int handler_resolve_dir(endpoint_t *ep, file_t **outdir, 
		const char *inname, const char **outpattern, const char *opts) {

	int err = CBM_ERROR_FAULT;
	file_t *dir = NULL;
	file_t *file = NULL;
	const char *pattern = NULL;
	openpars_t pars;

	if (outpattern != NULL) {
		*outpattern = NULL;
	}

	openpars_process_options((uint8_t*)opts, &pars);

	err = handler_resolve(ep, &dir, &file, inname, NULL, &pattern, &pars);

	log_debug("handler_resolve_dir: resolve gave err=%d, dir=%p (%s), parent=%p, pattern=%s\n", 
			err, dir, (dir==NULL)?"":dir->filename, (dir==NULL)?NULL:dir->parent,
			(pattern==NULL)?"":pattern);

	if (dir != NULL) {
		// we can close the parents anyway
		file_t *parent = NULL;
		parent = dir->handler->parent(dir);
		if (parent != NULL) {
			parent->handler->close(parent, 1);
			// forget reference so we don't try to close it again
			loose_parent(dir, parent);
		}

		if (err == CBM_ERROR_OK) {
	
			err = dir->handler->open(dir, &pars, FS_OPEN_DR);	
		}

		if (err == CBM_ERROR_OK) {
			*outdir = dir;	
			if (file != NULL) {
				file->handler->close(file, 0);
			}
		} else {
			if (dir != NULL) {
				dir->handler->close(dir, 0);
			}
			if (file != NULL) {
				file->handler->close(file, 0);
			}
		}
		
		if (pattern != NULL) {
			if (outpattern != NULL) {
				*outpattern = pattern;
			} else {
				mem_free((char*)pattern);
			}
		}
	}

	return err;
}

file_t *handler_parent(file_t *file) {
	return file->parent;
}

// --------------------------------------------------------------------------------------------
// default implementations for handler

void default_close(file_t *file, int recurse) {

	if (file->filename) {
		mem_free(file->filename);
		file->filename = NULL;
	}

	// we are a resolve wrapper, so close the inner file as well
	file->parent->handler->close(file->parent, recurse);

	// and then free the file struct memory
	mem_free(file);
}

int default_seek(file_t *file, long pos, int flag) {

	return file->parent->handler->seek(file->parent, pos, flag );
}

int default_truncate(file_t *file, long pos) {

	return file->parent->handler->truncate(file->parent, pos);
}

int default_read(file_t *file, char *buf, int len, int *readflg) {

	return file->parent->handler->readfile(file->parent, buf, len, readflg );
}

int default_write(file_t *file, const char *buf, int len, int writeflg) {

	return file->parent->handler->writefile(file->parent, buf, len, writeflg );
}

int default_open(file_t *file, openpars_t *pars, int opentype) {

        if (pars->filetype != FS_DIR_TYPE_UNKNOWN && pars->filetype != file->type) {
                log_debug("Expected file type %d, found file type %d\n", pars->filetype, file->type);
                return CBM_ERROR_FILE_TYPE_MISMATCH;
        }

        if (file->type == FS_DIR_TYPE_REL && (pars->recordlen != 0 && pars->recordlen != file->recordlen)) {
                return CBM_ERROR_RECORD_NOT_PRESENT;
        }

        openpars_t wrappedpars;
        wrappedpars.filetype = FS_DIR_TYPE_UNKNOWN;
        wrappedpars.recordlen = 0;

	cbm_errno_t rv = file->parent->handler->open(file->parent, &wrappedpars, opentype);
	if (rv == CBM_ERROR_OK) {
		rv = file->handler->seek(file, 0, SEEKFLAG_ABS);
	}
	return rv;
}

int default_scratch(file_t *file) {

	cbm_errno_t rv = file->parent->handler->scratch(file->parent);

	if (rv == CBM_ERROR_OK) {	
		// parent file is closed
		mem_free(file);
	}

	return rv;
}

file_t* default_parent(file_t *file) {
	if (file->parent != NULL) {
		return file->parent->handler->parent(file->parent);
	}
	return NULL;
}

int default_flush(file_t *file) {
	if (file->parent != NULL) {
		return file->parent->handler->flush(file->parent);
	}
	return CBM_ERROR_FAULT;
}


size_t default_realsize(file_t *file) {

	if (file->parent != NULL) {
		return file->parent->handler->realsize(file->parent);
	}
	return CBM_ERROR_FAULT;
}



