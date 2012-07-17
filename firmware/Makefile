

SERIAL=/dev/ttyUSB0

TARGET=fw

FILES=ieee.c bus.c main.c errormsg.c cmd.c file.c term.c name.c channel.c serial.c provider.c
DEPS=ieee.h bus.h main.h errormsg.h channel.h status.h debug.h cmd.h term.h packet.h name.h provider.h petscii.h serial.h delay.h wireformat.h

all:$(TARGET).hex

# Uncomment one of the following lines to select your device:
include xs1541/Makefile
#include petSD/Makefile

include avr/Makefile

