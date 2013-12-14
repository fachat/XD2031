# XS-1541 Options
#
# DO NOT EDIT Makefiles to set your options
# Your changes would prevent updating with 'git pull'.
# Store your personal settings here:
#
#         ~/.xd2031/firmware.conf
#
USE_FAT?=n                       # Enable optional FAT module? (requires 1284p)

# Select one of the following MCUs:
#MCU?=m644
MCU?=m644p
#MCU?=m1284p

#########################################################################
# Should not be modified


# specific switches for this device
DEFS+=-DHAS_IEC -DHAS_IEEE -DHAS_EEPROM

# AVR fuses settings
LFUSE = ff
HFUSE = d4
EFUSE = fc

# SCK period
DUDE_SPEED=-B 2.0

SRC+=xs1541/ieeehw.c xs1541/iechw.c xs1541/device.c xs1541/atn.S

# SD card low level routines
ifeq ($(strip $(USE_FAT)),y)
        CSRC+=fatfs/sdcard.c
endif

# include platform specific Makefile
include avr/Makefile

# Device specific scripts that should be copied to the bin directory
devicefiles: $(THISBINDIR)/xs1541up.sh $(THISBINDIR)/3.1541up.sh
$(THISBINDIR)/xs1541up.sh:
	cp xs1541/xs1541up.sh $@
$(THISBINDIR)/3.1541up.sh:
	cp xs1541/3.1541up.sh $@
