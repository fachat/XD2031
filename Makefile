
all:	pcserver/fsser firmware doc sample/telnet8

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

sample/telnet8: sample/telnet8.lst
	petcat -l 0401 -w4 sample/telnet8.lst > sample/telnet8

.PHONY:	pcserver/fsser install uninstall firmware doc
