
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


#include <stdbool.h>

#include "mem.h"
#include "log.h"
#include "errors.h"
#include "charconvert.h"
#include "provider.h"
#include "resolver.h"
#include "wireformat.h"
#include "endpoints.h"



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
 */
int resolve_endpoint(const char **pattern, charset_t cset, endpoint_t **outep) {

	const char *inname = *pattern;

        int drive = inname[0];
        inname++;

        if (drive == NAMEINFO_LAST_DRIVE) {
		// this means there was no drive or provider defined; we can just return
		// as ep has been set to the default value by the caller.
		return CBM_ERROR_OK;
        }

        if (drive == NAMEINFO_UNDEF_DRIVE) {
                if (inname[0] == 0) {
                        // no name specified, so return NULL (no provider found)
                        return CBM_ERROR_OK;
                }
                // the drive is not specified by number, but by provider name
		// TODO: charset-aware strchr, only works for provider with ASCII names right now
                char *p = strchr(inname, ':');
                if (p != NULL) {
			// only ASCII names work
                        unsigned int l = p-(inname);
                        p++; // first char after ':'
			const char *pname = mem_alloc_strn(inname, l);
                        log_debug("Trying to find provider for: %s\n", inname);

			provider_t *prov = provider_find(pname);

                        if (prov != NULL) {
                        // we got a provider, but no endpoint yet

	                        log_debug("Found provider '%s', trying to create temporary endpoint for '%s'\n",
                                               prov->name, p);

                                if (prov->tempep != NULL) {
                                	endpoint_t *ep = prov->tempep(&p, cset);
                                        if (ep != NULL) {
						*pattern = p;
                                                log_debug("Created temporary endpoint %p\n", ep);
                                                ep->is_temporary = 1;
                                        }
                                        mem_free(pname);
					*pattern = p;
					*outep = ep;
					return CBM_ERROR_OK;
                                } else {
                                        log_error("Provider '%s' does not support temporary drives\n",
                                                  prov->name);
                                }
                                mem_free(pname);
                                return CBM_ERROR_DRIVE_NOT_READY;
                        }
                        mem_free(pname);
                        log_error("Did not find provider for %s\n", inname);
                        return CBM_ERROR_DRIVE_NOT_READY;
                } else {
                        log_info("No provider name given for undef'd drive '%s', trying default %s\n",
                                                                        inname, (*outep)?(*outep)->ptype->name:"-");
			return CBM_ERROR_OK;
                }
        }

        log_debug("Trying to resolve drive %d with name '%s'\n", drive, inname);

	// NOTE: cdpath not yet used!
	ept_t *ept = endpoints_find(drive);

	if (ept != NULL) {
		*pattern = inname;
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

	if (!dir->handler->resolve2) {
		return CBM_ERROR_FAULT;
	}

	do {
		rv = dir->handler->resolve2(pattern, cset, &dir);

		if (rv == CBM_ERROR_SYNTAX_WILDCARDS) {

			direntry_t *de;
			const char *p = *pattern;
			int rdflag = 0;
			file_t *fp = NULL;

			rv = resolve_scan(dir, &p, cset, false, &de, &rdflag);

			if (rv == CBM_ERROR_OK) {
				// open the found dir entry
				rv = dir->handler->open2(de, NULL, FS_OPEN_DR, &fp);

			}
		}
	
	} while(rv == CBM_ERROR_OK);

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
 * rdflag 
 */
int resolve_scan(file_t *dir, const char **pattern, charset_t outcset, bool isdirscan,
	direntry_t **outde, int *rdflag) {

	int rv = CBM_ERROR_OK;

        const char *scanpattern = NULL;
        const char *name = NULL;
	direntry_t *direntry = NULL;

        do {
        	rv = dir->handler->direntry2(dir, &direntry, isdirscan, rdflag);

                name = (const char*)direntry->name;
                scanpattern = *pattern;
        } while (
                rv == CBM_ERROR_OK
                     && name
                     && !cconv_matcher(outcset, direntry->cset) (&scanpattern, &name, false )
        );

	*pattern = scanpattern;
	*outde = direntry;

	return rv;
}


