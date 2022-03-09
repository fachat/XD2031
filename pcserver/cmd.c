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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "provider.h"
#include "charconvert.h"
#include "dir.h"
#include "channel.h"
#include "xcmd.h"
#include "resolver.h"
#include "endpoints.h"

#define	MAX_BUFFER_SIZE			64

#undef	DEBUG_CMD


//------------------------------------------------------------------------------------

static int drive_scan_next(drive_and_name_t *dnt, charset_t cset, 
			chan_t *chan, int last_drv);


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

	int err = CBM_ERROR_OK;
	const int BUFLEN = 255;

	uint8_t *name = mem_alloc_c(BUFLEN+1, "assign name buffer");
	strncpy((char*) name, inname, BUFLEN);
	name[BUFLEN] = 0;

	nameinfo_t ninfo;
	nameinfo_init(&ninfo);

	parse_cmd_pars(name, strlen((char*)name), CMD_ASSIGN, &ninfo);
	if (ninfo.cmd != CMD_ASSIGN) {
		err = CBM_ERROR_SYNTAX_NONAME;
	} else
	if (ninfo.num_files != 1) {
                log_error("Wrong number of parameters!\n");
                err = CBM_ERROR_SYNTAX_NONAME;
	}

	if (err == CBM_ERROR_OK) {

		err = provider_assign(ninfo.trg.drive, &ninfo.file[0], cset, true);
	}

	mem_free(name);
	return err;
}

int cmd_assign_packet(const char *inname, int inlen, charset_t cset) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	openpars_t pars;
	int num_files = 2;
	drive_and_name_t names[2];

	rv = parse_filename_packet((uint8_t*) inname, inlen, &pars, names, &num_files);

	if (rv == CBM_ERROR_OK) {

	    if (num_files == 2) {


		int drive = names[0].drive;
		uint8_t *provider_name = names[1].drivename;
		uint8_t *provider_parameter = names[1].name;

		log_debug("Assigning from server: %d:=%s(%d):%s\n", drive, provider_name, names[1].drive, provider_parameter);
		
		// check trailing '/' on provider parameter
		int l = strlen((char*)provider_parameter);
		if (l > 0 && provider_parameter[l-1] == '/') {
			provider_parameter[l-1] = 0;
		}

		log_debug("cmdline_assign '%s' = '%s'\n", provider_name, provider_parameter);

		rv = provider_assign(drive, &names[1], cset, false);
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

int cmd_open_file(int tfd, const char *inname, int namelen, charset_t cset, drive_and_name_t *lastdrv, char *outbuf, int *outlen, int cmd) {

	
	int rv = CBM_ERROR_DRIVE_NOT_READY;
	file_t *fp = NULL;
	file_t *dir = NULL;
	openpars_t pars;
	int num_files = MAX_NAMEINFO_FILES+1;
	drive_and_name_t names[MAX_NAMEINFO_FILES+1];
	int outln = 0;

	rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	log_info("Open file for drive: %d(%s), path='%s' -> rv=%d\n", 0xff & names[0].drive, names[0].drivename, names[0].name, rv);

	if (rv == CBM_ERROR_OK) {
	    // TODO: default endpoint? 
	    endpoint_t *ep = NULL;
	    // note: may modify names.trg.name in-place
	    rv = resolve_endpoint(&names[0], cset, 0, &ep);
	    if (rv == CBM_ERROR_OK) {
		dir = ep->ptype->root(ep);
		rv = resolve_dir((const char**)&names[0].name, cset, &dir);
		if (rv == CBM_ERROR_OK) {
			// now resolve the actual filename
			rv = resolve_open(dir, names, cset, &pars, cmd, &fp);
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
			dir->handler->fclose(dir, NULL, NULL);
		}
	}

	*outlen = outln;
	return rv;
}

// ----------------------------------------------------------------------------------

int cmd_info(char *outbuf, int *outlen, charset_t outcset) {
	
	int rv = CBM_ERROR_OK;

	const char *info = "XDrive2031 filesystem server (C) A. Fachat et. al.";
	
	*outlen = cconv_converter(CHARSET_ASCII, outcset) (info, strlen(info), outbuf, strlen(info));
	
	return rv;
}

// ----------------------------------------------------------------------------------

int cmd_read(int tfd, char *outbuf, int *outlen, int *readflag, charset_t outcset, drive_and_name_t *lastdrv) {
	
	int rv = CBM_ERROR_FILE_NOT_OPEN;

	file_t *fp = NULL;
	chan_t *chan = channel_get(tfd);
	if (chan != NULL) {
		fp = chan->fp;
/*
		if (fp == NULL) {
			// start of new directory scan
	       	    	endpoint_t *ep = NULL;
	    		// note: may modify names.trg.name in-place
       	    		rv = resolve_endpoint(chan->searchpattern, outcset, 0, &ep);
	    		if (rv == 0) {
				file_t *fp = ep->ptype->root(ep);

				rv = resolve_dir((const char **)&chan->searchpattern->name, outcset, &fp);
				if (rv == 0) {
					fp->openmode = FS_OPEN_DR;
					chan->fp = fp;
				}
			}
		}
*/
	}

	if (fp != NULL) {
		*readflag = 0;	// default just in case
		if (fp->openmode == FS_OPEN_DR) {
			direntry_t *direntry;
			rv = resolve_scan(fp, chan->searchpattern, chan->num_pattern, outcset, true, &direntry, readflag);
			if (!rv) {
				rv = dir_fill_entry_from_direntry(outbuf, outcset, lastdrv->drive, direntry, 
						MAX_BUFFER_SIZE-FSP_DATA);
				direntry->handler->declose(direntry);
	
				if (READFLAG_EOF & *readflag) {
					// end of dir - do we need another scan?
					int rvx = drive_scan_next(chan->searchpattern, outcset, chan, lastdrv->drive);

					if (rvx == CBM_ERROR_OK) {
						*readflag &= ~READFLAG_EOF;
					}
				}

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
	} else {
		log_rv(rv);
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
#ifdef DEBUG_CMD
		if (has_eof) {
			log_info("WRITE_WITH_EOF(%d)\n", tfd);
		}
#endif
		if (fp->handler->writefile) {
			rv = fp->handler->writefile(fp, indata, datalen, has_eof);
			if (rv < 0) {
				// if negative, then it's an error
				rv = -rv;
				log_rv(rv);
			} else {
				// returns the number of bytes written when positive
				rv = CBM_ERROR_OK;
			}
		} else {
			rv = CBM_ERROR_FAULT;
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
		if (outlen) {
			*outlen = 0;
		}
		rv = fp->handler->fclose(fp, outbuf, outlen);
		channel_free(tfd);
	}
	return rv;
}

// ----------------------------------------------------------------------------------

static drive_and_name_t *alloc_fixup_dirpattern(drive_and_name_t *pattern, int num_files, 
		drive_and_name_t *lastdrv) {

	drive_and_name_t *dnt = NULL;

	if (num_files == 0) {
		return NULL;
	}

	size_t len = num_files * sizeof(drive_and_name_t);
	dnt = mem_alloc_c(len, "dir_pattern");

	for (int i = 0; i < num_files; i++) {
		
		drive_and_name_init(&dnt[i]);

		if (pattern[i].drive == NAMEINFO_LAST_DRIVE) {
			if (i == 0) {
				dnt[i].drive = lastdrv->drive;
				dnt[i].drivename = (uint8_t*) mem_alloc_str2((const char*) lastdrv->drivename, "drivename");
			} else {
				dnt[i].drive = dnt[i-1].drive;
				dnt[i].drivename = (uint8_t*) mem_alloc_str2((const char*) dnt[i-1].drivename, "drivename");
			}
		} else {
			dnt[i].drive = pattern[i].drive;
			dnt[i].drivename = (uint8_t*) mem_alloc_str2((const char*) pattern[i].drivename, "drivename");
		}
		dnt[i].name = (uint8_t*) mem_alloc_str2((const char*) pattern[i].name, "fname");

		dnt[i].drivename_m = (char*) dnt[i].drivename;
		dnt[i].name_m = (char*) dnt[i].name;
	}

	if (lastdrv->drivename) {
		mem_free(lastdrv->drivename);
		lastdrv->drivename = NULL;
	}
	return dnt;
}

/*
 * Identify and open the next drive directory in a directory listing.
 *
 * The algorithm of the CBM DOS is to first scan the drive used last, then the
 * second drive. The second drive then becomes the last drive, and is shown first
 * on the next directory. 
 * During a drive scan, all search patterns are then matched against drive and
 * pattern. 
 * 
 * So in a drive search pattern list (each directory command can have multiple pattern)
 * each pattern could apply to one drive, or both. A drive is only included in the
 * directory listing, if any one of the pattern applies to it.
 *
 * So here we first scan the last drive, then go through the list of assigned
 * drives (with the exception of the last drive). Last we go through the non-assigned
 * drives (like "ftp:"). 
 * 
 * In our system we have the complication that each pattern can potentially contain
 * a directory path before the actual pattern. This pattern will only be evaluated
 * for the first pattern that matches a drive. The other pattern then will apply
 * to this directory directly.
 */
static int drive_scan_next(drive_and_name_t *dnt, charset_t cset, 
			chan_t *chan, int last_drv) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	int idx = -1;
	int num_files = chan->num_pattern;

	while (rv == CBM_ERROR_DRIVE_NOT_READY) {
		if (chan->searchdrv < 0) {
			// initial drive
			chan->searchdrv = last_drv;
		} else 
		if (chan->searchdrv == last_drv) {
			// done with the last drive, start from 0 (or 1 if last_drv is 0)
			chan->searchdrv = last_drv ? 0 : 1;
		} else
		if (chan->searchdrv < 10+num_files) {
			// up to 10 numeric drives
			chan->searchdrv++;
			if (chan->searchdrv == last_drv) {
				// skip last drive, we did this first
				chan->searchdrv++;
			}
		}

		if (chan->searchdrv < 10) {
			// numeric drive
			ept_t *ep = endpoints_find(chan->searchdrv);
			if (ep != NULL) {
				// drive is assigned
				for (idx = 0; idx < num_files; idx++) {
					if (dnt[idx].drive == chan->searchdrv
						|| dnt[idx].drive == NAMEINFO_UNUSED_DRIVE) {
						// found a matching search pattern
						rv = CBM_ERROR_OK;
						break;
					}
				}
			}
		} else {
			// scan search pattern for named drive
			idx = chan->searchdrv - 10;
			if (idx >= num_files) {
				// went over the last drive
				return CBM_ERROR_DRIVE_NOT_READY;
			}
			if (dnt[idx].drive == NAMEINFO_UNDEF_DRIVE) {
				// found a named drive
				rv = CBM_ERROR_OK;
			}
		}
	}

	// idx contains a matching drive

       	endpoint_t *ep = NULL;
	int searchdrv=dnt[idx].drive;
	dnt[idx].drive = chan->searchdrv;
       	rv = resolve_endpoint(&dnt[idx], cset, 0, &ep);
	dnt[idx].drive= searchdrv;
	if (rv == CBM_ERROR_OK) {
		file_t *fp = ep->ptype->root(ep);

		rv = resolve_dir((const char **)&dnt[idx].name, cset, &fp);
		if (rv == CBM_ERROR_OK) {
			fp->openmode = FS_OPEN_DR;
			chan->fp = fp;
		}
	}
	if (rv != CBM_ERROR_OK) {
		log_rv(rv);
	}
	return rv;
}

int cmd_open_dir(int tfd, const char *inname, int namelen, charset_t cset, drive_and_name_t *lastdrv) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	openpars_t pars;
	int num_files = MAX_NAMEINFO_FILES+1;
	drive_and_name_t names[MAX_NAMEINFO_FILES+1];
	drive_and_name_t *dnt = NULL;

        rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	if (rv == 0) {
	    	dnt = alloc_fixup_dirpattern(names, num_files, lastdrv);
	
	    	log_info("Open directory for drive: %d (%s), path='%s'\n", names[0].drive, names[0].drivename, names[0].name);

	    	channel_set(tfd, NULL);
		chan_t *chan = channel_get(tfd);

		chan->searchpattern = dnt;
		chan->num_pattern = num_files;
		chan->searchdrv = -1;

		rv = drive_scan_next(dnt, cset, chan, lastdrv->drive);
	}
	if (rv != CBM_ERROR_OK) {
		log_rv(rv);
	}

	return rv;
}

// ----------------------------------------------------------------------------------

static int delete_name(drive_and_name_t *name, charset_t cset, endpoint_t **epp, int isrmdir, int *outdeleted) {

	int rv = resolve_endpoint(name, cset, 0, epp);
	endpoint_t *ep = *epp;
	file_t *dir = NULL;

	if (rv == CBM_ERROR_OK) {
		dir = ep->ptype->root(ep);
		rv = resolve_dir((const char**)&name->name, cset, &dir);
		while (rv == CBM_ERROR_OK) {
			// now resolve the actual filenames
			direntry_t *dirent = NULL;
			rv = resolve_scan(dir, name, 1, cset, false, 
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
				dirent->handler->declose(dirent);
			}
		}
		if (rv == CBM_ERROR_FILE_NOT_FOUND) {
			// no match is ok
			rv = CBM_ERROR_OK;
		}
	}
	if (dir) {
		dir->handler->fclose(dir, NULL, NULL);
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

	int rv = resolve_endpoint(name, cset, 0, epp);
	endpoint_t *ep = *epp;
	file_t *dir = NULL;

	if (rv == CBM_ERROR_OK) {
		dir = ep->ptype->root(ep);
		rv = resolve_dir((const char**)&name->name, cset, &dir);
		if (rv == CBM_ERROR_OK) {
			rv = resolve_open(dir, name, cset, pars, FS_MKDIR, NULL);
		}
	}
	if (dir) {
		dir->handler->fclose(dir, NULL, NULL);
	}
	return rv;
}

int cmd_mkdir(const char *inname, int namelen, charset_t cset) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;

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
}

// ----------------------------------------------------------------------------------

int cmd_chdir(const char *inname, int namelen, charset_t cset) {

	(void) namelen;
	(void) cset;

	int rv = CBM_ERROR_FAULT;

	log_info("CHDIR(%s)\n", inname);

	//rv = provider_chdir(inname, namelen, cset);

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

	rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	if (num_files != 2) {
		return CBM_ERROR_SYNTAX_UNKNOWN;
	}

	if (rv == CBM_ERROR_OK) {

	    // TODO: default endpoint? 
	    // note: may modify names.trg.name in-place
	    rv = resolve_endpoint(&names[0], cset, 0, &trgep);
	    if (rv == CBM_ERROR_OK) {

		srcep = trgep;	// default
		rv = resolve_endpoint(&names[1], cset, 0, &srcep);
		if (rv == CBM_ERROR_OK) {
    	
		    if (srcep == trgep) {

			// find the source file
			srcdir = srcep->ptype->root(srcep);
			rv = resolve_dir((const char**)&names[1].name, cset, &srcdir);

			if (rv == CBM_ERROR_OK) {
			    // now resolve the actual source filename into dirent
			    rv = resolve_scan(srcdir, &names[1], 1, cset, 
					    false, &dirent, NULL);
			    if (rv == CBM_ERROR_OK && dirent) {
		
				// find the target directory	
				trgdir = trgep->ptype->root(trgep);
				rv = resolve_dir((const char**)&names[0].name, cset, &trgdir);

				if (rv == CBM_ERROR_OK) {

				    if (dirent->handler->move2) {
					rv = dirent->handler->move2(dirent, trgdir, (const char*) names[0].name, cset);
				    } else {
					rv = CBM_ERROR_FAULT;
				    }
				    trgdir->handler->fclose(trgdir, NULL, NULL);
				}
				dirent->handler->declose(dirent);
			    }
			    srcdir->handler->fclose(srcdir, NULL, NULL);
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
	rv = resolve_endpoint(name, cset, 0, &srcep);
	if (rv == CBM_ERROR_OK) {

	    // find the target directory	
	    srcdir = srcep->ptype->root(srcep);
	    rv = resolve_dir((const char**)&name->name, cset, &srcdir);

	    if (rv == CBM_ERROR_OK) {
			
		rv = resolve_open(srcdir, name, cset, &pars, FS_OPEN_RD, &fromfile);

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

			fromfile->handler->fclose(fromfile, NULL, NULL);
		}
		srcdir->handler->fclose(srcdir, NULL, NULL);
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

	rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	if (num_files < 2) {
		return CBM_ERROR_SYNTAX_UNKNOWN;
	}

	if (rv == CBM_ERROR_OK) {

	    // TODO: default endpoint? 
	    // note: may modify names.trg.name in-place
	    rv = resolve_endpoint(&names[0], cset, 0, &trgep);
	    if (rv == CBM_ERROR_OK) {

		// find the target directory	
		trgdir = trgep->ptype->root(trgep);
		rv = resolve_dir((const char**)&names[0].name, cset, &trgdir);

		if (rv == CBM_ERROR_OK) {
			
		    rv = resolve_open(trgdir, names, cset, &pars, FS_OPEN_WR, &fp);

		    if (rv == CBM_ERROR_OK) {

		    	for (int i = 1; rv == CBM_ERROR_OK && i < num_files; i++) {
			    if (names[i].drive == NAMEINFO_UNUSED_DRIVE 
				|| names[i].drive == NAMEINFO_LAST_DRIVE) {
				names[i].drive = names[i-1].drive;
			    }
			    rv = copy_file(fp, &names[i], cset);
			}

			fp->handler->fclose(fp, NULL, NULL);
		    }
		}
		trgdir->handler->fclose(trgdir, NULL, NULL);

		provider_cleanup(trgep);
	    }
	}
	
	return rv;
}

// ----------------------------------------------------------------------------------

int cmd_block(int tfd, const char *indata, const int datalen, char *outdata, int *outlen) {

	(void)datalen; // silence warning unused parameter

	drive_and_name_t name;
	endpoint_t *ep = NULL;

	int rv = CBM_ERROR_DRIVE_NOT_READY;

	drive_and_name_init(&name);

	// not file-related, so no file descriptor (tfd)
	// we only support mapped drives (thus name is NULL)
	// we only interpret the drive, so namelen for the lookup is 1

	name.drive = *indata;
	rv = resolve_endpoint(&name, CHARSET_ASCII, 0, &ep);
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
	int num_files = 2;
	drive_and_name_t names[2];
	endpoint_t *ep = NULL;

	rv = parse_filename_packet((uint8_t*) inname, namelen, &pars, names, &num_files);

	if (rv != CBM_ERROR_OK) {
		return rv;
	}

	if (num_files > 2) {
		return CBM_ERROR_SYNTAX_PATTERN;
	}
	if (num_files == 2) {
		// format has ID
		if (names[1].drive != NAMEINFO_UNUSED_DRIVE) {
			return CBM_ERROR_SYNTAX_INVAL;
		}
		if (names[1].name == NULL
			|| strlen((char*)names[1].name) > 2) {
			return CBM_ERROR_SYNTAX_INVAL;
		}
	}

	rv = resolve_endpoint(names, cset, 0, &ep);

	if (rv == CBM_ERROR_OK && ep != NULL) {
		provider_t *prov = (provider_t*) ep->ptype;
		if (prov->format != NULL) {
			rv = prov->format(ep, (char*)names[0].name, num_files == 2 ? (char*)names[1].name : NULL);
		}
	}
	return rv;
}

	

