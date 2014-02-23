#!/bin/sh
# http://en.wikipedia.org/wiki/Here_document

TESTFILE=rtc
CFLAGS="-Wall -std=c99 -DHAS_RTC"
INCLUDE="-I.. -I../.. -I../../rtc"

cc -D PCTEST $INCLUDE $CFLAGS ../../rtc/$TESTFILE.c ../mains/$TESTFILE.c -o ../bin/$TESTFILE || exit 1

../bin/$TESTFILE << "EOF"
# Calling without parameters should output date and time (empty line)
T
# Calling without parameters should output date and time (whitespaces)
T  
# Called as TI should output a ti$ compatible format
TI
# Setting time in a ti$ compatible format
TI123456
# Set only day of week to monday
TMON
# Set only day of week to tuesday
TTUE
# Set date to 2012-01-13
T2012-01-13
# Set date without delimiter (it was a monday)
T20111024
# Should be a sunny sunday
T2038-02-28
# Set time to 12:34
TT12:34
# Set time to 12:34:56
TT12:34:56
# Set time to 12:34
TT1234
# Set time to 12:34:56
TT123456
# Set time to 12:34 (whitespaces)
T   T  1  2  :  3  4
# Set day of week, date and time
TWED 2013-01-02 T23:45:16
#####################################################################
############ The following tests should produce errors ##############
#####################################################################
# Anything but not a date
TLAYER 8 ERROR
# Year < 2000
T1999-01-01
# Year > 2099
T2100-01-01
# Month < 1
T2013-00-00
# Month > 12
T2013-13-01
# Day < 1
T2013-01-00
# Day > 31
T2013-01-32
# Short date
T2013-01
# Short time
TT12
# Too short time
TT
# Hours too long
TT123:4
# Minutes too long
TT12:003
# Minutes too long
TT12:003:0
# Year too long but day too short
T20131-02-3
# Month too long but day too short
T2013-123-0
EOF
exit

Anything below 'exit' will be ignored.
Feel free to move the lines EOF/exit if you want to run some tests only.
