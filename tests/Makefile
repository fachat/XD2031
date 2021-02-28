
BASFILES=i179_rel i180_mkrel i180_lsrel

all: reldump seq dump $(BASFILES)

CFLAGS=-W -Wall -pedantic -ansi -std=c99 -g

CFILES=../pcserver/handler/diskimgs.c ../pcserver/util/log.c ../pcserver/os/terminal.c
HFILES=../pcserver/handler/diskimgs.h ../pcserver/util/log.h ../common/petscii.h ../pcserver/os/terminal.h

reldump: reldump.c ${CFILES} ${HFILES}
	gcc ${CFLAGS} -o reldump -I../pcserver -I../pcserver/handler -I../pcserver/util -I../pcserver/os -I../common reldump.c ${CFILES} -lncurses

dump: dump.lst
	petcat -w40 -o dump dump.lst

seq: seq.lst
	petcat -w40 -o seq seq.lst

$(BASFILES): % : %.lst
	petcat -w40 -o $@ $^

clean:
	rm -f reldump dump seq $(BASFILES)


