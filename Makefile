
all: pcserver/fsser

pcserver/fsser:
	make -C pcserver

clean:
	make -C pcserver clean

