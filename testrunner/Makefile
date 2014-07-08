
COMMON=../pcserver/terminal.c ../pcserver/log.c ../pcserver/mem.c ../pcserver/registry.c script.c connect.c
INCLUDES=-I../pcserver -I../common -I../firmware/sockserv
LIBS=-lncurses 
CFLAGS=-g -W -Wall -pedantic -ansi -std=c99 -funsigned-char -D_POSIX_C_SOURCE=200809 -DLOG_PREFIX=\"\]\]\"

all: fwrunner pcrunner

clean:
	rm -f fwrunner pcrunner

fwrunner: fwrunner.c  ${COMMON}
	gcc -o $@ ${CFLAGS} ${INCLUDES} $< ${COMMON} ${LIBS}

pcrunner: pcrunner.c  ${COMMON}
	gcc -o $@ ${CFLAGS} ${INCLUDES} $< ${COMMON} ${LIBS}

