

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

TARGET=XD2031-$(DEVICE)
BINDIR=bin

FILES=ieee.c bus.c main.c errormsg.c cmd.c file.c term.c name.c channel.c serial.c provider.c rtconfig.c iec.c
DEPS=assert.h ieee.h bus.h main.h errormsg.h errors.h channel.h status.h debug.h cmd.h cmd2.h term.h packet.h name.h provider.h petscii.h serial.h delay.h wireformat.h led.h rtconfig.h version.h file.h iec.h timer.h

all:	$(BINDIR)/$(TARGET).hex size

# The device makefile automatically includes the platform Makefile
include $(DEVICE)/Makefile

doc:
	mkdir -p ../doc/doxygen/firmware
	doxygen Doxyfile

rootclean:
	rm -f $(BINDIR)/$(TARGET).elf $(BINDIR)/$(TARGET).hex

BAR="======================================================================\\n"
zoo:			   # Compile everything
	rm -f $(BINDIR)/*
	mkdir -p $(BINDIR)
	@for d in xs1541 petSD ;do \
	  echo "\n$(BAR)   Compiling $$d\n$(BAR)" ;\
	  make DEVICE=$$d ;\
	  make DEVICE=$$d clean; \
	done
	# Copy firmware updater script
	cp xs1541/xs1541up.sh $(BINDIR)

zip:	zoo
	@echo "\n$(BAR)   Zip\n$(BAR)"
	zip -j XD2031-$$(date +%Y-%m-%d).zip $(BINDIR)/*

zooclean:
	rm -f $(BINDIR)/*

dist:	zooclean zoo zip
