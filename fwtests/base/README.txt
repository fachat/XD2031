
Remaining errors
================

make
./base.sh -C -q
assign1-4040.frs: Ok
assign2-4040.frs: Ok
base1-4040.frs: errors: 3
	=> this is expected, as it is a test of error detection in test runner
cmdchan1-4040.frs: Ok
cmdchan1-4040.frs: File base.d64 differs!
	=> opening a file for write, and closing cmd channel seems to temporarily create a block
	   in 4040 CBM DOS (not in 1001/8250 DOS, see below)
xunit-4040.frs: Ok
./base-d82.sh -C -q
cmdchan1-1001.frs: Ok

