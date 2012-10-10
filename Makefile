
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

.PHONY:	pcserver/fsser firmware doc
