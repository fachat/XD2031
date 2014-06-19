#!/bin/sh
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

echo "THISDIR=$THISDIR"

# necessary files to copy to temp
TESTFILES="rel1.d64"

# server options
SERVEROPTS="-v -A0:fs=rel1.d64"


########################
# source and execute actual functionality
. ../func.sh

