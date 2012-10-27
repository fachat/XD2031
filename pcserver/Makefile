PREFIX=/usr/local
BINDIR=bin
DOCDIR=xd2031/doc
SAMPLEDIR=xd2031/sample
TOOLSDIR=xd2031/tools
# The space separeted list of directories in DROPDIR will be removed at uninstall too
DROPDIR=xd2031

CC=gcc
CFLAGS=-g -W -Wall -pedantic -ansi -std=c99

COMMONC=fscmd.c privs.c name.c dir.c log.c fs_provider.c curl_provider.c provider.c mem.c
COMMONH=fscmd.h privs.h name.h dir.h wireformat.h log.h provider.h mem.h types.h

#output of `curl-config --libs`
#LIBS=-L/usr/lib/i386-linux-gnu -lcurl -Wl,-Bsymbolic-functions
LIBS=-lcurl

# fstcp builds a version that listens on a TCP/IP port - but there is not
# device (yet) to use it
#all: fstcp fsser
all: fsser

fsser: fsser.c $(COMMONC) $(COMMONH)
	# Note: LIBS must be at the end for curl/libs to be found
	${CC} ${CFLAGS} fsser.c $(COMMONC) -o fsser ${LIBS}
	sudo chown root $@
	sudo chmod u+s $@

clean:
	rm -f fstcp fsser curltest


run:
	MALLOC_CHECK_=1 ./fsser -d auto -A0=fs:../sample -A1=fs:../tools .

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
	chmod u+s $(PREFIX)/$(BINDIR)/fsser
	install -m 0755 fstcp $(PREFIX)/$(BINDIR)
	test -d $(PREFIX)/$(DOCDIR) || mkdir -p $(PREFIX)/$(DOCDIR)
	cp ../doc/* $(PREFIX)/$(DOCDIR)
	test -d $(PREFIX)/$(SAMPLEDIR) || mkdir -p $(PREFIX)/$(SAMPLEDIR)
	cp ../sample/* $(PREFIX)/$(SAMPLEDIR)
	test -d $(PREFIX)/$(TOOLSDIR) || mkdir -p $(PREFIX)/$(TOOLSDIR)
	cp ../tools/* $(PREFIX)/$(TOOLSDIR)

uninstall:
	rm -f $(PREFIX)/$(BINDIR)/fsser
	rm -f $(PREFIX)/$(BINDIR)/fstcp
	rm -f $(PREFIX)/$(DOCDIR)/*
	rmdir $(PREFIX)/$(DOCDIR) 2>/dev/null | exit 0
	rm -f $(PREFIX)/$(SAMPLEDIR)/*
	rmdir $(PREFIX)/$(SAMPLEDIR) 2>/dev/null | exit 0
	rm -f $(PREFIX)/$(TOOLSDIR)/*
	rmdir $(PREFIX)/$(TOOLSDIR) 2>/dev/null | exit 0
	for i in $(DROPDIR) ; do \
	  rmdir $(PREFIX)/$$i 2>/dev/null | exit 0; \
	done;

# make dmg generates xd2031.dmg for binary distribution with uninstaller
# ---> edit --version "0.9.1" a few lines below
# ---> compile with -arch i386 in CFLAGS first!
# works on OS X only
dmg:
	# Clean first, then make a temporary fauxroot install
	rm -rf fauxroot dmg xd2031.dmg
	make install PREFIX=fauxroot/usr/local
	# Create .pkg from fauxroot
	mkdir dmg
	/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker \
	--verbose --root-volume-only --root fauxroot/  --id com.xd2031 \
	--version "0.9.1" --target 10.5 --discard-forks --out dmg/XD-2031.pkg \
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
	hdiutil create -srcfolder dmg -volname "XD-2031" xd2031.dmg
	# Clean again
	rm -rf fauxroot dmg

.PHONY: all install uninstall run dmg
