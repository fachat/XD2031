# Nano488 Options
#
# DO NOT EDIT Makefiles to set your options
# Your changes would prevent updating with 'git pull'.
# Store your personal settings here:
#
#         ~/.xd2031/firmware.conf
#
USE_FAT?=n                       # Enable optional FAT module?
USE_RTC?=n                       # Enable optional real time clock?

#########################################################################
# Should not be modified

HW_NAME=NANO488

MCU?=m328p

# specific switches for this device
DEFS=-DHAS_IEEE

# AVR fuses settings
# arduino default clock setting
LFUSE = ff
# arduino default DA plus set reset vector to 0x0000 and BOOTSZ = 256 byte
HFUSE = df
# arduino default
EFUSE = fd

# SCK period
DUDE_SPEED=-B 1.3

# only used to clean dependency directory on clean
SRC+=ieeehw.c device.c

# include platform specific Makefile
include avr/Makefile

# No petSD-II specific files
devicefiles:

