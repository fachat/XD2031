
Notes
=====

Note that shell-d64.sh tests the same test cases as base-d64.sh, but using the "inner.d64" 
disk image stored in the shell.d80 image. This tests the correctness of the cascaded mounts.

Note that assign1 and assign2 will probably change in the future, as they re-assign the 
same disk images already used in drive 0 to drive 1

Remaining errors
================

make
./base.sh -C -q
base1-4040.frs: errors: 3
	=> this is expected, as it is a test of error detection in test runner

