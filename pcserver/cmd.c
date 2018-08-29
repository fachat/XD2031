/****************************************************************************

    Commodore filesystem server
    Copyright (C) 2012,2018 Andre Fachat, Nils Eilers

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
 * In this file the actual command work is done
 */

#include "os.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "charconvert.h"
#include "wireformat.h"
#include "list.h"
#include "cmd.h"
#include "charconvert.h"
#include "petscii.h"
#include "provider.h"
#include "dir.h"
#include "handler.h"
#include "log.h"
#include "xcmd.h"
#include "channel.h"
#include "serial.h"
#include "handler.h"
#include "provider.h"
#include "resolver.h"
#include "types.h"

#define	MAX_BUFFER_SIZE			64


//------------------------------------------------------------------------------------
//


void cmd_init() {
	handler_init();
	provider_init();
	channel_init();
	xcmd_init();

	// init P00/S00/R00/... file handler
	x00_handler_init();
	// init ",P"/",R123"/ ... file handler
	typed_handler_init();

	// default
	//provider_set_ext_charset("PETSCII");
}

void cmd_free() {

	xcmd_free();
	handler_free();
	provider_free();
}


//------------------------------------------------------------------------------------

int cmd_assign_cmdline(const char *inname, charset_t cset) {

	int drive = -1;
	const char *provider_name = NULL;
	char *provider_parameter = NULL;
		
	char *c = strchr(inname, ':');
	if (c == NULL) {
		return CBM_ERROR_FAULT;
	}
	// zero-terminate
	*c = 0;
	c++;
	
	drive = strtol(inname, NULL, 10);
	
	provider_name = c;

	char *e = strchr(c, '=');
	if (e == NULL) {
		return CBM_ERROR_FAULT;
	}
	
	*e = 0;
	e++;

	provider_parameter = e;

	return provider_assign(drive, provider_name, provider_parameter, cset, true);
	
}

int cmd_assign_packet(const char *inname, int inlen, charset_t cset) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	openpars_t pars;
	int num_files = 2;
	drive_and_name_t names[2];

	rv = parse_filename_packet((uint8_t*) inname, inlen, &pars, names, &num_files);

	if (rv == CBM_ERROR_OK) {

	    if (num_files == 2) {

		log_debug("Assigning from server: %d:%s=%s\n", names[0].drive, names[0].name, names[1].name);

		int drive = names[0].drive;
		const char *provider_name = names[0].name;
		char *provider_parameter = names[1].name;
		
		// check trailing '/' on provider parameter
		int l = strlen(provider_parameter);
		if (l > 0 && provider_parameter[l-1] == '/') {
			provider_parameter[l-1] = 0;
		}

		log_debug("cmdline_assign '%s' = '%s'\n", provider_name, provider_parameter);
		rv = provider_assign(drive, provider_name, 
				provider_parameter, cset, false);
	    } else {
		log_error("Illegal number of parameters (%d)\n", num_files);
		rv = CBM_ERROR_FAULT;
	    }
	}
	return rv;
}


const char *get_options(const char *name, int len) {
	int l = strlen(name);
	if ((l + 1) < len) {
		return name + l + 1;
	}
	return NULL;
}

// ----------------------------------------------------------------------------------

int cmd_open_file(int tfd, const char *inname, int namelen, charset_t cset, char *outbuf, int *outlen, int cmd) {

	log_info("Open file for drive: %d, path='%s'\n", 0xff & *inname, inname+1);

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	file_t *fp = NULL;
	file_t *dir = NULL;
	openpars_t pars;
	int num_files = MAX_NAMEINFO_FILES+1;
	drive_and_name_t names[MAX_NAMEINFO_FILES+1];
	int outln = 0;

	rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	if (rv == CBM_ERROR_OK) {
	    // TODO: default endpoint? 
	    endpoint_t *ep = NULL;
	    // note: may modify names.trg.name in-place
	    rv = resolve_endpoint(&names[0], cset, &ep);
	    if (rv == CBM_ERROR_OK) {
		dir = ep->ptype->root(ep);
		rv = resolve_dir((const char**)&names[0].name, cset, &dir);
		if (rv == CBM_ERROR_OK) {
			// now resolve the actual filename
			rv = resolve_open(dir, (const char*)names[0].name, cset, &pars, cmd, &fp);
			if (rv == CBM_ERROR_OK || rv == CBM_ERROR_OPEN_REL) {
				// ok, we have the directory entry
				if (fp->recordlen > 0) {
					int record = fp->recordlen;
					outbuf[0] = record & 0xff;
					outbuf[1] = (record >> 8) & 0xff;
					outln = 2;
					rv = CBM_ERROR_OPEN_REL;
				}
				fp->openmode = cmd;
				channel_set(tfd, fp);
			}
		}
	    }
	}
	if (rv != CBM_ERROR_OK && rv != CBM_ERROR_OPEN_REL) {
		log_rv(rv);
		if (dir) {
			dir->handler->close(dir, 1, NULL, NULL);
		}
	}

	*outlen = outln;
	return rv;
#if 0	
	int rv = CBM_ERROR_DRIVE_NOT_READY;
	const char *name = NULL;
	file_t *fp = NULL;
	int outln = 0;

	endpoint_t *ep = provider_lookup(inname, namelen, cset, &name, NAMEINFO_UNDEF_DRIVE);
	if (ep != NULL) {
		provider_t *prov = (provider_t*) ep->ptype;
		//provider_convto(prov)(name, convlen, name, convlen);
		const char *options = get_options(inname + 1, namelen - 1);
		log_info("OPEN %d (%d->%s:%s,%s)\n", cmd, tfd,
			prov->name, name, options);
		rv = handler_resolve_file(ep, &fp, name, cset, options, cmd);
		if ((rv == CBM_ERROR_OK || rv == CBM_ERROR_OPEN_REL) && fp->recordlen > 0) {
			int record = fp->recordlen;
			outbuf[0] = record & 0xff;
			outbuf[1] = (record >> 8) & 0xff;
			outln = 2;
			rv = CBM_ERROR_OPEN_REL;
		}
		if (rv == CBM_ERROR_OK || rv == CBM_ERROR_OPEN_REL) {
			fp->openmode = cmd;
			channel_set(tfd, fp);
		} else {
			if (fp != NULL) {
				fp->handler->close(fp, 1, outbuf, &outln);
				fp = NULL;
			}
			log_rv(rv);
		}
		// cleanup when not needed anymore
		provider_cleanup(ep);
		mem_free(name);
	}
	*outlen = outln;
	return rv;
#endif
}

// ----------------------------------------------------------------------------------

int cmd_info(char *outbuf, int *outlen, charset_t outcset) {
	
	int rv = CBM_ERROR_OK;

	const char *info = "XDrive2031 filesystem server (C) A. Fachat et. al.";
	
	*outlen = cconv_converter(CHARSET_ASCII, outcset) (info, strlen(info), outbuf, strlen(info));
	
	return rv;
}

// ----------------------------------------------------------------------------------

int cmd_read(int tfd, char *outbuf, int *outlen, int *readflag, charset_t outcset) {
	
	int rv = CBM_ERROR_FILE_NOT_OPEN;

	file_t *fp = channel_to_file(tfd);
	if (fp != NULL) {
		direntry_t *direntry;
		*readflag = 0;	// default just in case
		if (fp->openmode == FS_OPEN_DR) {
			rv = resolve_scan(fp, fp->searchpattern, fp->numpattern, outcset, true, &direntry, readflag);
			if (!rv) {
				rv = dir_fill_entry_from_direntry(outbuf, outcset, fp->searchdrive, direntry, 
						MAX_BUFFER_SIZE-FSP_DATA);
			}
		} else {
		    	rv = fp->handler->readfile(fp, outbuf, MAX_BUFFER_SIZE-FSP_DATA, readflag, outcset);
		}
		// TODO: handle error (rv<0)
		if (rv < 0) {
			// an error is sent as REPLY with error code
			rv = -rv;
			log_rv(rv);
		} else {
			*outlen = rv;
			rv = CBM_ERROR_OK;
		}
	}
	return rv;
}

// ----------------------------------------------------------------------------------

int cmd_write(int tfd, int cmd, const char *indata, int datalen) {

	int rv = CBM_ERROR_FILE_NOT_OPEN;

	file_t *fp = channel_to_file(tfd);
	//printf("WRITE: chan=%d, ep=%p\n", tfd, ep);
	if (fp != NULL) {
		bool_t has_eof = (cmd == FS_WRITE_EOF);
		if (has_eof) {
			log_info("WRITE_WITH_EOF(%d)\n", tfd);
		}
		rv = fp->handler->writefile(fp, indata, datalen, has_eof);
		if (rv < 0) {
			// if negative, then it's an error
			rv = -rv;
			log_rv(rv);
		} else {
			// returns the number of bytes written when positive
			rv = CBM_ERROR_OK;
		}
	}
	return rv;
}

// ----------------------------------------------------------------------------------

int cmd_position(int tfd, const char *indata, int datalen) {

	int rv = CBM_ERROR_FILE_NOT_OPEN;

	if (datalen < 2) {
		rv = CBM_ERROR_FAULT;
	} else {
		// position the read/write cursor into a file
		file_t *fp = channel_to_file(tfd);
		if (fp != NULL) {
			int record = (indata[0] & 0xff) | ((indata[1] & 0xff) << 8);
			log_debug("POSITION: chan=%d, record=%d\n", tfd, record);
			rv = fp->handler->seek(fp, record * fp->recordlen, SEEKFLAG_ABS);
			if (rv != 0) {
				log_rv(rv);
			}
		}
	}
	return rv;
}


// ----------------------------------------------------------------------------------

int cmd_close(int tfd, char *outbuf, int *outlen) {

	int rv = CBM_ERROR_FILE_NOT_OPEN;
	
	*outlen = 2;

	file_t *fp = channel_to_file(tfd);
	if (fp != NULL) {
		log_info("CLOSE(%d)\n", tfd);
		rv = fp->handler->close(fp, 1, outbuf, outlen);
		channel_free(tfd);
	}
	return rv;
}

// ----------------------------------------------------------------------------------

int cmd_open_dir(int tfd, const char *inname, int namelen, charset_t cset) {


	int rv = CBM_ERROR_DRIVE_NOT_READY;
	file_t *fp = NULL;
	openpars_t pars;
	int num_files = MAX_NAMEINFO_FILES+1;
	drive_and_name_t names[MAX_NAMEINFO_FILES+1];
	int driveno = -1;

        rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	// determine driveno, and ensure all patterns use the same drive
	// (this is a current limitation compared to CBM DOS, where multiple pattern
	// in a single LOAD could utilize patterns of multiple drives)
	driveno = names[0].drive;
	for (int i = 1; i < num_files; i++) {
		if (names[i].drive == NAMEINFO_UNUSED_DRIVE
			|| names[i].drive == NAMEINFO_LAST_DRIVE
			|| names[i].drive == driveno) {
			// ok, drive reused (numeric)
			continue;
		}
		if (names[i].drive == NAMEINFO_UNDEF_DRIVE
			&& names[0].drive == NAMEINFO_UNDEF_DRIVE
			&& strcmp(names[0].drivename, names[i].drivename)) {
			// ok, drive reused (named)
			continue;
		}
		// here drives [i] and [0] do not match
		return CBM_ERROR_SYNTAX_PATTERN;
	}

	log_info("Open directory for drive: %d(%s), path='%s'\n", names[0].drive, names[0].drivename, names[0].name);

        if (rv == CBM_ERROR_OK) {
            // TODO: default endpoint? 
            endpoint_t *ep = NULL;
	    // note: may modify names.trg.name in-place
            rv = resolve_endpoint(&names[0], cset, &ep);
	
	    if (rv == 0) {
		fp = ep->ptype->root(ep);
		rv = resolve_dir((const char**)&names[0].name, cset, &fp);
		if (rv == 0) {
			fp->searchdrive = driveno;
			for (int i = 0; i < num_files; i++) {
				fp->searchpattern[i] = mem_alloc_str(names[i].name);
			}
			fp->numpattern = num_files;
			fp->openmode = FS_OPEN_DR;
			channel_set(tfd, fp);
		} else {
			log_rv(rv);
		}
	    }
	}
	if (rv != CBM_ERROR_OK) {
		log_rv(rv);
	}

	return rv;
}

// ----------------------------------------------------------------------------------

static int delete_name(drive_and_name_t *name, charset_t cset, endpoint_t **epp, int isrmdir, int *outdeleted) {

	int rv = resolve_endpoint(name, cset, epp);
	endpoint_t *ep = *epp;
	file_t *dir = NULL;

	if (rv == CBM_ERROR_OK) {
		dir = ep->ptype->root(ep);
		rv = resolve_dir((const char**)&name->name, cset, &dir);
		while (rv == CBM_ERROR_OK) {
			// now resolve the actual filenames
			const char *pattern = (const char*) name->name;
			direntry_t *dirent = NULL;
			rv = resolve_scan(dir, &pattern, 1, cset, false, 
					&dirent, NULL);
			if (dirent) {
				log_info("DELETE(%s / %s)\n", dir->filename, dirent->name);
				if (isrmdir) {
					// do a "RMDIR"
					if (dirent->handler->rmdir2) {
						dirent->handler->rmdir2(dirent);
					} else {
						log_error("Unable to RMDIR %s\n", dirent->name);
					}
				} else {
					// do a "DELETE"
					if (dirent->handler->scratch2) {
						dirent->handler->scratch2(dirent);
					} else {
						log_error("Unable to DELETE %s\n", dirent->name);
					}
				}
				(*outdeleted)++;
			}
		}
		if (rv == CBM_ERROR_FILE_NOT_FOUND) {
			// no match is ok
			rv = CBM_ERROR_OK;
		}
	}
	if (dir) {
		dir->handler->close(dir, 1, NULL, NULL);
	}
	return rv;
}

int cmd_delete(const char *inname, int namelen, charset_t cset, char *outbuf, int *outlen, int isrmdir) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	int outdeleted = 0;

	openpars_t pars;
	int num_files = MAX_NAMEINFO_FILES+1;
	drive_and_name_t names[MAX_NAMEINFO_FILES+1];

	rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	if (rv == CBM_ERROR_OK) {
	    	// TODO: default endpoint? 
	    	endpoint_t *ep = NULL;

	    	int i = 0;
	    	while (rv == CBM_ERROR_OK && i < num_files) {
	    		// note: may modify names.trg.name in-place
			// ep will be carried over from previous invocations
			rv = delete_name(&names[i], cset, &ep, isrmdir, &outdeleted);
			i++;
	    	}
	}
	if (rv == CBM_ERROR_OK) {
		outbuf[0] = outdeleted > 99 ? 99 : outdeleted;
		*outlen = 1;
		rv = CBM_ERROR_SCRATCHED;
	}
	return rv;
}

// ----------------------------------------------------------------------------------

static int mkdir_name(drive_and_name_t *name, charset_t cset, openpars_t *pars, endpoint_t **epp) {

	int rv = resolve_endpoint(name, cset, epp);
	endpoint_t *ep = *epp;
	file_t *dir = NULL;

	if (rv == CBM_ERROR_OK) {
		dir = ep->ptype->root(ep);
		rv = resolve_dir((const char**)&name->name, cset, &dir);
		if (rv == CBM_ERROR_OK) {
			rv = resolve_open(dir, (const char*)name->name, cset, pars, FS_MKDIR, NULL);
		}
	}
	if (dir) {
		dir->handler->close(dir, 1, NULL, NULL);
	}
	return rv;
}

int cmd_mkdir(const char *inname, int namelen, charset_t cset) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	int outdeleted = 0;

	openpars_t pars;
	int num_files = MAX_NAMEINFO_FILES+1;
	drive_and_name_t names[MAX_NAMEINFO_FILES+1];

	rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	if (rv == CBM_ERROR_OK) {
	    	// TODO: default endpoint? 
	    	endpoint_t *ep = NULL;

	    	int i = 0;
	    	while (rv == CBM_ERROR_OK && i < num_files) {
	    		// note: may modify names.trg.name in-place
			// ep will be carried over from previous invocations
			rv = mkdir_name(&names[i], cset, &pars, &ep);
			i++;
	    	}
	}
	return rv;
#if 0
	int rv = CBM_ERROR_DRIVE_NOT_READY;
	file_t *newdir = NULL;
	const char *name = NULL;

	(void) namelen;	// silence unused warning

	endpoint_t *ep = provider_lookup(inname, namelen, cset, &name, NAMEINFO_UNDEF_DRIVE);
	if (ep != NULL) {
		log_info("MKDIR(%s)\n", name);
		rv = handler_resolve_file(ep, &newdir, name, cset, NULL, FS_MKDIR);

		provider_cleanup(ep);
		mem_free(name);
	}
	return rv;
#endif
}

// ----------------------------------------------------------------------------------

int cmd_chdir(const char *inname, int namelen, charset_t cset) {

	int rv = CBM_ERROR_FAULT;

	log_info("CHDIR(%s)\n", inname);

	rv = provider_chdir(inname, namelen, cset);

	return rv;
}

// ----------------------------------------------------------------------------------

int cmd_move(const char *inname, int namelen, charset_t cset) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	direntry_t *dirent = NULL;
	file_t *srcdir = NULL;
	file_t *trgdir = NULL;
	endpoint_t *srcep = NULL;
	endpoint_t *trgep = NULL;
	openpars_t pars;
	int num_files = MAX_NAMEINFO_FILES+1;
	drive_and_name_t names[MAX_NAMEINFO_FILES+1];
	int outln = 0;

	rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	if (num_files != 2) {
		return CBM_ERROR_SYNTAX_UNKNOWN;
	}

	if (rv == CBM_ERROR_OK) {

	    // TODO: default endpoint? 
	    // note: may modify names.trg.name in-place
	    rv = resolve_endpoint(&names[0], cset, &trgep);
	    if (rv == CBM_ERROR_OK) {

		srcep = trgep;	// default
		rv = resolve_endpoint(&names[1], cset, &srcep);
		if (rv == CBM_ERROR_OK) {
    	
		    if (srcep == trgep) {

			// find the source file
			srcdir = srcep->ptype->root(srcep);
			rv = resolve_dir((const char**)&names[1].name, cset, &srcdir);

			if (rv == CBM_ERROR_OK) {
			    // now resolve the actual source filename into dirent
			    rv = resolve_scan(srcdir, (const char**)&names[1].name, 1, cset, 
					    false, &dirent, NULL);
			    if (rv == CBM_ERROR_OK && dirent) {
		
				// find the target directory	
				trgdir = trgep->ptype->root(trgep);
				rv = resolve_dir((const char**)&names[0].name, cset, &trgdir);

				if (rv == CBM_ERROR_OK) {

				    if (dirent->handler->move2) {
					rv = dirent->handler->move2(dirent, trgdir, names[0].name, cset);
				    } else {
					rv = CBM_ERROR_FAULT;
				    }
				    trgdir->handler->close(trgdir, 0, NULL, NULL);
				}
				//fp->handler->close(fp, 0, NULL, NULL);
				//dirent->handler->declose(dirent);
			    }
			    srcdir->handler->close(srcdir, 0, NULL, NULL);
			}
		    } else {
       		    	rv = CBM_ERROR_DRIVE_NOT_READY;
		    	provider_cleanup(trgep);
		    }
		    provider_cleanup(srcep);
		}
	    }
	}
	return rv;

#if 0
	int err = CBM_ERROR_DRIVE_NOT_READY;

	int todrive = inname[0];
	const char *fromname = NULL;
	const char *toname = NULL;
	endpoint_t *epto = provider_lookup(inname, namelen, cset, &toname, NAMEINFO_UNDEF_DRIVE);
	if (epto != NULL) {
		const char *name2 = strchr(inname+1, 0);	// points to null byte after name
		name2++;					// first byte of second name
		endpoint_t *epfrom = provider_lookup(name2, namelen, cset, &fromname, todrive);

		if (epfrom != NULL) {
			file_t *fromfile = NULL;
			
			err = handler_resolve_file(epfrom, &fromfile, fromname, cset, NULL, FS_MOVE);
			
			if (err == CBM_ERROR_OK) {
				file_t *todir = NULL;
				const char *topattern = NULL;
	
				err = handler_resolve_dir(epto, &todir, toname, cset, &topattern, NULL);

				if (err == CBM_ERROR_OK && todir != NULL) {

					// TODO check if that is really working (with those temp provs...)
					if (fromfile->endpoint == todir->endpoint) {
						// we can just forward it to the provider proper

						if (fromfile->handler->move != NULL) {
							err = fromfile->handler->move(fromfile, todir, topattern, cset);
						} else {
							// e.g. x00 does not support it now
							log_warn("File type spec not supported\n");
							err = CBM_ERROR_DRIVE_NOT_READY;
						}	
					} else {
						// TODO some kind of copy/remove stuff...
						log_warn("Drive spec combination not supported\n");
						err = CBM_ERROR_DRIVE_NOT_READY;
					}
					todir->handler->close(todir, 1, NULL, NULL);
				}
				fromfile->handler->close(fromfile, 1, NULL, NULL);
			}
			provider_cleanup(epfrom);
		}
		provider_cleanup(epto);
	}
	return err;
#endif
}

// ----------------------------------------------------------------------------------

static int copy_file(file_t *tofile, drive_and_name_t *name, charset_t cset) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	file_t *srcdir = NULL;
	file_t *fromfile = NULL;
	endpoint_t *srcep = NULL;
	openpars_t pars;

	openpars_init_options(&pars);

	// TODO: default endpoint? 
	// note: may modify names.trg.name in-place
	rv = resolve_endpoint(name, cset, &srcep);
	if (rv == CBM_ERROR_OK) {

	    // find the target directory	
	    srcdir = srcep->ptype->root(srcep);
	    rv = resolve_dir(&name->name, cset, &srcdir);

	    if (rv == CBM_ERROR_OK) {
			
		rv = resolve_open(srcdir, name->name, cset, &pars, FS_OPEN_RD, &fromfile);

		if (rv == CBM_ERROR_OK) {
			// read file is open, can do the copy
			char buffer[8192];
			int readflag = 0;
			int rlen = 0;
			int wlen = 0;
			int nwritten = 0;

			do {
				rlen = fromfile->handler->readfile(fromfile, 
					buffer, 8192, &readflag, cset);
				if (rlen < 0) {
					rv = -rlen;
					break;
				}
				nwritten = 0;
				while (rlen > nwritten) {
					wlen = tofile->handler->writefile(tofile,
						buffer + nwritten, rlen - nwritten, 
						readflag & READFLAG_EOF);
					if (wlen < 0) {
						rv = -wlen;
						break;
					}
					nwritten += wlen;
				}
			} while ((rv == CBM_ERROR_OK) 
				&& ((readflag & READFLAG_EOF) == 0));	

			fromfile->handler->close(fromfile, 1, NULL, NULL);
		}
		srcdir->handler->close(srcdir, 0, NULL, NULL);
	    }
	    provider_cleanup(srcep);
	}
	return rv;
}

int cmd_copy(const char *inname, int namelen, charset_t cset) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	file_t *trgdir = NULL;
	file_t *fp = NULL;
	endpoint_t *trgep = NULL;
	openpars_t pars;
	int num_files = MAX_NAMEINFO_FILES+1;
	drive_and_name_t names[MAX_NAMEINFO_FILES+1];
	int outln = 0;

	rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	if (num_files < 2) {
		return CBM_ERROR_SYNTAX_UNKNOWN;
	}

	if (rv == CBM_ERROR_OK) {

	    // TODO: default endpoint? 
	    // note: may modify names.trg.name in-place
	    rv = resolve_endpoint(&names[0], cset, &trgep);
	    if (rv == CBM_ERROR_OK) {

		// find the target directory	
		trgdir = trgep->ptype->root(trgep);
		rv = resolve_dir((const char**)&names[0].name, cset, &trgdir);

		if (rv == CBM_ERROR_OK) {
			
		    rv = resolve_open(trgdir, names[0].name, cset, &pars, FS_OPEN_WR, &fp);

		    if (rv == CBM_ERROR_OK) {

		    	for (int i = 1; rv == CBM_ERROR_OK && i < num_files; i++) {
			    if (names[i].drive == NAMEINFO_UNUSED_DRIVE 
				|| names[i].drive == NAMEINFO_LAST_DRIVE) {
				names[i].drive = names[i-1].drive;
			    }
			    rv = copy_file(fp, &names[i], cset);
			}

			fp->handler->close(fp, 0, NULL, NULL);
		    }
		}
		trgdir->handler->close(trgdir, 0, NULL, NULL);

		provider_cleanup(trgep);
	    }
	}
	
	return rv;
}

#if 0
	int err = CBM_ERROR_DRIVE_NOT_READY;

	int todrive = inname[0];
	const char *fromname = NULL;
	const char *toname = NULL;
	const char *p = inname+1;
	endpoint_t *epto = provider_lookup(inname, namelen, cset, &toname, NAMEINFO_UNDEF_DRIVE);
	if (epto != NULL) {
		file_t *tofile = NULL;
		err = handler_resolve_file(epto, &tofile, toname, cset, NULL, FS_OPEN_WR);
		if (err == CBM_ERROR_OK) {
			// file is opened for writing...
			// now iterate over the source files

			file_t *fromfile = NULL;

			// find next name
			fromname = strchr(p, 0) + 1;	// behind terminating zero-byte
			int thislen = namelen - (fromname - inname);
			while ((err == CBM_ERROR_OK) && (thislen > 0)) {
				p = fromname + 1;
				endpoint_t *fromep = provider_lookup(fromname, thislen, cset, &fromname, todrive);
				if (fromep != NULL) {
					err = handler_resolve_file(fromep, &fromfile, fromname, cset,
										NULL, FS_OPEN_RD);
					if (err == CBM_ERROR_OK) {
						// read file is open, can do the copy
						char buffer[8192];
						int readflag = 0;
						int rlen = 0;
						int wlen = 0;
						int nwritten = 0;

						do {
							rlen = fromfile->handler->readfile(fromfile, 
									buffer, 8192, &readflag, cset);
							if (rlen < 0) {
								err = -rlen;
								break;
							}
							nwritten = 0;
							while (rlen > nwritten) {
								wlen = tofile->handler->writefile(tofile,
									buffer + nwritten, rlen - nwritten, 
									readflag & READFLAG_EOF);
								if (wlen < 0) {
									err = -wlen;
									break;
								}
								nwritten += wlen;
							}
						} while ((err == CBM_ERROR_OK) 
								&& ((readflag & READFLAG_EOF) == 0));	
						
						fromfile->handler->close(fromfile, 1, NULL, NULL);
					}
					provider_cleanup(fromep);
				}
				fromname = strchr(p, 0) + 1;	// behind terminating zero-byte
				thislen = namelen - (fromname - inname);
			}				
			tofile->handler->close(tofile, 1, NULL, NULL);
		}
		provider_cleanup(epto);
	}
	return err;
}
#endif

// ----------------------------------------------------------------------------------

int cmd_block(int tfd, const char *indata, const int datalen, char *outdata, int *outlen) {

	(void)datalen; // silence warning unused parameter

	drive_and_name_t name;
	endpoint_t *ep = NULL;

	int rv = CBM_ERROR_DRIVE_NOT_READY;

	nameinfo_init(&name);

	// not file-related, so no file descriptor (tfd)
	// we only support mapped drives (thus name is NULL)
	// we only interpret the drive, so namelen for the lookup is 1

	name.drive = *indata;
	rv = resolve_endpoint(&name, CHARSET_ASCII, &ep);
	//endpoint_t *ep = provider_lookup(indata, 1, 0, NULL, NAMEINFO_UNDEF_DRIVE);
	
	if (ep != NULL) {
		provider_t *prov = (provider_t*) ep->ptype;
		if (prov->block != NULL) {
			log_info("DIRECT(%d,...)\n", tfd);
			rv = prov->block(ep, indata + 1, outdata, outlen);
			if (rv != 0) {
				log_rv(rv);
			}
			log_debug("block: outlen=%d, outdata=%02x %02x %02x %02x\n",
					*outlen, outdata[0], outdata[1], outdata[2], outdata[3]);
		}
		// cleanup when not needed anymore
		provider_cleanup(ep);
	}
	return rv;
}

// ----------------------------------------------------------------------------------

int cmd_format(const char *inname, int namelen, charset_t cset) {
	
	int rv = CBM_ERROR_DRIVE_NOT_READY;

	openpars_t pars;
	int num_files = 1;
	drive_and_name_t names[1];
	endpoint_t *ep = NULL;

	rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	if (rv != CBM_ERROR_OK) {
		return rv;
	}

	if (num_files != 1) {
		return CBM_ERROR_SYNTAX_PATTERN;
	}

	rv = resolve_endpoint(names, cset, &ep);

	if (rv == CBM_ERROR_OK && ep != NULL) {
		provider_t *prov = (provider_t*) ep->ptype;
		if (prov->format != NULL) {
			rv = prov->format(ep, names[0].name);
		}
	}
	return rv;
}

	

