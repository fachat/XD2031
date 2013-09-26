#!/bin/sh
# http://en.wikipedia.org/wiki/Here_document

TESTFILE=cmdnames
CFLAGS="-Wall -std=c99"
INCLUDE="-I. -I.. -I../../common"

gcc -D PCTEST $INCLUDE $CFLAGS ../$TESTFILE.c -o $TESTFILE || exit 1

./$TESTFILE << "EOF"
# CBM Commands
I0
INIT
INI
S1:FILE
SCRA FILE
U1 2,0,18,0
RENAME NEW=OLD
R NEW=OLD
REN 1:NEW=OLD
# Illegal
MURX
# FS COMMANDS
MKDIR newdir
MD newdir
CD..
CD SUB
XRESET
$1:*
EOF
exit 0
