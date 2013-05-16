#!/bin/sh
# Flash petSD firmware with AVR ISP programmer

PROGRAMMER=avrispmkii
CONNECTED_TO=usb

# ------------------------------------------------------------------------
# Do not edit below this line
#
default=XD2031-petSD.hex
if [ $# -ne 1 ]
then
  if test ! -s $default
  then
    echo Could not find default file $default, aborting
    exit 1
  else
    avrdude -v -y -u -c $PROGRAMMER -P $CONNECTED_TO -p m1284p -U flash:w:$default
  fi
else
  if test ! -s "$1"
  then
    echo File $1 not found
  else 
    avrdude -v -y -u -c $PROGRAMMER -P $CONNECTED_TO -p m1284p -U flash:w:$1
  fi
fi
