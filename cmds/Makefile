
COMMON=../pcserver/os/os.c ../pcserver/os/terminal.c ../pcserver/util/log.c ../pcserver/util/mem.c ../testrunner/connect.c ../common/name.c ../common/cmdnames.c
INCLUDES=-I../pcserver -I../common -I../pcserver/util -I../pcserver/os -I../testrunner/
LIBS=-lncurses 
CFLAGS=-g -W -Wall -pedantic -ansi -std=c99 -funsigned-char -D_POSIX_C_SOURCE=200809 -DLOG_PREFIX=\"\]\]\" -DSERVER

all: xdcmd

clean:
	rm -f xdcmd 

xdcmd: xdcmd.c  ${COMMON}
	gcc -o $@ ${CFLAGS} ${INCLUDES} $< ${COMMON} ${LIBS}


