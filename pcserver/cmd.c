/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012,2014 Andre Fachat, Nils Eilers

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
#include "cmdnames.h"

#define DEBUG_CMD
#undef DEBUG_CMD_TERM
#undef DEBUG_READ
#undef DEBUG_WRITE

#define	MAX_BUFFER_SIZE			64
#define	RET_BUFFER_SIZE			200


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

int cmd_assign(const char *assign_str, charset_t cset, int from_cmdline) {

	log_debug("Assigning from server: '%s'\n", assign_str);

			if (!isdigit(assign_str[0])) {
				log_error("Could not identify %c as drive number!\n", assign_str[0]);
				return CBM_ERROR_FAULT;
			}

			if (assign_str[1] != ':') {
				log_error("Could not identify %s as ASSIGN parameter\n", assign_str);
				return CBM_ERROR_FAULT;
			}
	
			// int rv = provider_assign(argv[i][2] & 0x0f, &(argv[i][4]));
			int rv=0;
			int drive = assign_str[0] & 0x0f;
			char *provider_parameter;
			const char *provider_name;
			int provider_len;

			// provider name followed by parameter?
			char *p = strchr(assign_str, '=');
			if (p) {
				if ((p - assign_str - 2) > MAX_LEN_OF_PROVIDER_NAME) {
					log_error("Provider name '%.8s'.. exceeds %d characters\n", assign_str + 2,
						  MAX_LEN_OF_PROVIDER_NAME);
				} else {
					// fix provider parameter character set
					//const char *orig_charset = mem_alloc_str(
					//				provider_get_ext_charset());
					//provider_set_ext_charset(CHARSET_ASCII_NAME);

					provider_name = assign_str + 2;
					provider_len = p - assign_str - 2;
					provider_parameter = p + 1;

					// make provider name a null-terminated string
					char *pname = mem_alloc_c(provider_len + 1, "provider_name");
					strncpy (pname, provider_name, provider_len+1);
					pname[provider_len] = 0;

					// check trailing '/' on provider parameter
					int l = strlen(provider_parameter);
					if (l > 0 && provider_parameter[l-1] == '/') {
						provider_parameter[l-1] = 0;
					}

					log_debug("cmdline_assign '%s' = '%s'\n", pname, 
						provider_parameter);
					rv = provider_assign(drive, pname, 
						provider_parameter, CHARSET_ASCII, from_cmdline);

					mem_free(pname);
					// reset character set
					//provider_set_ext_charset(orig_charset);
					//mem_free(orig_charset);
				}
			} else {
				log_debug("No parameter for cmdline_assign\n");
				rv = provider_assign(drive, assign_str + 2, NULL, cset, 0);
			} 
			if (rv < 0) {
				log_error("Could not assign, error number is %d\n", rv);
			}
	return rv;
}

/**
 * take the command line, search for "-A<driv>=<name>" parameters and assign
 * the value; returns error code
 */
int cmd_assign_from_cmdline(list_t *assign_list) {

	int err = CBM_ERROR_OK;

	list_iterator_t *iter = list_iterator(assign_list);
	while (list_iterator_has_next(iter)) {
		const char *name = list_iterator_next(iter);
		err = cmd_assign(name, CHARSET_ASCII, 1);

		if (err != CBM_ERROR_OK) {
			log_error("%d Error assigning %s\n", err, name);
			break;
		}
	}

#if 0

	for (int i = 0; i < argc; i++) {

		if (argv[i][0] != '-') {
			continue;
		}

		if ((strlen(argv[i]) >2) 
			&& argv[i][1] == 'X') {

			if (strchr(argv[i]+2, ':') == NULL) {
				// we need a ':' as separator between bus name and actual command
				log_error("Could not find bus name separator ':' in '%s'\n", argv[i]+2);
				continue;
			}

			xcmd_register(argv[i]+2);
		}
		if ((strlen(argv[i]) >4) 
			&& argv[i][1] == 'A') {

		}
	}
#endif
	return err;
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
}


int cmd_info(char *outbuf, int *outlen, charset_t outcset) {
	
	int rv = CBM_ERROR_OK;

	const char *info = "XDrive2031 filesystem server (C) A. Fachat et. al.";
	
	*outlen = cconv_converter(CHARSET_ASCII, outcset) (info, strlen(info), outbuf, strlen(info));
	
	return rv;
}

int cmd_read(int tfd, char *outbuf, int *outlen, int *readflag, charset_t outcset) {
	
	int rv = CBM_ERROR_FILE_NOT_OPEN;

	file_t *fp = channel_to_file(tfd);
	if (fp != NULL) {
		    *readflag = 0;	// default just in case
		    rv = fp->handler->readfile(fp, outbuf, MAX_BUFFER_SIZE-FSP_DATA, readflag, outcset);
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

int cmd_open_dir(int tfd, const char *inname, int namelen, charset_t cset) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	const char *name = NULL;
	file_t *fp = NULL;

	//log_debug("Open directory for drive: %d\n", 0xff & buf[FSP_DATA]);
	endpoint_t *ep = provider_lookup(inname, namelen, cset, &name, NAMEINFO_UNDEF_DRIVE);
	if (ep != NULL) {
		provider_t *prov = (provider_t*) ep->ptype;
		const char *options = get_options(inname, namelen - 1);
		log_info("OPEN_DR(%d->%s:%s)\n", tfd, prov->name, name);
		rv = handler_resolve_dir(ep, &fp, name, cset, NULL, options);
		if (rv == 0) {
			channel_set(tfd, fp);
		} else {
			log_rv(rv);
		}
		// cleanup when not needed anymore
		provider_cleanup(ep);
		mem_free(name);
	}
	return rv;
}

int cmd_delete(const char *inname, int namelen, charset_t cset, char *outbuf, int *outlen, int isrmdir) {
	int rv = CBM_ERROR_DRIVE_NOT_READY;
	int outdeleted = 0;
	file_t *file = NULL;
	file_t *dir = NULL;
	int readflag;
	const char *name;
	const char *outname;

	(void) namelen;	// silence unused warning

	endpoint_t *ep = provider_lookup(inname, namelen, cset, &name, NAMEINFO_UNDEF_DRIVE);
	if (ep != NULL) {
		rv = handler_resolve_dir(ep, &dir, name, cset, NULL, NULL);

		if (rv == CBM_ERROR_OK) {

			if (dir->handler->direntry != NULL) {

				while (((rv = dir->handler->direntry(dir, &file, 1, &readflag, &outname, cset))
							== CBM_ERROR_OK)
					&& file != NULL) {

					log_info("DELETE(%s)\n", file->filename);

					if (isrmdir) {
						if (file->handler->rmdir != NULL) {
							// supports RMDIR
							rv = file->handler->rmdir(file);
						} else {
							log_warn("File %s does not support RMDIR\n", 
									file->filename);
						}
					} else {
						rv = file->handler->scratch(file);
					}

					if (rv != CBM_ERROR_OK) {
						break;
					}
					outdeleted++;
				}

				if (rv == CBM_ERROR_OK) {
					outbuf[0] = outdeleted > 99 ? 99 : outdeleted;
					*outlen = 1;
					rv = CBM_ERROR_SCRATCHED;
				}
			} else {
				rv = CBM_ERROR_FAULT;
			}
			dir->handler->close(dir, 1, NULL, NULL);
		}
		mem_free(name);
		provider_cleanup(ep);
	}
	return rv;
}

int cmd_mkdir(const char *inname, int namelen, charset_t cset) {

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
}

int cmd_chdir(const char *inname, int namelen, charset_t cset) {

	int rv = CBM_ERROR_FAULT;

	log_info("CHDIR(%s)\n", inname);

	rv = provider_chdir(inname, namelen, cset);

	return rv;
}

int cmd_move(const char *inname, int namelen, charset_t cset) {

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
}

int cmd_copy(const char *inname, int namelen, charset_t cset) {

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

int cmd_block(int tfd, const char *indata, const int datalen, char *outdata, int *outlen) {

	(void)datalen; // silence warning unused parameter

	int rv = CBM_ERROR_DRIVE_NOT_READY;

	// not file-related, so no file descriptor (tfd)
	// we only support mapped drives (thus name is NULL)
	// we only interpret the drive, so namelen for the lookup is 1
	endpoint_t *ep = provider_lookup(indata, 1, 0, NULL, NAMEINFO_UNDEF_DRIVE);
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

int cmd_format(const char *inname, int namelen, charset_t cset) {

	int rv = CBM_ERROR_DRIVE_NOT_READY;
	const char *name = NULL;

	endpoint_t *ep = provider_lookup(inname, namelen, cset, &name, NAMEINFO_UNDEF_DRIVE);
	if (ep != NULL) {
		provider_t *prov = (provider_t*) ep->ptype;
		if (prov->format != NULL) {
			rv = prov->format(ep, inname + 1);
		}
	}
	return rv;
}

	

