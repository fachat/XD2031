################################################################################
#
# AVR ATMega Architecture
#

ARCHFILES=avr/timer.c avr/arch-timer.c avr/uarthw.c
ARCHDEPS=avr/timer.h avr/arch-timer.h avr/atomic.h avr/compat.h avr/led.h avr/delayhw.h avr/uarthw.h

ARCH=avr

DUDE=/opt/cross/bin/avrdude

CC=avr-gcc
ARCHINCLUDE=-I/opt/cross/avr/include -I$(ARCH) 
#ARCHDEVICE=-mmcu=atmega644p -D__AVR_ATmega644P__
ARCHDEVICE=-mmcu=atmega644p

# F_CPU represents the clock frequency and is used to compute the delay loop times in util/delay.h
CFLAGS=-Wall -gdwarf-2 -std=gnu99 -DF_CPU=14745600UL -O1 -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums $(ARCHINCLUDE) $(ARCHDEVICE) -I$(DEVICE) -I.

all: $(TARGET).hex

fw: $(FILES) $(DEPS) $(ARCHDEPS) $(ARCHFILES) $(DEVICEDEPS) $(DEVICEFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(FILES) $(ARCHFILES) $(DEVICEFILES)


$(TARGET).hex : $(TARGET)
	 avr-objcopy -j .data -j .text -O ihex $< $@

load:
	$(DUDE) -v -c avr109 -P $(SERIAL) -b 115200 -p m644 -u -U flash:w:$(TARGET).hex:i

clean:
	rm $(TARGET) $(TARGET).hex

