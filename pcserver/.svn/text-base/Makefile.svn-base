
CC=gcc
CFLAGS=-g -W -Wall -pedantic -ansi -std=c99

all: fstcp fsser

fstcp: fstcp.c fscmd.c fscmd.h 
	${CC} ${CFLAGS} fstcp.c fscmd.c -o fstcp

fsser: fsser.c fscmd.c fscmd.h 
	${CC} ${CFLAGS} fsser.c fscmd.c -o fsser

clean:
	rm -f fstcp fsser


run:
	./fsser -d /dev/ttyUSB0 ../sample

debug:
	echo "Start with: run -d /dev/ttyUSB0 ../sample"
	gdb ./fsser 

