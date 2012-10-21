
all:	pcserver/fsser firmware doc

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

.PHONY:	pcserver/fsser install uninstall firmware doc
