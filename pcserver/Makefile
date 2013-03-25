VERSION=0.9.2-RC2

PREFIX=/usr/local
BINDIR=bin
DOCDIR=xd2031/doc
SAMPLEDIR=xd2031/sample
TOOLSDIR=xd2031/tools
# The space separeted list of directories in DROPDIR will be removed at uninstall too
DROPDIR=xd2031

CC=gcc
CFLAGS=-g -W -Wall -pedantic -ansi -std=c99 -I../common

# Uncomment the following line for a 32 bit version that will run on 
# both 32 and 64 bit OS X
# CFLAGS+= -arch i386

COMMONC=fscmd.c privs.c name.c dir.c log.c fs_provider.c curl_provider.c provider.c mem.c xcmd.c os.c tcp_provider.c serial.c
COMMONH=fscmd.h privs.h name.h dir.h ../common/wireformat.h log.h provider.h mem.h types.h xcmd.h os.h ../common/errors.h serial.h

#output of `curl-config --libs`
#LIBS=-L/usr/lib/i386-linux-gnu -lcurl -Wl,-Bsymbolic-functions
LIBS=-lcurl

# Open the serial device with root or user rights
PRIVILEGED=y
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
	PRIVILEGED=n
endif
ifeq ($(PRIVILEGED),n)
	CFLAGS+=-DUNPRIVILEGED
endif

# fstcp builds a version that listens on a TCP/IP port - but there is not
# device (yet) to use it
#all: fstcp fsser
all: fsser

fsser: fsser.c $(COMMONC) $(COMMONH)
	# Note: LIBS must be at the end for curl/libs to be found
	${CC} ${CFLAGS} fsser.c $(COMMONC) -o fsser ${LIBS}
ifeq ($(PRIVILEGED),y)
	sudo chown root $@
	sudo chmod u+s $@
	@echo 'fsser opens the serial device with root rights.'
else
	@echo 'fsser opens the serial device with user rights.'
endif

clean:
	rm -f fsser curltest


run:
	MALLOC_CHECK_=1 ./fsser -d auto -A0=fs:../sample -A1=fs:../tools .

runv:
	# I'm lazy...
	MALLOC_CHECK_=1 ./fsser -v -d auto -A0=fs:../sample -A1=fs:../tools .

run9:
	MALLOC_CHECK_=1 ./fsser -d auto -A0=fs:../sample -A1=fs:../tools -Xiec:U=9 -Xieee:U=9 .

debug:
	echo "Start with: run -d auto ../sample"
	sudo gdb ./fsser 

curltest: curl_test.c curl_provider.c log.c
	${CC} ${CFLAGS} -o curltest curl_provider.c curl_test.c log.c ${LIBS}

doc:
	mkdir -p ../docsrc/doxygen
	doxygen Doxyfile

install:
	test -d $(PREFIX) || mkdir -p $(PREFIX)
	test -d $(PREFIX)/$(BINDIR) || mkdir -p $(PREFIX)/$(BINDIR)
	install -m 0755 fsser $(PREFIX)/$(BINDIR)
ifeq ($(PRIVILEGED),y)
	chmod u+s $(PREFIX)/$(BINDIR)/fsser
endif
	test -d $(PREFIX)/$(DOCDIR) || mkdir -p $(PREFIX)/$(DOCDIR)
	cp ../doc/* $(PREFIX)/$(DOCDIR)
	test -d $(PREFIX)/$(SAMPLEDIR) || mkdir -p $(PREFIX)/$(SAMPLEDIR)
	cp ../sample/* $(PREFIX)/$(SAMPLEDIR)
	test -d $(PREFIX)/$(TOOLSDIR) || mkdir -p $(PREFIX)/$(TOOLSDIR)
	cp ../tools/* $(PREFIX)/$(TOOLSDIR)

uninstall:
	rm -f $(PREFIX)/$(BINDIR)/fsser
	rm -f $(PREFIX)/$(DOCDIR)/*
	rmdir $(PREFIX)/$(DOCDIR) 2>/dev/null | exit 0
	rm -f $(PREFIX)/$(SAMPLEDIR)/*
	rmdir $(PREFIX)/$(SAMPLEDIR) 2>/dev/null | exit 0
	rm -f $(PREFIX)/$(TOOLSDIR)/*
	rmdir $(PREFIX)/$(TOOLSDIR) 2>/dev/null | exit 0
	for i in $(DROPDIR) ; do \
	  rmdir $(PREFIX)/$$i 2>/dev/null | exit 0; \
	done;

# make dmg generates xd2031-$(VERSION).dmg for binary distribution 
# with uninstaller
# ---> compile with -arch i386 in CFLAGS first!
# works on OS X only
dmg:
	# Clean first, then make a temporary fauxroot install
	rm -rf fauxroot dmg XD-2031-$(VERSION).dmg
	make install PREFIX=fauxroot/usr/local
	# Create .pkg from fauxroot
	mkdir dmg
	/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker \
	--verbose --root-volume-only --root fauxroot/  --id com.xd2031 \
	--version "$(VERSION)" --target 10.5 --discard-forks --out dmg/XD-2031-$(VERSION).pkg \
	--title "XD-2031" --domain system
	# you may want to add some README etc. to dmg/
	cp COPYING dmg/
	# auto-generate uninstall-xd2031 from fauxroot
	echo "#!/bin/sh" > dmg/XD-2031-uninstall
	for i in `find fauxroot -type f | cut -f 2- -d"/"` ;\
	  do echo sudo rm -f /$$i >> dmg/XD-2031-uninstall ;\
	done
	echo "sudo rmdir $(PREFIX)/$(DOCDIR) 2>/dev/null" >> dmg/XD-2031-uninstall
	echo "sudo rmdir $(PREFIX)/$(SAMPLEDIR) 2>/dev/null" >> dmg/XD-2031-uninstall
	echo "sudo rmdir $(PREFIX)/$(TOOLSDIR) 2>/dev/null" >> dmg/XD-2031-uninstall
	for i in $(DROPDIR) ; do \
	  echo "sudo rmdir $(PREFIX)/$$i 2>/dev/null" >> dmg/XD-2031-uninstall ;\
	done;
	echo "echo \"XD-2031 was removed from your system.\"" >> dmg/XD-2031-uninstall
	echo "read -p \"Press ENTER to continue...\"" >> dmg/XD-2031-uninstall
	chmod +x dmg/XD-2031-uninstall
	# Create dmg image from dmg/ directory
	hdiutil create -srcfolder dmg -volname "XD-2031" XD-2031-$(VERSION).dmg
	# Clean again
	rm -rf fauxroot dmg
	@echo Did you remember compiling first with CFLAGS+= -arch i386?

.PHONY: all install uninstall run dmg
