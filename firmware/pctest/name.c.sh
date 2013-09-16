#!/bin/sh
# http://en.wikipedia.org/wiki/Here_document

TESTFILE=name
CFLAGS="-Wall -std=c99"
INCLUDE="-I. -I.. -I../../common"

gcc -D PCTEST $INCLUDE $CFLAGS ../$TESTFILE.c ../cmdnames.c -o $TESTFILE || exit 1

./$TESTFILE << "EOF"
# To set the parsehint parameter, give a
!PARSEHINT_COMMAND
# or
!PARSEHINT_LOAD
# without any trailing or leading spaces.
# On unknown commands or known ones with (trailing) whitespaces
# you should get a syntax error.
!NOT-A-COMMAND  
#
#
# LOAD"TEST",8
TEST
# LOAD"0:TEST",8
0:TEST
# LOAD":TEST",8
:TEST
# LOAD"FTP:TEST",8
FTP:TEST
# Filename beginning with digit
!PARSEHINT_LOAD
1TEST
!PARSEHINT_COMMAND
1TEST
!PARSEHINT_COMMAND
# RENAME
R:NEW=OLD
# Some CD variations
CD 0:NAME
CD:NAME
CD NAME
CD FTP:NAME
!PARSEHINT_LOAD
NAME,SEQ,WRITE
NAME,S,W
NAME,R,U
NAME,R,P
NAME,N
RELFILE,L,?
RELFILE,L,?@
EOF
exit 0
