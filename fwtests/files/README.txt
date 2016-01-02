
Tests
=====


fXXX
	create files of the given length, and check if the file on disk matches
	This is a test for block allocation and data pointer arithmetic in last block
dirXXX
	create N number of single-block files, until the directory is full
	checks directory block allocation etc
	Note: dir22 and dir26 test the track selection for new files in a directory,
	as for 4040 and 1001 respectively, the "side" is changed the first time, from 
	a track below the dir track to a track above the dir track (16 -> 19, resp.
	track 37 -> 40)


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
	=> maybe block allocation error (interleave?)
./filetests-d82.sh -C -q
f168488-1001.frs: Ok
f168488-1001.frs: File blk.d82 differs!
	==> DOS has residue of previous block written to last block
f253-1001.frs: Ok
f254-1001.frs: Ok
f255-1001.frs: Ok
f507-1001.frs: Ok
f508-1001.frs: Ok
f509-1001.frs: Ok
f509-1001.frs: File blk.d82 differs!
	==> DOS has residue of previous block written to last block

