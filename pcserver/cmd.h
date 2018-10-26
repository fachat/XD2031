/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

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

#ifndef FSCMD_H
#define FSCMD_H

/* status values */
/* Note: not really used at themoment, only F_FREE is set in the init */
#define F_FREE          0	/* must be 0 */

#define MAXFILES        16

void cmd_init();
void cmd_free();

int cmd_assign_cmdline(const char *inname, charset_t cset);
int cmd_assign_packet(const char *inname, int inlen, charset_t cset);
int cmd_open_file(int tfd, const char *inname, int namelen, charset_t cset, drive_and_name_t *lastdrv, char *outbuf, int *outlen, int cmd);
int cmd_read(int tfd, char *outbuf, int *outlen, int *readflag, charset_t outcset, drive_and_name_t *lastdrv);
int cmd_info(char *outbuf, int *outlen, charset_t outcset);
int cmd_write(int tfd, int cmd, const char *indata, int datalen);
int cmd_position(int tfd, const char *indata, int datalen);
int cmd_close(int tfd, char *outbuf, int *outlen);
int cmd_open_dir(int tfd, const char *inname, int namelen, charset_t cset, drive_and_name_t *lastdrv);
int cmd_delete(const char *inname, int namelen, charset_t cset, char *outbuf, int *outlen, int isrmdir);
int cmd_mkdir(const char *inname, int namelen, charset_t cset);
int cmd_chdir(const char *inname, int namelen, charset_t cset);
int cmd_move(const char *inname, int namelen, charset_t cset);
int cmd_copy(const char *inname, int namelen, charset_t cset);
int cmd_block(int tfd, const char *indata, const int datalen, char *outdata, int *outlen);
int cmd_format(const char *inname, int namelen, charset_t cset);

#endif
