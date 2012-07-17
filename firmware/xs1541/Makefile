
# directory name used in includes
DEVICE=xs1541

DEVICEFILES=xs1541/ieeehw.c
DEVICEDEPS=xs1541/device.h xs1541/config.h xs1541/atn.o xs1541/ieeehw.h

DEVICEASMOBJ=atn.o

ARCHDEVICE=-mmcu=atmega644p

## Assembler files
xs1541/atn.o: xs1541/atn.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -c  $<

deviceclean:
	rm -f atn.o

