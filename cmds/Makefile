

TARGET=xdcmd

CMDSRC=$(wildcard *.c)
COMMON=../pcserver/os/os.c ../pcserver/os/terminal.c ../pcserver/util/log.c ../pcserver/util/mem.c ../testrunner/connect.c ../common/name.c ../common/cmdnames.c ../pcserver/cerrno.c ../pcserver/openpars.c ../common/errnames.c

SRC=$(CMDSRC) $(COMMON)

INCPATHS=. ../pcserver ../common ../pcserver/util ../pcserver/os ../testrunner/

LIBS=-lncurses 

# _POSIX_C_SOURCE needed for nanosleep() used in testrunner/connect.c
EXTCFLAGS=-D_POSIX_C_SOURCE=200809 -DSERVER


include ../pcserver/Makefile.common

-include Makefile.local

.PHONY: all install uninstall xdcmd 

