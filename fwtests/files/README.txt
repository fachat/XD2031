
Tests
=====


fXXX
	create files of the given length, and check if the file on disk matches
	This is a test for block allocation and data pointer arithmetic in last block

fgapXXX	
	create a long file, another long file, scratch the first file, then create a longer
	file, to see how block allocation works with gaps in the BAM


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


