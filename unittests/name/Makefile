

INCLUDES=-I.. -I../../firmware -I../../common
LIBS=
CFLAGS=-g -std=gnu99 -Wall -pedantic

NAMESRC=../../firmware/name.c  ../../firmware/cmdnames.c
TESTSRC=../myunit.c

tests: nametest
	./nametest -q

clean:
	rm -f nametest

nametest: nametest.c ${NAMESRC} ${TESTSRC}
	gcc ${CFLAGS} -o $@ ${INCLUDES} ${LIBS} $< ${NAMESRC} ${TESTSRC}


