################################################################################
#
# PC Architecture
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


ARCH=pc
TARGET=$(THISBINDIR)/$(BINNAME).elf

firmware: $(TARGET) 

CC=$(SILENT_CC)gcc -DPC
AS=$(SILENT_AS)gcc $(MCU_CC)
LD=$(SILENT_LD)gcc $(MCU_CC)

MCU_CC=-g

#SRC+=avr/uarthw.c avr/ledhw.c avr/timerhw.c avr/avr.c avr/nvconfighw.c avr/reset.c
#SRC+=avr/uartints.S avr/timerint.S
SRC+=pc/delayhw.c pc/ledhw.c pc/reset.c

CFLAGS+=-g
CFLAGS+=-Os
CFLAGS+=-funsigned-char
CFLAGS+=-fshort-enums

# Note that due to me using the "%02x" printf conversion, actually the
# full(!) printf version is required, including the floating point stuff, to be linked.
# That is where the "-Wl,-u,vfprintf -lprintf_flt" option comes from...
#LDFLAGS+=-Wl,-u,vfprintf -lprintf_flt


