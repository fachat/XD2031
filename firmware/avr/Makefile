################################################################################
#
# AVR ATMega Architecture
#

ARCHFILES=avr/timer.c avr/arch-timer.c avr/uarthw.c
ARCHDEPS=avr/timer.h avr/arch-timer.h avr/atomic.h avr/compat.h avr/led.h avr/delayhw.h avr/uarthw.h avr/ints.o
ARCHASMOBJ=ints.o

ARCH=avr

DUDE=/opt/cross/bin/avrdude

CC=avr-gcc
ARCHINCLUDE=-I/opt/cross/avr/include -I$(ARCH) 

ASMFLAGS=

CFLAGS=-Wall -gdwarf-2 -std=gnu99 -O1 -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums $(ARCHINCLUDE) $(ARCHDEVICE) -I$(DEVICE) -I.

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
	rm -f ints.o

## Assembler files
avr/ints.o: avr/ints.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -c  $<


