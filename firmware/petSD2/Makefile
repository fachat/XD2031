# petSD-II Options
#
# DO NOT EDIT Makefiles to set your options
# Your changes would prevent updating with 'git pull'.
# Store your personal settings here:
#
#         ~/.xd2031/firmware.conf
#
USE_FAT?=y                       # Enable optional FAT module?
USE_RTC?=y                       # Enable optional real time clock?

#########################################################################
# Should not be modified

HW_NAME=PETSD-II

MCU?=m1284p

# specific switches for this device
DEFS+=-DHAS_IEEE -DHAS_EEPROM

# AVR fuses settings
LFUSE = f7
HFUSE = d2
EFUSE = ff

# SCK period
DUDE_SPEED=-B 1.3

SRC+=ieeehw.c
SRC+=device.c
SRC+=atn.S

# SD card low level routines
ifeq ($(strip $(USE_FAT)),y)
   SRC+=fatfs/sdcard.c
   INCPATHS+=rtc
endif

ifeq ($(strip $(USE_RTC)),y)
   DEFS+=-DHAS_RTC
   INCPATHS+=i2c rtc
   # T commands
   SRC+=rtc/rtc.c
   # DS1307 Real time clock routines
   SRC+=rtc/ds1307.c
   # I2C high level routines
   SRC+=i2c/i2c.c
   # I2C bit banging routines
   SRC+=i2c/i2c-bb.c
endif

# include platform specific Makefile
include avr/Makefile

# No petSD-II specific files
devicefiles:
