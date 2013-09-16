

# Uncomment one of the following lines to select your device:
DEVICE=xs1541
#DEVICE=petSD
#DEVICE=petSD2

# This defines the serial-over-USB port to use when loading the
# firmware into the device with "make load"
# For AVR (AtMega) needs avrdude - see avr/Makefile for details
SERIAL=/dev/ttyUSB0

# If you do ISP programming with "make flash", select your programmer/port:
DUDE_PROGRAMMER = avrispmkii
# DUDE_PROGRAMMER = stk200
# DUDE_PROGRAMMER = petsd2

DUDE_PORT = usb
# DUDE_PORT = /dev/parport0
# DUDE_PORT = lpt1
# DUDE_PORT = /dev/ttyUSB0

# If you didn't find your programmer, have a took at the list given from:
# avrdude -c ?
#
####################################################################
# Should not be modified

.PHONY: clean rootclean testclean doc zip zoo dist

TARGET=XD2031-$(DEVICE)
BINDIR=bin

FILES=ieee.c bus.c main.c errormsg.c cmd.c cmdnames.c file.c term.c name.c channel.c serial.c provider.c rtconfig.c iec.c bufcmd.c dirconverter.c debug.c ../common/charconvert.c ../common/wildcard.c
DEPS=assert.h ieee.h bus.h main.h errormsg.h ../common/errors.h channel.h debug.h cmd.h cmdnames.h cmd2.h term.h packet.h name.h provider.h ../common/petscii.h serial.h delay.h ../common/wireformat.h led.h rtconfig.h version.h file.h iec.h timer.h nvconfig.h bufcmd.h dirconverter.h ../common/charconvert.h ../common/wildcard.h

all:	$(BINDIR)/$(TARGET).hex size

# The device makefile automatically includes the platform Makefile
include $(DEVICE)/Makefile

# Include optional modules?
ifeq ($(strip $(USE_FAT)),y)
	include fatfs/Makefile
endif

doc:
	mkdir -p ../docsrc/doxygen
	mkdir -p ../docsrc/doxygen/firmware
	doxygen Doxyfile

rootclean:
	rm -f $(BINDIR)/$(TARGET).elf $(BINDIR)/$(TARGET).hex

testclean:
	rm -f rtc/rtc

BAR="======================================================================\\n"
zoo:			   # Compile everything
	rm -f $(BINDIR)/*
	mkdir -p $(BINDIR)
	@for d in xs1541 petSD petSD2 ;do \
	  printf "\n$(BAR)   Compiling $$d\n$(BAR)" ;\
	  make DEVICE=$$d clean; \
	  make DEVICE=$$d ;\
	  make DEVICE=$$d clean; \
	done
	# Copy firmware updater scripts
	cp xs1541/xs1541up.sh $(BINDIR)
	cp xs1541/3.1541up.sh $(BINDIR)
	cp petSD/petSDisp.sh $(BINDIR)
	# Copy petSD fix fuses script
	cp petSD/petSD_fix_fuses.sh $(BINDIR)
	# Copy ISP programmer configuration
	cp petSD/programmer.cfg $(BINDIR)

zip:	zoo
	@printf "\n$(BAR)   Zip\n$(BAR)"
	zip -j XD2031-$$(date +%Y-%m-%d).zip $(BINDIR)/*

zooclean:
	rm -f $(BINDIR)/*

dist:	zooclean zoo zip
