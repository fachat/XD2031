
REL file tests
==============

Here is a description of the various REL file tests. 
Note they have started as a more random set of test programs, only
later have they become more systematic in trying only to test
a single thing

reltest1.2
	Different types of opening a REL file, with and without
	record len, existing or not. No position or write.
reltest2.4
	open a new rel file with record length given, then do a
	position command to record 0 (no write)
reltest2.5
	same as 2.4, just different record number (in second block,
	so RECORD NOT PRESENT expected
reltest2.6
	same as 2.5 but write data as well
reltest3.c
	a convoluted set of tests, with a BASIC menu to select.
	scripts are run as "z", i.e. all tests in a row.
	Tests are concerned about correct position, writing
	across record limits etc
reltest4.7
	Similar to reltest3.c, but with more tests
reltest5.2
	create some records, position and write into the middle 
	of a record; all-zero record handling, full record handling,
	record with zero in the middle, write after read
reltest6.*
	Open a new REL file, position into first block and write,
	then write to another record and write. The position of the
	second record is strategically placed, like first block in 
	new side sector / side sector group...

The remaining errors are:

bug1) DOS does not correctly update the block count for REL files 
   when it is created.
   Interestingly the 4040 drive does not seem to be affected, while
   all(!) newer drives are.
bug2) When a new side sector starts, DOS allocates an extra data
   block, but only puts it into the data file chain. It is neiter
   included in the side sector, nor is it initialized with 0xff
   at the beginning of each record. 
   Interestingly, when larger file sizes are created, that data block
   is actually the one that is being used and put into the side sector.
   Our implementation allocates it (maybe unecessarily) but puts
   it into the side sector correctly
bug3) 2031 disk drives (in the VICE version at least) are considered 
   broken for REL files and are not included.
buf4) The 1571 drive at some point returns an ILLEGAL TRACK OR SECTOR
   instead of a RECORD NOT PRESENT.


Here is the list of "expected" changes due to these bugs:

bug1)
	reltest1.2-1541-d2
	reltest2.4-1541-d2
	reltest2.5-1541-d2
	reltest1.2-1541
	reltest2.4-1541
	reltest2.5-1541
	reltest1.2-1571
	reltest2.4-1571
	reltest1.2-8050		(also has error in DIR response due to this bug)
	reltest2.4-8050
	reltest1.2-1001		(also has error in DIR response due to this bug)
	reltest2.4-1001
	reltest2.5-1001
	reltest1.2-1581

bug2)
	reltest6.7-1541		(second side sector contains 2 instead of one block)
	reltest6.7-4040
	reltest6.7-1571		(second side sector has 2 blocks)
	reltest6.7-8050
	reltest6.7-1581
	reltest6.7-1001
	reltest6.a-1541		(third side sector contains 2 instead of one block)
	reltest6.a-1001
	reltest6.d-1001
	reltest6.d-1581
	reltest6.g-1001
	reltest6.g-1581
	
bug4)
	reltest6.1-1571		(the P command for the "1234" data is not correctly
				 executed due to Illegal T&S, so no extra data block,
				 and also the data is written into the wrong record
				 i.e. the one after the last write)

