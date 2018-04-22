# First target, invoked by "make" without parameters
code:	pcserver/fsser firmware imgtool testrunner

all:	pcserver/fsser testrunner firmware doc samples imgtool

samples: sample/telnet sample/u1test sample/webcat

pcserver/fsser:
	make -C pcserver

imgtool:
	make -C imgtool

firmware:
	make -C firmware zoo

doc:
	make -C pcserver doc
	make -C firmware doc
clean:
	make -C pcserver clean
	make -C imgtool clean
	make -C firmware clean veryclean
	make -C testrunner clean

install:
	make -C pcserver install
	make -C imgtool install

uninstall:
	make -C pcserver uninstall
	make -C imgtool uninstall

testrunner:
	make -C testrunner

sockserv:
	DEVICE=sockserv make -C firmware

tests: pcserver/fsser testrunner sockserv
	make -C servertests tests
	make -C fwtests tests
	make -C unittests tests

	
sample/webcat: doc/webcat.lst
	petcat -l 0401 -w4 doc/webcat.lst > sample/webcat
sample/telnet: doc/telnet.lst
	petcat -l 0401 -w4 doc/telnet.lst > sample/telnet
sample/u1test: doc/u1test.lst
	petcat -l 0401 -w4 doc/u1test.lst > sample/u1test

.PHONY:	pcserver/fsser install uninstall firmware doc imgtool testrunner
