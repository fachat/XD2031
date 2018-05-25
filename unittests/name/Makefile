

INCLUDES=-I.. -I../../firmware -I../../common
LIBS=
CFLAGS=-g -std=gnu99 -Wall -pedantic

NAMESRC=../../common/name.c  ../../common/cmdnames.c
TESTSRC=../myunit.c

tests: nametest
	./nametest -q

clean:
	rm -f nametest

nametest: nametest.c ${NAMESRC} ${TESTSRC}
	gcc ${CFLAGS} -o $@ ${INCLUDES} ${LIBS} $< ${NAMESRC} ${TESTSRC}


