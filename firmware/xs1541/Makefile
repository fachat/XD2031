# XS-1541 Options

USE_FAT=y                       # Enable optional FAT module?

#########################################################################
# Should not be modified


# specific switches for this device
DEVICEOPTS=-DHAS_IEC -DHAS_IEEE -DHAS_EEPROM

# MCU name madness
ARCHDEVICE = -mmcu=atmega644p   # for compiling
TARGET_MCU = m644p              # for programming with AVRDUDE
SIZE_MCU   = atmega644p         # for info about memory usage

# AVR fuses settings
LFUSE = ff
HFUSE = d4
EFUSE = fc

# SCK period
DUDE_SPEED=-B 2.0

DEVICEFILES=xs1541/ieeehw.c xs1541/iechw.c xs1541/device.c
DEVICEDEPS=xs1541/device.h xs1541/config.h xs1541/atn.o xs1541/ieeehw.h xs1541/iechw.h xs1541/device.h

DEVICEASMOBJ=xs1541/atn.o

ifeq ($(strip $(USE_FAT)),y)
        DEVICEDEPS+=avr/spi.h               # SPI routines
        DEVICEFILES+=fatfs/sdcard.c         # SD card low level routines
endif

## Assembler files
xs1541/%.o: xs1541/%.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -o $@ -c  $<

deviceclean:
	rm -f $(DEVICEASMOBJ)

# include platform specific Makefile
include avr/Makefile
