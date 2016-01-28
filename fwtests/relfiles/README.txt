
REL file tests
==============

The remaining errors are:

bug1) DOS does not correctly update the block count for REL files 
   when it is created.
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

bug2)
	reltest6.7-1541		(second side sector contains 2 instead of one block)
	reltest6.7-4040
	reltest6.a-1541		(third side sector contains 2 instead of one block)
	reltest6.7-1571		(second side sector has 2 blocks)
	reltest6.7-8050
	reltest6.d-1581
	reltest6.7-1001
	reltest6.a-1001
	reltest6.d-1001
	
bug4)
	reltest6.1-1571		(the P command for the "1234" data is not correctly
				 executed due to Illegal T&S, so no extra data block,
				 and also the data is written into the wrong record
				 i.e. the one after the last write)

