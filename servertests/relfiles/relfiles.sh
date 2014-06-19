#!/bin/sh
#
# call this script without params to run all *.trs tests in this directory
# Providing a .trs file as parameter only runs the given test script
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

