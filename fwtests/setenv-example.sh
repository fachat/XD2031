#!/bin/bash

if [ "$_" == "" ]; then
	echo "please source this file, do not execute it!"
	exit 1
fi

PWD="`pwd`"
THISDIR=`dirname "${PWD}/${BASH_SOURCE}"`



# if using VICE without installation, use VICESRC to point to the installation directory
VICESRC="${THISDIR}/../../vice-2.4"

# if using an installed VICE, but replacing the test run binary with the patched version,
# use VICE to point to the binary
# VICE="${THISDIR}/../../vice-2.4/src

if [ "x${VICESRC}" != "x" ]; then
	VICE=${VICESRC}/src/
	VICEDATA="${VICESRC}/data"
else 
	VICEPAR=""
fi

export VICE
export VICEDATA

