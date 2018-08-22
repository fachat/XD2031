

# Note: main differences in Makefile.common, here (still) needed for ARCH
# Cross compile for Windows with make WIN=y
# Please note doc/README-win32
ifeq ($(WIN),y)
  ARCH    = win32
else
  ARCH    = posix
endif

TARGET=fsser

# Sorting include paths and defines simplifies checking them
INCPATHS=. ../common util os handler $(ARCH)

# TODO: expand SRC and vpath from INCPATHS
SRC=$(wildcard *.c) $(wildcard util/*.c) $(wildcard os/*.c)  $(wildcard handler/*.c) $(wildcard ../common/*.c) $(wildcard $(ARCH)/*.c)

EXTCFLAGS=-DSERVER

# locations where make searches C source files
vpath %.c . ../common util os handler $(ARCH)

include Makefile.common


run:
	MALLOC_CHECK_=1 ./fsser -d auto -A0:fs=../sample -A1:fs=../tools .

-include Makefile.local

doc:
	mkdir -p ../docsrc/doxygen
	doxygen Doxyfile

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
