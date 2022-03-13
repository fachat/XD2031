################################################################################
#
# AVR ATMega Architecture
#
# DO NOT EDIT Makefiles to set your options
# Your changes would prevent updating with 'git pull'.
# Store your personal settings here:
#
#         ~/.xd2031/firmware.conf
#
# This defines the serial-over-USB port to use when loading the
# firmware into the device with "make load"
# For AVR (AtMega) needs avrdude - see avr/Makefile for details
SERIAL?=/dev/ttyACM0

# If you do ISP programming with "make flash", select your programmer/port:
DUDE_PROGRAMMER ?= jtag2updi

DUDE_PORT ?= usb

####################################################################
# Should not be modified

ARCH=avrx
TARGET=$(THISBINDIR)/$(BINNAME).elf
HEX=$(TARGET:.elf=.hex)

firmware: $(HEX) devicefiles

CC=$(SILENT_CC)avr-gcc
AS=$(SILENT_AS)avr-gcc $(MCU_CC)
LD=$(SILENT_LD)avr-gcc $(MCU_CC)

SRC+=avrx/uarthw.c avrx/ledhw.c avrx/timerhw.c avrx/avr.c avrx/nvconfighw.c avrx/reset.c

CFLAGS+=-g
CFLAGS+=-Os
CFLAGS+=-funsigned-char
CFLAGS+=-fshort-enums

# Note that due to me using the "%02x" printf conversion, actually the
# full(!) printf version is required, including the floating point stuff, to be linked.
# That is where the "-Wl,-u,vfprintf -lprintf_flt" option comes from...
LDFLAGS+=-Wl,-u,vfprintf -lprintf_flt

# arduino nano every
ifeq ($(strip $(MCU)),m4809)
   MCU_CC=-mmcu=atmega4809
   MCU_AVRDUDE=m4809
   MCU_SIZE=avrxmega3
endif


size: $(TARGET)
	@avr-size -t $(OBJ)

# Info about memory usage, give only supported options
relsize: $(TARGET)
	@FORMAT=berkeley ;\
	if avr-size --help | grep -e "--format=" | grep -q avr ; then\
	  FORMAT=avr ;\
	fi ;\
	MCU= ;\
	if avr-size --help | grep -q -e "--mcu=" ; then\
	  MCU=--mcu=$(MCU_SIZE) ;\
	fi ;\
	avr-size --format $$FORMAT $$MCU $<

$(HEX): $(TARGET)
	$(V2) HX $(notdir $@)
	$(V1) avr-objcopy -j .data -j .text -O ihex $< $@

# If avrdude isn't at given location, search it in the $PATH
DUDE=/opt/cross/bin/avrdude
ifeq ($(wildcard $(DUDE)),)
   DUDE = avrdude
endif
ifeq ($(V),1)
   DUDE_VERBOSE=-v
endif

reset:
	# force reset
	sudo stty -F $(SERIAL) hup ispeed 1200
	sudo stty -F $(SERIAL) hup ispeed 1200

# 115200
load: $(HEX) reset
	# program
	$(DUDE) $(DUDE_VERBOSE) -C ./avrx/avrdude.conf -c $(DUDE_PROGRAMMER) -P $(SERIAL) -b 19200 -p atmega4809 -u -U flash:w:$(THISBINDIR)/$(BINNAME).hex:i

# ISP programming
#flash:	$(HEX)
#	$(DUDE) $(DUDE_VERBOSE) -u -c $(DUDE_PROGRAMMER) -P $(DUDE_PORT) $(DUDE_SPEED) -p $(MCU_AVRDUDE) \
#	        -U flash:w:$(THISBINDIR)/$(BINNAME).hex:i

# Fuse programming
fuses: reset
	$(DUDE) $(DUDE_VERBOSE) -C ./avrx/avrdude.conf -c $(DUDE_PROGRAMMER) -P $(SERIAL) -p $(MCU_AVRDUDE) -B 10 \
                -U fuse2:w:0x$(FUSE_OSCCFG):m 

readfuses: reset
	$(DUDE) $(DUDE_VERBOSE) -C ./avrx/avrdude.conf -c $(DUDE_PROGRAMMER) -P $(SERIAL) -p $(MCU_AVRDUDE) \
		-U fuse2:r:fuse2_OSCCFG.txt:h \
		-U fuse5:r:fuse5_SYSCFG0.txt:h \
		-U fuse8:r:fuse8_BOOTEND.txt:h


