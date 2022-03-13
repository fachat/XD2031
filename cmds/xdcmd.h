/****************************************************************************

    xd2031 filesystem server - command frontend
    Copyright (C) 2018 Andre Fachat

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


extern int cmd_dir(int sockfd, int argc, const char *argv[]);
extern int cmd_ls(int sockfd, int argc, const char *argv[]);
extern int cmd_put(int sockfd, int argc, const char *argv[]);
extern int cmd_get(int sockfd, int argc, const char *argv[]);
extern int cmd_rm(int sockfd, int argc, const char *argv[]);
extern int cmd_rmdir(int sockfd, int argc, const char *argv[]);
extern int cmd_mkdir(int sockfd, int argc, const char *argv[]);
extern int cmd_move(int sockfd, int argc, const char *argv[]);
extern int cmd_copy(int sockfd, int argc, const char *argv[]);
extern int cmd_assign(int sockfd, int argc, const char *argv[]);
extern int cmd_cd(int sockfd, int argc, const char *argv[]);

// --------------------------------------------------------------------------
// helpers

void log_cbmerr(uint8_t cerrno, uint8_t track, uint8_t sect);

// --------------------------------------------------------------------------
// send/receive packets


int send_packet(int sockfd, const uint8_t *buf, int len);

// convenience
int send_cmd(int sockfd, uint8_t cmd, uint8_t fd);

// convenience
int send_longcmd(int sockfd, uint8_t cmd, uint8_t fd, nameinfo_t *ninfo);

int recv_packet(int fd, uint8_t *outbuf, int buflen);


