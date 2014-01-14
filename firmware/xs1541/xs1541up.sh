#!/bin/sh
# Update XS-1541 firmware via bootloader

PORT=/dev/ttyUSB0

# ------------------------------------------------------------------------
# Do not edit below this line
#

default=XD2031-xs1541.hex
if test ! -s $default
then
  if [ `ls *.hex | wc -w` = "1" ] ; then
    default=`find . -name *.hex`
    echo Defaulting to $default
  else
    printf "Error: None or more than a single .hex files found.\n"
    printf "Please specify one with $0 <name.hex>\n"
    exit 1
  fi
fi

# If the Server is running, terminate it first to free the serial line
if pidof fsser > /dev/null ; then
  sudo service fsser stop
  RESTART=true
else
  RESTART=false
fi

if [ $# -ne 1 ]
then
  if test ! -s $default ; then
    echo $default not found
  else
    avrdude -v -c avr109 -P $PORT -b 115200 -p m644 -u -U flash:w:$default
  fi
else
  if test ! -s "$1"
  then
    echo File $1 not found
  else
    avrdude -v -c avr109 -P $PORT -b 115200 -p m644 -u -U flash:w:$1
  fi
fi

# Restart the Server if it was running before
if $RESTART ; then
  sudo service fsser start
fi
