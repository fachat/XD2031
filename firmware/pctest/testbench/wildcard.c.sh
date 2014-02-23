#!/bin/sh
# http://en.wikipedia.org/wiki/Here_document

TESTFILE=wildcard
CFLAGS="-Wall -std=c99"
INCLUDE="-I.. -I../.. -I../../../common"

cc -D PCTEST $INCLUDE $CFLAGS ../../../common/$TESTFILE.c ../mains/$TESTFILE.c ../../cmdnames.c -o ../bin/$TESTFILE || exit 1

../bin/$TESTFILE << "EOF"
!NAMES
BUDGET1977
BUDGET1978
BUDGET1979
BUDGET1979.Q1
BUDGET1979.Q2
BUDGET1979.Q3

!PATTERNS
BUDGET197?
BUDGET1979?
BUDGET1979*
*Q*
*GET***?Q*?
*NOTTHERE***?Q*?

!CLASSIC_MATCH
!ADVANCED_MATCH
EOF
exit 0
