
/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

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
#include <stdio.h>

#include "log.h"
#include "provider.h"
#include "wireformat.h"
#include "handler.h"

extern provider_t ftp_provider;
extern provider_t http_provider;
extern provider_t fs_provider;

void fs_test();
void ftp_test();
void http_test();

void get_test(provider_t *prov, const char *rootpath, const char *name);
void dir_test(provider_t *prov, const char *rootpath, const char *name);

int main(int argc, char *argv[]) {

	set_verbose(1);

	provider_init();
	handler_init();

	(void) argc;
	(void) argv;

	fs_test();

	ftp_test();

	http_test();

}


void fs_test() {

	provider_t *prov = &fs_provider;

	get_test(prov, "/etc", "hosts");

	dir_test(prov, "/etc", "s*");
}

void ftp_test() {

	provider_t *prov = &ftp_provider;

	//get_test(prov, "anonymous:fachat_at_web.de@ftp.zimmers.net/pub/cbm", "00INDEX");
	//get_test(prov, "ftp.zimmers.net/pub/cbm", "00INDEX");
	get_test(prov, "ftp.zimmers.net/pub/cbm", "index.html");

	dir_test(prov, "ftp.zimmers.net/pub/cbm", "");
}

void http_test() {

	provider_t *prov = &http_provider;

	get_test(prov, "www.zimmers.net/anonftp/pub/cbm", "index.html");
}

void dir_test(provider_t *prov, const char *rootpath, const char *name) {


	printf("dirprov=%s\n", prov->name);

	prov->init();

	printf("prov init done\n");

	// get endpoint for base URL
	endpoint_t *ep = prov->newep(NULL, rootpath, CHARSET_ASCII, 1);

	printf("ep=%p\n", ep->ptype->name);

	//file_t *root = ep->ptype->root(ep);

	file_t *direntry = NULL;
	const char *outname = NULL;
	//int rv = root->handler->resolve(root, &direntry, name, CHARSET_ASCII, &outname);

	int rv = handler_resolve_dir(ep, &direntry, name, CHARSET_ASCII, &outname, NULL);

	printf("open -> %d, direntry=%p, outname=%s\n", rv, (void*)direntry, outname);

	file_t *outentry = NULL;
	int readflag = 0;
	do {
		const char *outpattern = NULL;
		outentry = NULL;

		//rv = prov->readfile(ep, 1, buffer, 100, &eof);
		rv = direntry->handler->direntry(direntry, &outentry, 0, &readflag, &outpattern, CHARSET_ASCII);

		if (rv != 0) {
			printf("readfile -> %d\n", rv);
		} else {
			printf("direntry -> %s\n", outentry ? outentry->filename : "<nil>");
		}

		if (outentry != NULL) {
			outentry->handler->close(outentry, 0, NULL, NULL);
		}
	}
	while (rv >= 0 && outentry != NULL);

	direntry->handler->close(direntry, 0, NULL, NULL);
	//root->handler->close(root, 0, NULL, NULL);
	
	ep->ptype->freeep(ep);

}

void get_test(provider_t *prov, const char *rootpath, const char *name) {

	openpars_t openpars = { FS_DIR_TYPE_UNKNOWN, 0 };
	printf("prov=%p\n", (void*)prov);

	prov->init();

	printf("prov init done\n");

	// get endpoint for base URL
	endpoint_t *ep = prov->newep(NULL, rootpath, CHARSET_ASCII, 1);

	printf("ep=%p\n", (void*)ep);

	file_t *file = NULL;
	int rv = handler_resolve_file(ep, &file, name, CHARSET_ASCII, NULL, FS_OPEN_RD);
	printf("resolve -> %d, file=%p\n", rv, (void*)file);

	//file_t *root = ep->ptype->root(ep);
	//file_t *file = NULL;
	//const char *outname = NULL;
	//int rv = root->handler->resolve(root, &file, name, CHARSET_ASCII, &outname);
	//printf("resolve -> %d, file=%p, outname=%s\n", rv, (void*)file, outname);

	rv = file->handler->open(file, &openpars, FS_OPEN_RD);
	printf("open -> %d\n", rv);

	char *buffer = malloc(8192 * sizeof(char));
	int readflag = 0;
	do {
		rv = file->handler->readfile(file, buffer, 100, &readflag, CHARSET_ASCII);

		if (rv != 0) {
			printf("readfile -> %d, readflg=%d\n", rv, readflag);
		}

		if (rv > 0) {
			buffer[rv] = 0;
			printf("---> %s\n", buffer);
		}
	}
	while (rv >= 0 && readflag != READFLAG_EOF);

	free(buffer);

	file->handler->close(file, 0, NULL, NULL);
	//root->handler->close(root, 0, NULL, NULL);
	
	// done on close of last file
	//ep->ptype->freeep(ep);

}

