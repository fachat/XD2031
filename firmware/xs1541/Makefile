
# directory name used in includes
DEVICE=xs1541

DEVICEFILES=
DEVICEDEPS=xs1541/xs1541.h xs1541/config.h xs1541/atn.o

DEVICEASMOBJ=atn.o

## Assembler files
xs1541/atn.o: xs1541/atn.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -c  $<


