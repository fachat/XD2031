
CC=gcc

SERVER=../../pcserver

COMMON=$(SERVER)/os/os.c $(SERVER)/os/terminal.c $(SERVER)/util/*.c $(SERVER)/handler/*.c ../../common/*.c $(SERVER)/handler.c $(SERVER)/dir.c $(SERVER)/provider.c $(SERVER)/openpars.c $(SERVER)/channel.c

INCPATHS=. $(SERVER) $(SERVER)/util $(SERVER)/os  ../../common 
INCLUDE=$(sort $(addprefix -I,$(INCPATHS)))

CFLAGS=-g -W -Wall -pedantic -ansi -std=c99 -funsigned-char $(INCLUDE) -DSERVER
LDFLAGS=-lncurses -lcurl -lc

all: curltest

tests:
	# currently ignored to avoid network dependencies

curltest: curl_test.c $(COMMON)
	${CC} ${CFLAGS} -o curltest $^ ${LDFLAGS}

