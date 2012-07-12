
all: pcserver/fsser firmware/fw.hex

pcserver/fsser:
	make -C pcserver

firmware/fw.hex:
	make -C firmware

clean:
	make -C pcserver clean
	make -C firmware clean

