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
#
# Note that these files are interpreted to different names:
# F1,p 		-> F1		PRG
# P2U.P00	-> F5		PRG
# F3.S00	-> f3		SEQ
# F4,S		-> F4		SEQ
# T1.U00	-> T1		USR
# T2,u		-> T2		USR
# REL2.R00	-> Rel2		REL
# Rel1,l20	-> Rel1		REL
#
TESTFILES="F1,p P2U.P00 F3.S00 F4,S T1.U00 T2,u REL2.R00 Rel1,l20"

# files to compare after test iff files like <file>-<test> exist
# e.g. if there is a file "rel1.d64" and a test "position2.trs",
# then after the test rel1.d64 is compared to "rel1.d64-position2" iff it exists
COMPAREFILES=""

# server options
SERVEROPTS="-v -A0:fs=."

# tsr scripts from the directory to exclude
#EXCLUDE="position1.trs"
EXCLUDE=""

########################
# source and execute actual functionality
. ../func.sh

