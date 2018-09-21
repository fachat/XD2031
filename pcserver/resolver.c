
/****************************************************************************

    Commodore filesystem server
    Copyright (C) 2012,2018 Andre Fachat

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


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <ctype.h>
#include <inttypes.h>

#include "log.h"
#include "provider.h"
#include "endpoints.h"
#include "handler.h"

static int resolve_scan_int(file_t *dir, const char **pattern, int num_pattern, bool fixpattern, 
		charset_t outcset, bool isdirscan, direntry_t **outde, int *rdflag);

/** 
 * resolve the endpoint for a given pattern. This is separated from the
 * file-path resolve, as a found endpoint can be reused in later 
 * file pattern (e.g. in "C0:foo=bar"), where endpoint drive "0" is reused
 * for the second file-path "bar" as well.
 *
 * ep is not changed if not endpoint is found in the pattern, so it should
 * be set to either NULL or the endpoint to reuse before the call.
 * If an endpoint is found, ep is set to it.
 *
 * pattern should be set to the beginning of a provider-address as input,
 * and will be set to the beginning of the file-path on successful return.
 *
 * provider_lookup uses the XD2031 name format, i.e. first byte is drive
 * (or NAMEINFO_UNDEF_DRIVE), rest until the zero-byte is file name.
 * It then identifies the drive, puts the CD path before the name if it
 * is not absolute, and allocates the new name that it returns
 *
 * The privileged flag is given to the endpoint, to enable different handling
 * esp. of the fs provider. Priviliged access starts at /, non-privileged 
 * starts at the run directory.
 */
int resolve_endpoint(drive_and_name_t *dname, charset_t cset, int privileged, endpoint_t **outep) {

        if (dname->drive == NAMEINFO_LAST_DRIVE) {
		// this means there was no drive or provider defined; we can just return
		// as ep has been set to the default value by the caller.
		return CBM_ERROR_OK;
        }

	if (dname->drive == NAMEINFO_UNDEF_DRIVE) {
		if (dname->drivename != NULL 
			&& strlen((char*)dname->drivename) == 1
			&& isdigit(dname->drivename[0])) {
			dname->drive = dname->drivename[0] & 0x0f;
		}
	}

        if (dname->drive == NAMEINFO_UNDEF_DRIVE) {
                if (dname->drivename == NULL || dname->drivename[0] == 0) {
                        // no name specified, so return NULL (no provider found)
                        return CBM_ERROR_DRIVE_NOT_READY;
                }
                // the drive is not specified by number, but by provider name
		// TODO: charset-aware strchr, only works for provider with ASCII names right now
		
                log_debug("Trying to find provider for: %s\n", dname->drivename);

		provider_t *prov = provider_find((const char*)dname->drivename);

                if (prov != NULL) {
                        // we got a provider, but no endpoint yet

	                log_debug("Found provider '%s', trying to create temporary endpoint for %s\n",
                                               prov->name, dname->name);

                        if (prov->tempep != NULL) {
                               	endpoint_t *ep = prov->tempep((char**)&dname->name, cset, privileged);
                                if (ep != NULL) {
                                        log_debug("Created temporary endpoint %p\n", ep);
                                        ep->is_temporary = 1;
					*outep = ep;
					return CBM_ERROR_OK;
                                }
				log_error("Provider '%s' did not provide endpoint!\n", prov->name);
                        } else {
                                log_error("Provider '%s' does not support temporary drives\n",
                                                  prov->name);
                        }
                        return CBM_ERROR_DRIVE_NOT_READY;
                } else {
                        log_info("No provider name given for undef'd drive '%s', trying default %s\n",
                                                              dname->drivename, (*outep)?(*outep)->ptype->name:"-");
			return CBM_ERROR_OK;
                }
        }

        log_debug("Trying to resolve drive %d with name '%s'\n", dname->drive, dname->drivename);

	// NOTE: cdpath not yet used!
	ept_t *ept = endpoints_find(dname->drive);

	if (ept != NULL) {
		*outep = ept->ep;
		return CBM_ERROR_OK;
	}
	return CBM_ERROR_DRIVE_NOT_READY;
}


/**
 * resolve a given file-path (pattern) from the directory given in *dir,
 * to the final directory (returned in *dir)
 * and the filename rest of the given file-path (returned in pattern)
 */
int resolve_dir(const char **pattern, charset_t cset, file_t **inoutdir) {

	int rv = CBM_ERROR_OK;

	file_t *dir = *inoutdir;

	do {
		if (dir->handler->resolve2) {
			rv = dir->handler->resolve2(pattern, cset, &dir);
		} else {
			if (strlen(*pattern) == 0 || strchr(*pattern, '/') == NULL) {
				rv = CBM_ERROR_FILE_EXISTS;
			} else {
				rv = CBM_ERROR_DIR_NOT_FOUND;
			}
		}

		if (rv == CBM_ERROR_SYNTAX_WILDCARDS || rv == CBM_ERROR_DIR_NOT_FOUND) {

			direntry_t *de;
			const char *p = *pattern;
			int rdflag = 0;
			file_t *fp = NULL;

			
			rv = resolve_scan_int(dir, &p, 1, true, cset, false, &de, &rdflag);

			if (rv == CBM_ERROR_OK) {
				// open the found dir entry
				rv = de->handler->open2(de, NULL, FS_OPEN_DR, &fp);

				if (rv == CBM_ERROR_OK) {
					// close old dir
					dir->handler->fclose(dir, NULL, NULL);
					// new scan dir
					dir = fp;
					*pattern = p;

					if (!strchr(*pattern, '/')) {
						rv = CBM_ERROR_FILE_EXISTS;
					}
				}
				de->handler->declose(de);
				de = NULL;

			}
		}

		if (rv == CBM_ERROR_FILE_EXISTS) {
			// the rest of the pattern is a file name (-> "File'name' exists", even 
			// if no actual file)
			//dir->pattern = mem_alloc_str(*pattern);
			rv = CBM_ERROR_OK;
			// found name
			break;
		}
	
	} while(rv == CBM_ERROR_OK);

	if (rv == CBM_ERROR_OK) {
		*inoutdir = dir;
	}
	return rv;
}


/**
 * scan a given directory (dir) for a search file-pattern (pattern), returning
 * the resulting directory entry in direntry. The pattern in/out parameter
 * is moved to behind the consumed file-pattern. 
 *
 * This method can be called multiple times to find all matching directory
 * entries (as long as pattern is reset between calls).
 *
 * Note that the directory entry is potentially wrapped in case an encapsulated
 * file (e.g. .gz, .P00) or directory (.zip, .D64) is detected.
 *
 * When isdirscan is true, the directory entries for disk header and blocks free
 * are also returned where available.
 *
 * rdflag is set to return side-channel information and consists of multiple ORd
 * values.
 * 	READFLAG_EOF	-> next read will not return further data, so set EOF
 */
static int resolve_scan_int(file_t *dir, const char **pattern, int num_pattern, bool fixpattern, 
		charset_t outcset, bool isdirscan, direntry_t **outde, int *rdflag) {

	log_debug("resolve_scan: pattern='%s'\n", *pattern);

	int rv = CBM_ERROR_OK;

        const char *scanpattern = NULL;
        const char *name = NULL;
	direntry_t *direntry = NULL;
	direntry_t *wrapped = NULL;
	bool found = false;

        do {
        	rv = dir->handler->direntry2(dir, &direntry, isdirscan, rdflag, *pattern, outcset);

		if (rv != CBM_ERROR_OK 
			|| !direntry
			|| !direntry->name
			|| direntry->mode == FS_DIR_MOD_NAM
			|| direntry->mode == FS_DIR_MOD_FRE) {
			break;
		}

		// match unwrapped entry (to enable unwrapped "foo.d64" addressing)
		found = false;
		for (int i = 0; i < num_pattern; i++) {
                	scanpattern = pattern[i];
                	name = (const char*)direntry->name;
			log_debug("match: pattern '%s' with name '%s'\n", scanpattern, name);
			if (cconv_matcher(outcset, direntry->cset) (&scanpattern, &name, false )) {
				found = true;
				break;
			}
		}

		// wrap
		rv = handler_wrap(direntry, &wrapped);
		if (rv == CBM_ERROR_OK
			&& wrapped != NULL) {
			direntry = wrapped;
		}

		// match wrapped entry (to enable match as in directory entry)
		for (int i = 0; i < num_pattern; i++) {
                	scanpattern = pattern[i];
                	name = (const char*)direntry->name;
			log_debug("match: pattern '%s' with name '%s'\n", scanpattern, name);
			if (cconv_matcher(outcset, direntry->cset) (&scanpattern, &name, false )) {
				found = true;
				break;
			}
		}
		if (!found) {
			direntry->handler->declose(direntry);
		}

        } while (!found);

	if (found && fixpattern) {
		if (*scanpattern == '/') {
			scanpattern++;
		}
		pattern[0] = scanpattern;
	}
	*outde = direntry;

	return rv;
}

int resolve_scan(file_t *dir, const char **pattern, int num_pattern, charset_t outcset, bool isdirscan,
	direntry_t **outde, int *rdflag) {

	return resolve_scan_int(dir, pattern, num_pattern, false, outcset, isdirscan, outde, rdflag);
}


/**
 * scan a directory and open the file, optionally creating it.
 * Does _not_ close the directory so you can call it repeatedly
 * to open multiple files matching the same pattern.
 */
int resolve_open(file_t *dir,
                const char *inname, charset_t cset, openpars_t *pars, uint8_t type, file_t **outfile) {

        file_t *file = NULL;
        int rdflag = 0;
        direntry_t *dirent;

        // now resolve the actual filename
        int rv = resolve_scan_int(dir, &inname, 1, true, cset, false, &dirent, &rdflag);

	if (rv == CBM_ERROR_OK && dirent && pars->filetype != FS_DIR_TYPE_UNKNOWN) {
		// we have a direntry and must check file types
		if (dirent->type != pars->filetype) {
			rv = CBM_ERROR_FILE_TYPE_MISMATCH;
		}
	}

        if (rv == CBM_ERROR_OK || rv == CBM_ERROR_FILE_NOT_FOUND) {
        	// ok, we have the directory entry. Or maybe not.
	
	
                log_info("File resolve gave %d\n", rv);
                log_debug("File resolve gave file=%p\n", dirent);

                switch (type) {
                case FS_OPEN_DR:
                case FS_OPEN_RD:
                        if (dirent == NULL) {
                                rv = CBM_ERROR_FILE_NOT_FOUND;
                        } else {
                                rv = dirent->handler->open2(dirent, pars, type, &file);
                        }
                        break;
                case FS_OPEN_WR:
                        if (dirent != NULL) {
                                rv = CBM_ERROR_FILE_EXISTS;
                                break;
                        }
                        // fall-through
                case FS_OPEN_AP:
                case FS_OPEN_OW:
                case FS_OPEN_RW:
                        if (dirent == NULL) {
                                if (pars->filetype == FS_DIR_TYPE_REL && pars->recordlen == 0) {
                                        rv = CBM_ERROR_FILE_NOT_FOUND;
                                        break;
                                }
                                rv = dir->handler->create(dir, &file, inname, cset, pars, type);
                                if (rv != CBM_ERROR_OK) {
                                        break;
                                }
                        } else {
                                rv = dirent->handler->open2(dirent, pars, type, &file);
				if (rv != CBM_ERROR_OK) {
					break;
				}
                        }
                        if (pars->filetype != FS_DIR_TYPE_UNKNOWN
                                && file->type != pars->filetype) {
                                rv = CBM_ERROR_FILE_TYPE_MISMATCH;
                                break;
                        }
                        if (!file->writable) {
                                rv = CBM_ERROR_WRITE_PROTECT;
                                break;
                        }
                        break;
                case FS_MKDIR:
                        if (dirent != NULL) {
                                rv = CBM_ERROR_FILE_EXISTS;
                        } else  {
                                if (dir->handler->mkdir != NULL) {
                                        rv = dir->handler->mkdir(dir, inname, cset, pars);
                                } else {
                                        rv = CBM_ERROR_DIR_NOT_SUPPORTED;
                                }
                        }
                        break;
                }

                if (rv == CBM_ERROR_OK) {
                        if (pars->recordlen != 0) {
                                if (file->recordlen != pars->recordlen
                                        || file->type != FS_DIR_TYPE_REL) {
                                        rv = CBM_ERROR_RECORD_NOT_PRESENT;
                                }
                        }
                }

                if (file != NULL) {
                        if (type == FS_OPEN_AP) {
                                rv = file->handler->seek(file, 0, SEEKFLAG_END);
                        }

                        *outfile = file;
                }
		if (dirent) {
			dirent->handler->declose(dirent);
			dirent = NULL;
		}
        }

        log_info("File open gave %d\n", rv);

        if (rv != CBM_ERROR_OK && rv != CBM_ERROR_OPEN_REL) {
                // on error
                if (file != NULL) {
                        file->handler->fclose(file, NULL, NULL);
                }
		if (outfile) {
                	*outfile = NULL;
		}
        }

        return rv;
}


