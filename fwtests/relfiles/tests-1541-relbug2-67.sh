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
TESTFILES="relbug2-67.d64"

# files to compare after test iff files like <file>-<test> exist
# e.g. if there is a file "rel1.d64" and a test "position2.frs",
# then after the test rel1.d64 is compared to "rel1.d64-position2" iff it exists
#COMPAREFILES="rel1.d64"
COMPAREFILES="relbug2-67.d64"

# server options
#SERVEROPTS="-v -A0:fs=rel1.d64"
SERVEROPTS="-v -A0:fs=relbug2-67.d64"

#firmware options
FWOPTS=-Xsock488:E=-

# tsr scripts from the directory to exclude
#EXCLUDE="position1.frs"
#EXCLUDE="relbug2-1541-6b.frs relbug2-1541-69.frs"
EXCLUDE=""
shopt -s extglob
FILTER='relbug2-1541-*'

DRIVETYPE=1541

########################
# source and execute actual functionality
. ../func.sh

