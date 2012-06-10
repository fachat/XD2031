
DUDE=/opt/cross/bin/avrdude
SERIAL=/dev/ttyUSB0

TARGET=fw

CC=avr-gcc
INCLUDE=-I/opt/cross/avr/include
#DEVICE=-mmcu=atmega644p -D__AVR_ATmega644P__
DEVICE=-mmcu=atmega644p

# F_CPU represents the clock frequency and is used to compute the delay loop times in util/delay.h
CFLAGS=-Wall -gdwarf-2 -std=gnu99 -DF_CPU=14745600UL -Os -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums $(INCLUDE) $(DEVICE)

NOTINCLUDED=hexdump.c ymodem.c

FILES=ieee.c main.c timer.c arch-timer.c uart2.c errormsg.c cmd.c file.c term.c name.c channel.c
DEPS=ieee.h main.h timer.h arch-timer.h uart2.h errormsg.h channel.h atomic.h status.h debug.h compat.h led.h xs1541.h cmd.h term.h packet.h name.h provider.h petscii.h

all: $(TARGET).hex

fw: $(FILES) $(DEPS)
	$(CC) $(CFLAGS) -o $(TARGET) $(FILES)


$(TARGET).hex : $(TARGET)
	 avr-objcopy -j .data -j .text -O ihex $< $@

load:
	$(DUDE) -v -c avr109 -P $(SERIAL) -b 115200 -p m644 -u -U flash:w:$(TARGET).hex:i

clean:
	rm $(TARGET) $(TARGET).hex

