# petSD Options

USE_FAT=y                       # Enable optional FAT module?

#########################################################################
# Should not be modified

# specific switches for this device
DEVICEOPTS=-DHAS_IEEE -DHAS_EEPROM -DHAS_RTC

# MCU name madness
ARCHDEVICE = -mmcu=atmega1284p  # for compiling
TARGET_MCU = m1284p             # for programming with AVRDUDE
SIZE_MCU   = atmega1284p        # for info about memory usage

# AVR fuses settings
LFUSE = f7
HFUSE = d2
EFUSE = ff

# SCK period
DUDE_SPEED=-B 1.3

DEVICEFILES=petSD/ieeehw.c petSD/device.c
DEVICEDEPS=petSD/device.h petSD/config.h petSD/atn.o petSD/ieeehw.h petSD/device.h

DEVICEASMOBJ=atn.o

ifeq ($(strip $(USE_FAT)),y)
	DEVICE_CFLAGS=-DUSE_FAT             # Define USE_FAT
	DEVICEDEPS+=avr/spi.h               # SPI routines
	DEVICEFILES+=fatfs/sdcard.c         # SD card low level routines
	DEVICEDEPS+=rtc/rtc.h               # Real time clock defs and prototypes
	DEVICEFILES+=rtc/ds1307.c           # DS1307 Real time clock routines
	DEVICEDEPS+=i2c/i2c.h               # I2C prototypes
	DEVICEFILES+=i2c/i2c.c              # I2C high level routines
	DEVICEFILES+=i2c/i2c-bb.c           # I2C bit banging routines
	DEVICEFILES+=fat_provider.c         # FAT provider
	DEVICEDEPS+=fat_provider.h
endif

## Assembler files
petSD/atn.o: petSD/atn.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -c  $<

deviceclean:
	rm -f atn.o

# include platform specific Makefile
include avr/Makefile
