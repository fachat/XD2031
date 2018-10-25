# First target, invoked by "make" without parameters
code:	pcserver cmds firmware imgtool testrunner

all:	pcserver testrunner firmware doc samples imgtool cmds unittests

local:	pcserver testrunner sockserv samples imgtool cmds unittests

samples: sample/telnet sample/u1test sample/webcat

pcserver:
	make -C pcserver

imgtool:
	make -C imgtool

firmware:
	make -C firmware zoo

cmds:
	make -C cmds

doc:
	make -C pcserver doc
	make -C firmware doc
clean:
	make -C pcserver veryclean
	make -C imgtool clean
	make -C firmware veryclean
	make -C testrunner clean
	make -C cmds veryclean
	make -C unittests clean

install:
	make -C pcserver install
	make -C imgtool install
	make -C cmds install

uninstall:
	make -C pcserver uninstall
	make -C imgtool uninstall

testrunner:
	make -C testrunner

unittests:
	make -C unittests

sockserv:
	DEVICE=sockserv make -C firmware

tests: pcserver testrunner sockserv
	make -C servertests tests
	make -C fwtests tests
	make -C unittests tests

	
sample/webcat: doc/webcat.lst
	petcat -l 0401 -w4 doc/webcat.lst > sample/webcat
sample/telnet: doc/telnet.lst
	petcat -l 0401 -w4 doc/telnet.lst > sample/telnet
sample/u1test: doc/u1test.lst
	petcat -l 0401 -w4 doc/u1test.lst > sample/u1test

.PHONY:	pcserver install uninstall firmware doc imgtool testrunner cmds unittests
