#!/bin/bash
#
# call this script without params to run all *.trs tests in this directory
# Providing a .trs file as parameter only runs the given test script
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

# necessary files to copy to temp
TESTFILES="empty.d64"

# files to compare after test iff files like <file>-<test> exist
# e.g. if there is a file "rel1.d64" and a test "position2.trs",
# then after the test rel1.d64 is compared to "rel1.d64-position2" iff it exists
COMPAREFILES=""

# server options
SERVEROPTS="-v -A0:fs=empty.d64"

# tsr scripts from the directory to exclude
#EXCLUDE="position1.trs"
EXCLUDE=""

########################
# source and execute actual functionality
. ../func.sh

