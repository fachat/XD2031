################################################################################
#
# AVR ATMega Architecture
#

ARCHFILES=avr/uarthw.c avr/ledhw.c avr/timer.c avr/avr.c
ARCHDEPS=avr/compat.h avr/delayhw.h avr/uarthw.h avr/uartints.o avr/mem.h avr/timer.h avr/timerint.o
ARCHASMOBJ=avr/uartints.o avr/timerint.o

ARCH=avr

DUDE=/opt/cross/bin/avrdude

CC=avr-gcc
ARCHINCLUDE=-I/opt/cross/avr/include -I$(ARCH) 

ASMFLAGS=

# Note that due to me using the "%02x" printf conversion, actually the
# full(!) printf version is required, including the floating point stuff, to be linked.
# That is where the "-Wl,-u,vfprintf -lprintf_flt" option comes from...
CFLAGS=-Wall -gdwarf-2 -std=gnu99 -O1 -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -Wl,-u,vfprintf -lprintf_flt  $(ARCHINCLUDE) $(ARCHDEVICE) -I$(DEVICE) -I.

clean: archclean deviceclean

distclean: clean
	rm $(TARGET) $(TARGET).hex
	
fw: $(FILES) $(DEPS) $(ARCHDEPS) $(ARCHFILES) $(DEVICEDEPS) $(DEVICEFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(ARCHASMOBJ) $(DEVICEASMOBJ) $(FILES) $(ARCHFILES) $(DEVICEFILES)


$(TARGET).hex : $(TARGET)
	 avr-objcopy -j .data -j .text -O ihex $< $@

load:
	$(DUDE) -v -c avr109 -P $(SERIAL) -b 115200 -p m644 -u -U flash:w:$(TARGET).hex:i

archclean:
	rm -f $(ARCHASMOBJ)

## Assembler files
avr/%.o: avr/%.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -o $@ -c  $<



