################################################################################
#
# AVR ATMega Architecture
#

ARCHFILES=avr/uarthw.c avr/ledhw.c avr/timerhw.c avr/avr.c avr/nvconfighw.c avr/reset.c
ARCHDEPS=avr/compat.h avr/delayhw.h avr/uarthw.h avr/uartints.o avr/mem.h avr/timerhw.h avr/timerint.o avr/arch.h
ARCHASMOBJ=avr/uartints.o avr/timerint.o
# FatFs monitor (=test+demo) uses it's own kind of printf:
ifeq ($(strip $(FAT_TESTDEMO)),y)
        ARCHDEPS+=avr/xitoa.o
        ARCHASMOBJ+=avr/xitoa.o
endif

ARCH=avr

# If avrdude isn't at given location, search it in the $PATH 
DUDE=/opt/cross/bin/avrdude
ifeq ($(wildcard $(DUDE)),)
	DUDE = avrdude
endif

CC=avr-gcc
ARCHINCLUDE=-I/opt/cross/avr/include -I$(ARCH) 

ASMFLAGS=

# Note that due to me using the "%02x" printf conversion, actually the
# full(!) printf version is required, including the floating point stuff, to be linked.
# That is where the "-Wl,-u,vfprintf -lprintf_flt" option comes from...
CFLAGS=-Wall -gdwarf-2 -std=gnu99 -O1 -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -Wl,-u,vfprintf -lprintf_flt  $(ARCHINCLUDE) $(ARCHDEVICE) $(DEVICEOPTS) -I$(DEVICE) -I. -I../common

clean: archclean deviceclean

distclean: clean
	rm -f $(BINDIR)/$(TARGET).elf $(BINDIR)/$(TARGET).hex
	
$(BINDIR)/$(TARGET).elf: $(FILES) $(DEPS) $(ARCHDEPS) $(ARCHFILES) $(DEVICEDEPS) $(DEVICEFILES)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(DEVICE_CFLAGS) -o $(BINDIR)/$(TARGET).elf $(ARCHASMOBJ) $(DEVICEASMOBJ) $(FILES) $(ARCHFILES) $(DEVICEFILES)


$(BINDIR)/$(TARGET).hex : $(BINDIR)/$(TARGET).elf
	 avr-objcopy -j .data -j .text -O ihex $< $@

# Bootloader AVR programming with AVRPROG compatible bootloader from
# http://www.siwawi.arubi.uni-kl.de/avr_projects/#avrprog_boot
# Do NOT use $(TARGET_MCU) here. The bootloader expects m644 only
# but flashes m644p as well.
load:
	$(DUDE) -v -c avr109 -P $(SERIAL) -b 115200 -p m644 -u -U flash:w:$(BINDIR)/$(TARGET).hex:i

# ISP programming 
flash:	all
	$(DUDE) -v -y -u -c $(DUDE_PROGRAMMER) -P $(DUDE_PORT) $(DUDE_SPEED) -p $(TARGET_MCU) -U flash:w:$(BINDIR)/$(TARGET).hex:i

# Fuse programming
fuses:
	$(DUDE) -v -y -c $(DUDE_PROGRAMMER) -P $(DUDE_PORT) -p $(TARGET_MCU) $(DUDE_SPEED) -U lfuse:w:0x$(LFUSE):m -U hfuse:w:0x$(HFUSE):m -U efuse:w:0x$(EFUSE):m
# 

# Info about memory usage, give only supported options
size:
	@FORMAT=berkeley ;\
	if avr-size --help | grep -e "--format=" | grep -q avr ; then\
	  FORMAT=avr ;\
	fi ;\
	MCU= ;\
	if avr-size --help | grep -q -e "--mcu=" ; then\
	  MCU=--mcu=$(SIZE_MCU) ;\
	fi ;\
	avr-size --format $$FORMAT $$MCU $(BINDIR)/$(TARGET).elf 

archclean:
	rm -f $(ARCHASMOBJ)

## Assembler files
avr/%.o: avr/%.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -I$(DEVICE) -o $@ -c  $<



