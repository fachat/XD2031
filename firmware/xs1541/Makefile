
# directory name used in includes
DEVICE=xs1541

DEVICEFILES=xs1541/ieeehw.c
DEVICEDEPS=xs1541/device.h xs1541/config.h xs1541/atn.o xs1541/ieeehw.h

DEVICEASMOBJ=xs1541/atn.o

ARCHDEVICE=-mmcu=atmega644p

## Assembler files
xs1541/%.o: xs1541/%.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -o $@ -c  $<

deviceclean:
	rm -f $(DEVICEASMOBJ)


