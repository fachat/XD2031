
REL file tests
==============

The remaining errors are:

make
./relfiles.sh -C -q
reltest1.2-4040.frs: Ok
reltest2.4-2031.frs: Ok
reltest2.4-4040.frs: Ok
reltest3.c-2031.frs: errors: 8
	=> 2031 REL files considered broken!
reltest3.c-4040.frs: Ok
reltest4.7-2031.frs: errors: 9
	=> 2031 REL files considered broken!
reltest4.7-4040.frs: Ok
reltest5.2-2031.frs: errors: 2
	=> 2031 REL files considered broken!
reltest5.2-4040.frs: Ok
reltest6.1-4040.frs: Ok
reltest6.2-4040.frs: Ok
reltest6.3-4040.frs: Ok
reltest6.4-4040.frs: Ok
reltest6.5-4040.frs: Ok
reltest6.6-4040.frs: Ok

reltest1.2-8050.frs: errors: 1
reltest1.2-8050.frs: File rel.d80 differs!
	=> DOS bug that shows a REL file with zero blocks

reltest2.4-8050.frs: Ok
reltest2.4-8050.frs: File rel.d80 differs!
	=> DOS bug that shows a REL file with zero blocks

reltest6.5-8050.frs: Ok
reltest6.5-8050.frs: File rel.d80 differs!
	=> block allocation? (different between 8050 and 8250/1001)

reltest6.6-8050.frs: Ok
reltest6.6-8050.frs: File rel.d80 differs!
	=> block allocation? (different between 8050 and 8250/1001)

reltest6.7-8050.frs: Ok
reltest6.7-8050.frs: File rel.d80 differs!
	=> block allocation? (different between 8050 and 8250/1001)

reltest1.2-1001.frs: errors: 1
reltest1.2-1001.frs: File rel.d82 differs!
	=> DOS bug that shows a REL file with zero blocks

reltest2.4-1001.frs: Ok
reltest2.4-1001.frs: File rel.d82 differs!
	=> DOS bug that shows a REL file with zero blocks

reltest6.7-1001.frs: File rel.d82 differs!
	=> Unknown? DOS bug that allocates another data block without noting it in the side sector?


