#!/bin/sh
# Flash petSD firmware with AVR ISP programmer

# Reading configuration file
ISPCFG=programmer.cfg
if test ! -s $ISPCFG
then
	echo Could not find $ISPCFG, aborting
	exit 1
fi
. ./$ISPCFG

# Use default filename if no commandline parameter given
default=XD2031-petSD.hex
if [ $# -ne 1 ]
then
  if test ! -s $default
  then
    echo Could not find default file $default, aborting
    exit 1
  else
    avrdude -v -u -c $PROGRAMMER -P $CONNECTED_TO -p m1284p -U flash:w:$default
  fi
else
  if test ! -s "$1"
  then
    echo File $1 not found
  else 
    avrdude -v -u -c $PROGRAMMER -P $CONNECTED_TO -p m1284p -U flash:w:$1
  fi
fi
