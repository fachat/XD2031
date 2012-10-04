

# Uncomment one of the following lines to select your device:
DEVICE=xs1541
#DEVICE=petSD

# This defines the serial-over-USB port to use when loading the
# firmware into the device with "make load"
# For AVR (AtMega) needs avrdude - see avr/Makefile for details
SERIAL=/dev/ttyUSB0

# If you do ISP programming with "make flash", select your programmer/port:
DUDE_PROGRAMMER = avrispmkii
# DUDE_PROGRAMMER = stk200

DUDE_PORT = usb
# DUDE_PORT = /dev/parport0
# DUDE_PORT = lpt1

# If you didn't find your programmer, have a took at the list given from:
# avrdude -c ?
#
####################################################################
# Should not be modified

TARGET=fw

FILES=ieee.c bus.c main.c errormsg.c cmd.c file.c term.c name.c channel.c serial.c provider.c rtconfig.c
DEPS=assert.h ieee.h bus.h main.h errormsg.h errors.h channel.h status.h debug.h cmd.h cmd2.h term.h packet.h name.h provider.h petscii.h serial.h delay.h wireformat.h led.h rtconfig.h version.h file.h

all:$(TARGET).hex

# The device makefile automatically includes the platform Makefile
include $(DEVICE)/Makefile

doc:
	doxygen Doxyfile

rootclean:
	rm -f $(TARGET) $(TARGET).hex

