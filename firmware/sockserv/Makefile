# XS-1541 Options
#
# DO NOT EDIT Makefiles to set your options
# Your changes would prevent updating with 'git pull'.
# Store your personal settings here:
#
#         ~/.xd2031/firmware.conf
#
USE_FAT?=n                       # Enable optional FAT module? (requires 1284p)

MCU=pc

#########################################################################
# Should not be modified

# specific switches for this device
#DEFS+=-DHAS_IEC -DHAS_IEEE -DHAS_EEPROM
DEFS+=-DHAS_SOCK488 

#SRC+=xs1541/ieeehw.c xs1541/iechw.c xs1541/device.c xs1541/atn.S
SRC+=sockserv/device.c sockserv/uarthw.c sockserv/sock488.c sockserv/socket.c

# include platform specific Makefile
include pc/Makefile

