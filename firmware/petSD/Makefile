# petSD Options
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

MCU?=m1284p

# specific switches for this device
DEFS+=-DHAS_IEEE -DHAS_EEPROM

# AVR fuses settings
LFUSE = f7
HFUSE = d2
EFUSE = ff

# SCK period
DUDE_SPEED=-B 1.0

SRC+=petSD/ieeehw.c
SRC+=petSD/device.c
SRC+=petSD/atn.S

# SD card low level routines
ifeq ($(strip $(USE_FAT)),y)
	SRC+=fatfs/sdcard.c
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

# Device specific scripts that should be copied to the bin directory
devicefiles: $(THISBINDIR)/petSDisp.sh $(THISBINDIR)/petSD_fix_fuses.sh $(THISBINDIR)/programmer.cfg
$(THISBINDIR)/petSDisp.sh:
	$(V1) cp petSD/petSDisp.sh $@
$(THISBINDIR)/petSD_fix_fuses.sh:
	$(V1) cp petSD/petSD_fix_fuses.sh $@
$(THISBINDIR)/programmer.cfg:
	$(V1) cp petSD/programmer.cfg $@
