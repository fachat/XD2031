
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

dirXXXrel
	create N number of relative files; note they only test on 1001, as 4040 has a block
	allocation bug that mixes all up. 1001 "only" has the error that a rel file has 0 blocks
	instead of 3, but we can expect that.
	Note: dir10rel (unexpectedly?) changes the track from 37 to 40 even though there is still
	space in the track.
	Note: dir46rel breaks when the side sector is not allocated with ALLOC_SIDE_SECTOR


Remaining errors
================

f254-4040.frs: errors: 1
f254-4040.frs: File blk.d64 differs!
	=> when writing a 254 byte file, and closing it, 4040 DOS allocates a bogus empty sector
	   This is also reflected in the errorneous blocks free count

f255-4040.frs: Ok
f255-4040.frs: File blk.d64 differs!
	=> maybe block allocation error (interleave?)

f168488-1001.frs: Ok
f168488-1001.frs: File blk.d82 differs!
	==> DOS has residue of previous block written to last block

f509-1001.frs: Ok
f509-1001.frs: File blk.d82 differs!
	==> DOS has residue of previous block written to last block

dir145 (4040), dir225 (1001)
	When a new file is created in a directory that is already full, the CBM DOS
	allocates a bogus block that is not freed again.

