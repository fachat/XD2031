
all:	pcserver/fsser firmware doc samples

samples: sample/telnet sample/u1test

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

uninstall:
	make -C pcserver uninstall

sample/telnet: sample/telnet.lst
	petcat -l 0401 -w4 sample/telnet.lst > sample/telnet
sample/u1test: sample/u1test.lst
	petcat -l 0401 -w4 sample/u1test.lst > sample/u1test

.PHONY:	pcserver/fsser install uninstall firmware doc
