# First target, invoked by "make" without parameters
code:	pcserver/fsser firmware

all:	pcserver/fsser firmware doc samples

samples: sample/telnet sample/u1test sample/webcat

pcserver/fsser:
	make -C pcserver

firmware:
	make -C firmware zoo

doc:
	make -C pcserver doc
	make -C firmware doc
clean:
	make -C pcserver clean
	make -C firmware clean zooclean

install:
	make -C pcserver install
	make -C imgtool install

uninstall:
	make -C pcserver uninstall
	make -C imgtool uninstall

sample/webcat: doc/webcat.lst
	petcat -l 0401 -w4 doc/webcat.lst > sample/webcat
sample/telnet: doc/telnet.lst
	petcat -l 0401 -w4 doc/telnet.lst > sample/telnet
sample/u1test: doc/u1test.lst
	petcat -l 0401 -w4 doc/u1test.lst > sample/u1test

.PHONY:	pcserver/fsser install uninstall firmware doc
