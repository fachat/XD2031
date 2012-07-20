
# directory name used in includes
DEVICE=petSD

DEVICEFILES=petSD/ieeehw.c
DEVICEDEPS=petSD/device.h petSD/config.h petSD/atn.o petSD/ieeehw.h

DEVICEASMOBJ=atn.o

ARCHDEVICE=-mmcu=atmega1284p
#ARCHDEVICE=-mmcu=atmega644p

## Assembler files
petSD/atn.o: petSD/atn.S
	$(CC) $(INCLUDES) $(ARCHDEVICE) $(ASMFLAGS) -c  $<

deviceclean:
	rm -f atn.o

