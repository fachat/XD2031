
CC=gcc
CFLAGS=-g -W -Wall -pedantic -ansi -std=c99

COMMONC=fscmd.c privs.c name.c dir.c log.c fs_provider.c
COMMONH=fscmd.h privs.h name.h dir.h wireformat.h log.h provider.h

all: fstcp fsser

fstcp: fstcp.c $(COMMONC) $(COMMONH)
	${CC} ${CFLAGS} fstcp.c $(COMMONC) -o fstcp
	sudo chown root $@
	sudo chmod u+s $@

fsser: fsser.c $(COMMONC) $(COMMONH)
	${CC} ${CFLAGS} fsser.c $(COMMONC) -o fsser
	sudo chown root $@
	sudo chmod u+s $@

clean:
	rm -f fstcp fsser


run:
	./fsser -d /dev/ttyUSB0 ../sample

debug:
	echo "Start with: run -d /dev/ttyUSB0 ../sample"
	sudo gdb ./fsser 

