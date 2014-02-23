#!/bin/sh
# http://en.wikipedia.org/wiki/Here_document

TESTFILE=filetypes
CFLAGS="-Wall -std=c99"
INCLUDE="-I.. -I../.. -I../../../common"

cc -D PCTEST $INCLUDE $CFLAGS ../../../common/$TESTFILE.c ../mains/$TESTFILE.c ../../cmdnames.c -o ../bin/$TESTFILE || exit 1

../bin/$TESTFILE << "EOF"
# First parameter: filename
# [   Second parameter: numeric type for files without extension (2 = PRG)
#   [ Third parameter: numeric type for files with unknown extension (1 = SEQ) ] ]
#
without_extension 1 1
without_extension 2 1
without_extension
program.prg
PROGRAM.PRG
data.seq
DATA.SEQ
data.rel
DATA.REL
user.usr
USER.USR
image.d64
IMAGE.D64
image.d71
IMAGE.D71
image.d80
IMAGE.D80
image.d81
IMAGE.D81
image.d82
IMAGE.D82
unknown.tar.gz
EOF
exit 0
