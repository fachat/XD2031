
CC=gcc
CFLAGS=-g -W -Wall -pedantic -ansi -std=c99

COMMONC=fscmd.c privs.c name.c dir.c log.c fs_provider.c curl_provider.c provider.c
COMMONH=fscmd.h privs.h name.h dir.h wireformat.h log.h provider.h

#output of `curl-config --libs`
#LIBS=-L/usr/lib/i386-linux-gnu -lcurl -Wl,-Bsymbolic-functions
LIBS=-lcurl

# fstcp builds a version that listens on a TCP/IP port - but there is not
# device (yet) to use it
all: fstcp fsser
#all: fsser

fstcp: fstcp.c $(COMMONC) $(COMMONH)
	# Note: LIBS must be at the end for curl/libs to be found
	${CC} ${CFLAGS} fstcp.c $(COMMONC) -o fstcp ${LIBS}
	# The following lines are only necessary if you want to listen on
	# privileged ports (<1024)
	#sudo chown root $@
	#sudo chmod u+s $@

fsser: fsser.c $(COMMONC) $(COMMONH)
	# Note: LIBS must be at the end for curl/libs to be found
	${CC} ${CFLAGS} fsser.c $(COMMONC) -o fsser ${LIBS}
	sudo chown root $@
	sudo chmod u+s $@

clean:
	rm -f fstcp fsser curltest


run:
	MALLOC_CHECK_=1 ./fsser -d /dev/ttyUSB0 -A0=fs:../sample -A1=fs:../tools .

debug:
	echo "Start with: run -d /dev/ttyUSB0 ../sample"
	sudo gdb ./fsser 

curltest: curl_test.c curl_provider.c log.c
	${CC} ${CFLAGS} -o curltest curl_provider.c curl_test.c log.c ${LIBS}

doc:
	doxygen Doxyfile
