
# directory name used in includes
# This is already set in top level Makefile
#DEVICE=xs1541

# Microcontroller name for AVRDUDE
TARGET_MCU = m644p

# AVR fuses settings
LFUSE = ff
HFUSE = d4
EFUSE = fc

DEVICEFILES=xs1541/ieeehw.c
DEVICEDEPS=xs1541/device.h xs1541/config.h xs1541/atn.o xs1541/ieeehw.h

DEVICEASMOBJ=xs1541/atn.o

ARCHDEVICE=-mmcu=atmega644p

## Assembler files
xs1541/%.o: xs1541/%.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -o $@ -c  $<

deviceclean:
	rm -f $(DEVICEASMOBJ)

# include platform specific Makefile
include avr/Makefile

