#!/bin/bash
#
# call this script without params to run all *.trs tests in this directory
# Providing a .frs file as parameter only runs the given test script
#
# Available options are:
# 	-v 			verbose server log
#	-V			verbose runner log
#	-d <breakpoint>		run server with gdb and set given breakpoint. Can be 
#				used multiple times
#	-c			clean up non-log and non-data files from run directory
#	-C			clean up complete run directory
#	-R <run directory>	use given run directory instead of tmp folder (note:
#				will not be rmdir'd on -C
#

THISDIR=`dirname $0`

#echo "THISDIR=$THISDIR"

# necessary files to copy to temp
#TESTFILES="rel1.d64"
TESTFILES="blk.d80"

# files to compare after test iff files like <file>-<test> exist
# e.g. if there is a file "rel1.d64" and a test "position2.frs",
# then after the test rel1.d64 is compared to "rel1.d64-position2" iff it exists
#COMPAREFILES="rel1.d64"
COMPAREFILES="blk.d80"

# server options
#SERVEROPTS="-v -A0:fs=rel1.d64"
SERVEROPTS="-v -A0:=fs:blk.d80"

#firmware options
# switch off drive in error messages; also restricts track/sector to two chars
#FWOPTS="-Xsock488:E=-"
FWOPTS=

# tsr scripts from the directory to exclude
# Note that the 2031 drive is "worse" in a sense that some useful features are
# only in the dual drives. Like 12 direct buffers (as sockserv), or track/sector numbers
# > 100 being handled correctly in the error message. So as reference we only use the 4040 
# tests
#EXCLUDE="position1.frs"
EXCLUDE=""
#shopt -s extglob
#FILTER='+(2031|4040)'
FILTER='8050'

########################
# source and execute actual functionality
. ../func.sh

