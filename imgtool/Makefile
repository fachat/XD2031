# Verbose compile:                make V=1
# Cross compile for Windows:      make WIN=y
# To compile and zip for Windows: make winzip

XD2031PATH=..
INCLUDE=-I $(XD2031PATH)/common -I $(XD2031PATH)/pcserver

PRGNAME=imgtool

ifeq ($(WIN),y)
	CROSS=i686-pc-mingw32-
	EXE=$(PRGNAME).exe
else
	EXE=$(PRGNAME)
	LIB+=-lncurses
endif

ifneq ($(V),1)
	SILENT=@
endif

OBJ=$(PRGNAME).o diskimgs.o log.o terminal.o relfiles.o wildcard.o
CC=$(CROSS)gcc
CFLAGS=-Wall -std=c99 -g
RM=rm -f

all:	$(EXE)

# Link object files
$(EXE):	$(OBJ)
	@echo LINK $(EXE)
	$(SILENT)$(CC) $(CFLAGS) -o $(EXE) $(OBJ) $(LIB)

# Local files residing in this directory used only for imgtool
imgtool.o:	imgtool.c imgtool.h
	@echo CC $@
	$(SILENT)$(CC) $(CFLAGS) $(INCLUDE) -c $*.c

relfiles.o:	relfiles.c
	@echo CC $@
	$(SILENT)$(CC) $(CFLAGS) $(INCLUDE) -c $*.c

# External files from XD2031/common
wildcard.o:	$(XD2031PATH)/common/wildcard.c
	@echo CC $@
	$(SILENT)$(CC) $(CFLAGS) $(INCLUDE) -c $<


# External files from XD2031/pcserver
%.o:	$(XD2031PATH)/pcserver/%.c
	@echo CC $@
	$(SILENT)$(CC) $(CFLAGS) $(INCLUDE) -c $<

clean:
	$(RM) $(OBJ) $(EXE) $(PRGNAME).zip $(PRGNAME).exe

winzip:
	make WIN=y clean
	$(RM) $(PRGNAME).zip
	make WIN=y
	zip $(PRGNAME).zip $(PRGNAME).exe

.PHONY: all clean dist
