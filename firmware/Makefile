

SERIAL=/dev/ttyUSB0

TARGET=fw

FILES=ieee.c main.c errormsg.c cmd.c file.c term.c name.c channel.c serial.c
DEPS=ieee.h main.h errormsg.h channel.h status.h debug.h cmd.h term.h packet.h name.h provider.h petscii.h serial.h delay.h wireformat.h

all:$(TARGET)

include xs1541/Makefile

include avr/Makefile

