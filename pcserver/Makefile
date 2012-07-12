
CC=gcc
CFLAGS=-g -W -Wall -pedantic -ansi -std=c99

COMMONC=fscmd.c privs.c name.c dir.c log.c fs_provider.c curl_provider.c
COMMONH=fscmd.h privs.h name.h dir.h wireformat.h log.h provider.h

#output of `curl-config --libs`
#LIBS=-L/usr/lib/i386-linux-gnu -lcurl -Wl,-Bsymbolic-functions
LIBS=-lcurl

all: fstcp fsser

fstcp: fstcp.c $(COMMONC) $(COMMONH)
	# Note: $LIBS must be at the end for curl/libs to be found
	${CC} ${CFLAGS} fstcp.c $(COMMONC) -o fstcp ${LIBS}
	sudo chown root $@
	sudo chmod u+s $@

fsser: fsser.c $(COMMONC) $(COMMONH)
	# Note: $LIBS must be at the end for curl/libs to be found
	${CC} ${CFLAGS} fsser.c $(COMMONC) -o fsser ${LIBS}
	sudo chown root $@
	sudo chmod u+s $@

clean:
	rm -f fstcp fsser curltest


run:
	./fsser -d /dev/ttyUSB0 ../sample

debug:
	echo "Start with: run -d /dev/ttyUSB0 ../sample"
	sudo gdb ./fsser 

curltest: curl_test.c curl_provider.c log.c
	${CC} ${CFLAGS} -o curltest curl_provider.c curl_test.c log.c ${LIBS}

