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

DEVICEFILES=petSD2/ieeehw.c petSD2/device.c
DEVICEDEPS=petSD2/device.h petSD2/config.h petSD2/atn.o petSD2/ieeehw.h petSD2/device.h

DEVICEASMOBJ=atn.o

## Assembler files
petSD2/atn.o: petSD2/atn.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -c  $<

deviceclean:
	rm -f atn.o

# include platform specific Makefile
include avr/Makefile
