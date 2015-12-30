
Tests
=====


fxxxx
	create files of the given length, and check if the file on disk matches
	This is a test for block allocation and data pointer arithmetic in last block



Remaining errors
================

make
./filetests-d64.sh -C -q
f253-4040.frs: Ok
f254-4040.frs: errors: 1
f254-4040.frs: File blk.d64 differs!
	=> when writing a 254 byte file, and closing it, 4040 DOS allocates a bogus empty sector
	   This is also reflected in the errorneous blocks free count
f255-4040.frs: Ok
f255-4040.frs: File blk.d64 differs!
./filetests-d82.sh -C -q
f253-1001.frs: Ok
f254-1001.frs: Ok
f255-1001.frs: Ok

