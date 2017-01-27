
Tests
=====


dirXXX
	create N number of single-block files, until the directory is full
	checks directory block allocation etc
	Note: dir22 and dir26 test the track selection for new files in a directory,
	as for 4040 and 1001 respectively, the "side" is changed the first time, from 
	a track below the dir track to a track above the dir track (16 -> 19, resp.
	track 37 -> 40)

dirXXXrel
	create N number of relative files; note they only test on 1001, as 4040 has a block
	allocation bug that mixes all up. 
	Note: 1001 "only" has the error that a rel file has 0 blocks instead of 3, but we expect that.
	Note: dir10rel (unexpectedly?) changes the track from 37 to 40 even though there is still
	space in the track.

dir9rel
	It seems there is a problem in the drive writing back the BAM of the directory track,
	as it is all zeroed out


Remaining errors
================


dir145 (4040), dir225 (1001)
	When a new file is created in a directory that is already full, the CBM DOS
	allocates a bogus block that is not freed again.
	(and also writes the data to it)

dir9rel (1541)
	A BAM save error on the real drive. The actual BAM entry is zeroed out instead of
	containing the real values

1581 tests
	The first 8 file blocks contain bogus remainders from the directory header block


