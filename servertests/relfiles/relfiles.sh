#!/bin/sh

THISDIR=`dirname $0`

# necessary files to copy to temp
TESTFILES="rel1.d64"

# server options
SERVEROPTS="-v -A0:fs=rel1.d64"

# scripts to run
if [ "x$*" = "x" ]; then
	TESTSCRIPTS="open_l127.trs open_l0.trs";
else
	TESTSCRIPTS=$@;
fi;

########################
# source and execute actual functionality
. ../func.sh

