PREFIX=/usr/local
BINDIR=bin
DOCDIR=xd2031/doc
SAMPLEDIR=xd2031/sample
TOOLSDIR=xd2031/tools
# The space separeted list of directories in DROPDIR will be removed at uninstall too
DROPDIR=xd2031

CC=$(CROSS)gcc
LD=$(CROSS)ld
AR=$(CROSS)ar
PKG_CONFIG=$(CROSS)pkg-config

FSSER_EXE=fsser
CFLAGS=-g -W -Wall -pedantic -ansi -std=c99 -funsigned-char -I. -I../common -DSERVER

#COMMONC=fscmd.c privs.c dir.c log.c fs_provider.c curl_provider.c provider.c mem.c xcmd.c os.c tcp_provider.c serial.c channel.c di_provider.c ../common/charconvert.c ../common/wildcard.c openpars.c diskimgs.c ../common/filetypes.c terminal.c handler.c registry.c x00_handler.c
COMMONC=fscmd.c privs.c dir.c log.c fs_provider.c di_provider.c provider.c mem.c xcmd.c os.c serial.c channel.c  ../common/charconvert.c ../common/wildcard.c openpars.c diskimgs.c ../common/filetypes.c terminal.c handler.c registry.c x00_handler.c
COMMONH=fscmd.h privs.h ../common/wildcard.h dir.h ../common/wireformat.h log.h provider.h mem.h types.h xcmd.h os.h ../common/errors.h serial.h channel.h ../common/petscii.h ../common/charconvert.h openpars.h handler.h registry.h diskimgs.h ../common/filetypes.h terminal.h

#output of `curl-config --libs`
#LIBS=-L/usr/lib/i386-linux-gnu -lcurl -Wl,-Bsymbolic-functions
LIBS=-lncurses -lcurl

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
	${CC} ${CFLAGS} fsser.c $(COMMONC) -o $(FSSER_EXE) ${LIBS}
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
	MALLOC_CHECK_=1 ./fsser -d auto -A0:fs=../sample -A1:fs=../tools .

runv:
	# I'm lazy...
	#MALLOC_CHECK_=1 ./fsser -v -d auto -A0:fs=../sample/sample1.d64 -A1:fs=../sample .
	MALLOC_CHECK_=1 sudo gdb -ex "run -v -d auto -A0:fs=../sample/sample1.d64 -A1:fs=../sample ." ./fsser

runv2:
	# I'm lazy...
	MALLOC_CHECK_=1 ./fsser -v -d auto -A2:fs=../sample -A1:fs=../tools -A0:di=../sample/test.d64 -A3:ftp=ftp.zimmers.net/pub/cbm/pet -Xiec:U=9 . 

runv2d:
	MALLOC_CHECK_=1 sudo gdb -ex "run -D -v -d auto -A2:fs=../sample -A1:fs=../tools -A0:di=../sample/test.d64 -A3:ftp=ftp.zimmers.net/pub/cbm/pet -Xiec:U=9 . " ./fsser

rungdb:
	MALLOC_CHECK_=1 sudo gdb -ex "run -v -d auto -A3:fs=../sample/test2.d64 -A2:fs=../sample -A1:fs=../tools -A0:fs=../sample/test2.d64 ." ./fsser
	#MALLOC_CHECK_=1 sudo gdb -ex "run -v -d auto -A3:fs=../sample/test.d64 -A0:fs=../sample -A1:fs=../tools ." ./fsser
	#MALLOC_CHECK_=1 sudo gdb -ex "run -v -d auto -A0:fs=../sample -A1:fs=../tools ." ./fsser

run9:
	MALLOC_CHECK_=1 ./fsser -v -d auto -A0:fs=../sample -A1:fs=../tools -Xiec:U=9 -Xieee:U=9 .

debug:
	echo "Start with: run -d auto ../sample"
	sudo gdb ./fsser 

curltest: curl_test.c curl_provider.c log.c
	${CC} ${CFLAGS} -o curltest curl_provider.c curl_test.c log.c ${LIBS}

doc:
	mkdir -p ../docsrc/doxygen
	doxygen Doxyfile

install:
	@if [ `id -u` != "0" ] ; then echo "must be root!"; exit 1; fi;
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
	@if [ `id -u` != "0" ] ; then echo "must be root!"; exit 1; fi;
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
	hdiutil create -srcfolder dmg -volname "XD-2031" xd2031-osx-$$(date +%Y-%m-%d).dmg
	# Clean again
	rm -rf fauxroot dmg

daemon:
	@if [ `id -u` != "0" ] ; then echo "must be root!"; exit 1; fi;
	sed -e 's|PREFIX|'$(PREFIX)'|g' fsser.init.d > fsser.init.d.localized
	sed -i -e 's|BINDIR|'$(BINDIR)'|g' fsser.init.d.localized
	sed -e 's|PREFIX|'$(PREFIX)'|g' fsserd > fsserd.localized
	sed -i -e 's|BINDIR|'$(BINDIR)'|g' fsserd.localized
	./daemon.sh $(PREFIX)/$(BINDIR)

exorcism:
	@if [ `id -u` != "0" ] ; then echo "must be root!"; exit 1; fi;
	service fsser stop
	@if $$(command -v chkconfig > /dev/null ) ; then       \
	   chkconfig --del fsser                              ;\
	   rm -f /etc/init.d/fsser $(PREFIX)/$(BINDIR)/fsserd ;\
	   systemctl daemon-reload                            ;\
	else                                                   \
	   rm -f /etc/init.d/fsser $(PREFIX)/$(BINDIR)/fsserd ;\
	   update-rc.d fsser remove                           ;\
	fi
	@echo "Personal files preserved:"
	@echo "- /etc/default/fsser"
	@echo "- /var/log/fsser"

.PHONY: all install uninstall run dmg fsser daemon exorcism
