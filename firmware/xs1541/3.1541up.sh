#!/bin/sh
# Update 3.1541 (=XS-1541 @ Raspberry Pi) firmware via bootloader

PORT=/dev/ttyAMA0
default=XD2031-xs1541.hex

# This script requires the WiringPi library found at
# https://projects.drogon.net/raspberry-pi/wiringpi/
# to access Raspberry Pi's GPIO pins without the need
# of root rights

# Declare AVR reset pin as output
gpio mode 0 out
# Reset 3.1541
gpio write 0 1
# Wait...
sleep .2
# Release reset
gpio write 0 0
# Wait...
sleep .2

if [ $# -ne 1 ]
then
  if test ! -s $default
  then
    echo Could not find default file $default, aborting
    exit 1
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
