
# directory name used in includes
# this is already set in toplevel Makefile
#DEVICE=petSD

# Microcontroller name for AVRDUDE
TARGET_MCU = m1284p

# AVR fuses settings
LFUSE = f7
HFUSE = d2
EFUSE = ff

DEVICEFILES=petSD/ieeehw.c
DEVICEDEPS=petSD/device.h petSD/config.h petSD/atn.o petSD/ieeehw.h

DEVICEASMOBJ=atn.o

ARCHDEVICE=-mmcu=atmega1284p
#ARCHDEVICE=-mmcu=atmega644p

## Assembler files
petSD/atn.o: petSD/atn.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -c  $<

deviceclean:
	rm -f atn.o

# include platform specific Makefile
include avr/Makefile


