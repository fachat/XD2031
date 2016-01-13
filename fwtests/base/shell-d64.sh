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

# necessary files to copy to temp
#TESTFILES="rel1.d64"
TESTFILES="shell.d80 base.d64"

# custom command to be executed after a test, but before the compare
# Here used to extract the inner D64 from the shell D80 file
CUSTOMPOSTCMD="$VICE/c1541 -attach shell.d80 -read inner.d64 base.d64"

# files to compare after test iff files like <file>-<test> exist
# e.g. if there is a file "rel1.d64" and a test "position2.frs",
# then after the test rel1.d64 is compared to "rel1.d64-position2" iff it exists
#COMPAREFILES="rel1.d64"
COMPAREFILES="base.d64 shell.d80"

# server options
#SERVEROPTS="-v -A0:fs=rel1.d64"
SERVEROPTS="-v -A0:fs=shell.d80/inner.d64"

#firmware options
#FWOPTS=""
FWOPTS=-Xsock488:E=-


# tsr scripts from the directory to exclude
#EXCLUDE="position1.frs"
EXCLUDE="*-1001.frs"

FILTER=

########################
# source and execute actual functionality
. ../func.sh

