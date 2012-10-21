
# specific switches for this device
DEVICEOPTS=-DHAS_IEEE -DHAS_EEPROM

# MCU name madness
ARCHDEVICE = -mmcu=atmega1284p	# for compiling
TARGET_MCU = m1284p		# for programming with AVRDUDE
SIZE_MCU   = atmega1284p	# for info about memory usage

# AVR fuses settings
LFUSE = f7
HFUSE = d2
EFUSE = ff

DEVICEFILES=petSD/ieeehw.c petSD/device.c
DEVICEDEPS=petSD/device.h petSD/config.h petSD/atn.o petSD/ieeehw.h petSD/device.h

DEVICEASMOBJ=atn.o

## Assembler files
petSD/atn.o: petSD/atn.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -c  $<

deviceclean:
	rm -f atn.o

# include platform specific Makefile
include avr/Makefile


