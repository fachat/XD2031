################################################################################
#
# AVR ATMega Architecture
#

ARCHFILES=avr/uarthw.c avr/ledhw.c avr/timerhw.c avr/avr.c
ARCHDEPS=avr/compat.h avr/delayhw.h avr/uarthw.h avr/uartints.o avr/mem.h avr/timerhw.h avr/timerint.o
ARCHASMOBJ=avr/uartints.o avr/timerint.o

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
CFLAGS=-Wall -gdwarf-2 -std=gnu99 -O1 -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -Wl,-u,vfprintf -lprintf_flt  $(ARCHINCLUDE) $(ARCHDEVICE) $(DEVICEOPTS) -I$(DEVICE) -I.

clean: archclean deviceclean

distclean: clean
	rm -f $(BINDIR)/$(TARGET).elf $(BINDIR)/$(TARGET).hex
	
$(BINDIR)/$(TARGET).elf: $(FILES) $(DEPS) $(ARCHDEPS) $(ARCHFILES) $(DEVICEDEPS) $(DEVICEFILES)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$(TARGET).elf $(ARCHASMOBJ) $(DEVICEASMOBJ) $(FILES) $(ARCHFILES) $(DEVICEFILES)


$(BINDIR)/$(TARGET).hex : $(BINDIR)/$(TARGET).elf
	 avr-objcopy -j .data -j .text -O ihex $< $@

# Bootloader AVR programming with AVRPROG compatible bootloader from
# http://www.siwawi.arubi.uni-kl.de/avr_projects/#avrprog_boot
# Do NOT use $(TARGET_MCU) here. The bootloader expects m644 only
# but flashes m644p as well.
load:
	$(DUDE) -v -c avr109 -P $(SERIAL) -b 115200 -p m644 -u -U flash:w:$(BINDIR)/$(TARGET).hex:i

# ISP programming 
flash:
	$(DUDE) -v -y -u -c $(DUDE_PROGRAMMER) -P $(DUDE_PORT) -p $(TARGET_MCU) -U flash:w:$(BINDIR)/$(TARGET).hex:i

# Fuse programming
fuses:
	$(DUDE) -v -y -c $(DUDE_PROGRAMMER) -P $(DUDE_PORT) -p $(TARGET_MCU) -U lfuse:w:0x$(LFUSE):m -U hfuse:w:0x$(HFUSE):m -U efuse:w:0x$(EFUSE):m
# 

# Info about memory usage
size:
	avr-size -C $(BINDIR)/$(TARGET).elf --mcu=$(SIZE_MCU)

archclean:
	rm -f $(ARCHASMOBJ)

## Assembler files
avr/%.o: avr/%.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -I$(DEVICE) -o $@ -c  $<



