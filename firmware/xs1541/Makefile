
# specific switches for this device
DEVICEOPTS=-DHAS_IEC -DHAS_IEEE

# MCU name madness
ARCHDEVICE = -mmcu=atmega644p   # for compiling
TARGET_MCU = m644p              # for programming with AVRDUDE
SIZE_MCU   = atmega644p         # for info about memory usage

# AVR fuses settings
LFUSE = ff
HFUSE = d4
EFUSE = fc

DEVICEFILES=xs1541/ieeehw.c xs1541/iechw.c xs1541/device.c
DEVICEDEPS=xs1541/device.h xs1541/config.h xs1541/atn.o xs1541/ieeehw.h xs1541/iechw.h xs1541/device.h

DEVICEASMOBJ=xs1541/atn.o

## Assembler files
xs1541/%.o: xs1541/%.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -o $@ -c  $<

deviceclean:
	rm -f $(DEVICEASMOBJ)

# include platform specific Makefile
include avr/Makefile

