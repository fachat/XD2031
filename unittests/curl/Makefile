
CC=gcc

SERVER=../../pcserver

INCPATHS=. $(SERVER) $(SERVER)/util $(SERVER)/os  ../../common 
INCLUDE=$(sort $(addprefix -I,$(INCPATHS)))

CFLAGS=-g -W -Wall -pedantic -ansi -std=c99 -funsigned-char $(INCLUDE) -DSERVER
LDFLAGS=-lncurses -lcurl -lc


curltest: curl_test.c $(SERVER)/handler/curl_provider.c $(SERVER)/util/log.c
	${CC} ${CFLAGS} -o curltest $^ ${LDFLAGS}

