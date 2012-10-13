# petSD Options

USE_FAT=y                       # Enable optional FAT module?

#########################################################################
# Should not be modified

# specific switches for this device
DEVICEOPTS=-DHAS_IEEE -DHAS_RTC

# MCU name madness
ARCHDEVICE = -mmcu=atmega1284p  # for compiling
TARGET_MCU = m1284p             # for programming with AVRDUDE
SIZE_MCU   = atmega1284p        # for info about memory usage

# AVR fuses settings
LFUSE = f7
HFUSE = d2
EFUSE = ff

DEVICEFILES=petSD/ieeehw.c petSD/device.c
DEVICEDEPS=petSD/device.h petSD/config.h petSD/atn.o petSD/ieeehw.h petSD/device.h

DEVICEASMOBJ=atn.o

ifeq ($(strip $(USE_FAT)),y)
	DEVICEFILES+=petSD/fatfshw.c	    # HW specific code for FatFs
	DEVICEFILES+=fatfs/sdcard.c         # SD card low level routines
	DEVICEFILES+=fatfs/rtc-ds1307.c     # DS1307 Real time clock routines
	DEVICEFILES+=fatfs/i2c.c            # I2C high level routines
	DEVICEFILES+=fatfs/i2c-bb.c         # I2C bit banging routines
	DEVICEDEPS+=petSD/fatfshw.h	    # HW specific defs / protos for FatFs
	DEVICEDEPS+=fatfs/rtc.h             # Real time clock defs  and prototypes
	DEVICEDEPS+=fatfs/i2c.h             # I2C high level routines
endif


## Assembler files
petSD/atn.o: petSD/atn.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -c  $<

deviceclean:
	rm -f atn.o

# include platform specific Makefile
include avr/Makefile


