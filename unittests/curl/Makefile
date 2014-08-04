
CC=gcc

SERVER=../../pcserver

INCPATHS=. $(SERVER) ../../common
INCLUDE=$(sort $(addprefix -I,$(INCPATHS)))

CFLAGS=-g -W -Wall -pedantic -ansi -std=c99 -funsigned-char $(INCLUDE) -DSERVER
LDFLAGS=-lncurses -lcurl -lc


curltest: curl_test.c $(SERVER)/curl_provider.c $(SERVER)/log.c
	${CC} ${CFLAGS} -o curltest $^ ${LDFLAGS}

