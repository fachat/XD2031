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
SERIAL?=/dev/ttyUSB0

# If you do ISP programming with "make flash", select your programmer/port:
DUDE_PROGRAMMER ?= avrispmkii

DUDE_PORT ?= usb

####################################################################
# Should not be modified


ARCH=avr
TARGET=$(THISBINDIR)/$(BINNAME).elf
HEX=$(TARGET:.elf=.hex)

firmware: $(HEX) devicefiles

CC=$(SILENT_CC)avr-gcc
AS=$(SILENT_AS)avr-gcc $(MCU_CC)
LD=$(SILENT_LD)avr-gcc $(MCU_CC)

SRC+=avr/uarthw.c avr/ledhw.c avr/timerhw.c avr/avr.c avr/nvconfighw.c avr/reset.c
SRC+=avr/uartints.S avr/timerint.S

CFLAGS+=-g
CFLAGS+=-Os
CFLAGS+=-funsigned-char
CFLAGS+=-fshort-enums

# Note that due to me using the "%02x" printf conversion, actually the
# full(!) printf version is required, including the floating point stuff, to be linked.
# That is where the "-Wl,-u,vfprintf -lprintf_flt" option comes from...
LDFLAGS+=-Wl,-u,vfprintf -lprintf_flt

# MCU name madness
ifeq ($(strip $(MCU)),m1284p)
   MCU_CC=-mmcu=atmega1284p
   MCU_AVRDUDE=m1284p
   MCU_SIZE=atmega1284p
endif
ifeq ($(strip $(MCU)),m644p)
   MCU_CC=-mmcu=atmega644p
   MCU_AVRDUDE=m644p
   MCU_SIZE=atmega644p
endif
ifeq ($(strip $(MCU)),m644)
   MCU_CC=-mmcu=atmega644
   MCU_AVRDUDE=m644
   MCU_SIZE=atmega644
endif
# arduino nano
ifeq ($(strip $(MCU)),m328p)
   MCU_CC=-mmcu=atmega328p
   MCU_AVRDUDE=m328p
   MCU_SIZE=atmega328p
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

# Bootloader AVR programming with AVRPROG compatible bootloader from
# http://www.siwawi.arubi.uni-kl.de/avr_projects/#avrprog_boot
# Do NOT use $(MCU_AVRDUDE) here. The bootloader expects m644 only
# but flashes m644p as well.
load: $(HEX)
	#$(DUDE) $(DUDE_VERBOSE) -c avr109 -P $(SERIAL) -b 115200 -p m644 -u -U flash:w:$(THISBINDIR)/$(BINNAME).hex:i
	#$(DUDE) $(DUDE_VERBOSE) -c arduino -P $(SERIAL) -b 57600 -p m328p -u -U flash:w:$(THISBINDIR)/$(BINNAME).hex:i
	$(DUDE) $(DUDE_VERBOSE) -c arduino -P $(SERIAL) -b 115200 -p m328p -u -U flash:w:$(THISBINDIR)/$(BINNAME).hex:i

# ISP programming
flash:	$(HEX)
	$(DUDE) $(DUDE_VERBOSE) -u -c $(DUDE_PROGRAMMER) -P $(DUDE_PORT) $(DUDE_SPEED) -p $(MCU_AVRDUDE) \
	        -U flash:w:$(THISBINDIR)/$(BINNAME).hex:i

# Fuse programming
fuses:
	$(DUDE) $(DUDE_VERBOSE) -c $(DUDE_PROGRAMMER) -P $(DUDE_PORT) -p $(MCU_AVRDUDE) -B 10 \
                -U lfuse:w:0x$(LFUSE):m -U hfuse:w:0x$(HFUSE):m -U efuse:w:0x$(EFUSE):m

readfuses:
	$(DUDE) $(DUDE_VERBOSE) -c $(DUDE_PROGRAMMER) -P $(DUDE_PORT) -p $(MCU_AVRDUDE) \
		-U lfuse:r:low_fuse_val.txt:h \
		-U hfuse:r:high_fuse_val.txt:h \
		-U efuse:r:ext_fuse_val.txt:h

