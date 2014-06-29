#!/bin/bash

#hardcode firmware binary for now
#VERSION?=$(shell date +"%Y-%m-%d")

SWNAME=xd2031
VERSION=`date +"%Y-%m-%d"`
ZOOBINDIR=${SWNAME}-firmware-${VERSION}
THISBINDIR=${ZOOBINDIR}/${SWNAME}-${VERSION}-sockserv-pc
BINNAME=${SWNAME}-${VERSION}-sockserv-pc.elf

FIRMWARE=../firmware/${THISBINDIR}/${BINNAME}

function usage() {
	echo "Running *.trs test runner scripts"
	echo "  $0 [options] [trs_scripts]"
	echo "Options:"
	echo "       -v                      verbose server log"
	echo "       -d <breakpoint>         run server with gdb and set given breakpoint. Can be "
	echo "                               used multiple times"
	echo "       -V                      verbose runner log"
	echo "       -D <breakpoint>         run testrunner with gdb and set given breakpoint. Can be "
	echo "                               used multiple times"
	echo "       -c                      clean up non-log and non-data files from run directory"
	echo "       -C                      clean up complete run directory"
	echo "       -R <run directory>      use given run directory instead of tmp folder (note:"
	echo "                               will not be rmdir'd on -C"
	echo "       -h                      show this help"
}

function hexdiff() {
	if ! cmp -b "$1" "$2"; then
		tmp1=`mktemp`
		tmp2=`mktemp`

		hexdump -C "$1" > $tmp1
		hexdump -C "$2" > $tmp2

		diff -u $tmp1 $tmp2

		rm $tmp1 $tmp2
	fi;
}

VERBOSE=""
RVERBOSE=""
DEBUG=""
RDEBUG=""
CLEAN=0

TMPDIR=`mktemp -d`
OWNDIR=1	

while test $# -gt 0; do 
  case $1 in 
  -h)
	usage
	exit 0;
	;;
  -v)
	VERBOSE="-v"
	shift;
	;;
  -V)
	RVERBOSE="-v"
	shift;
	;;
  -d)
	if test $# -lt 2; then
		echo "Option -d needs the break point name for gdb as parameter"
		exit -1;
	fi;
	DEBUG="$DEBUG $2"
	shift 2;
	;;
  -D)
	if test $# -lt 2; then
		echo "Option -D needs the break point name for gdb as parameter"
		exit -1;
	fi;
	RDEBUG="$RDEBUG $2"
	shift 2;
	;;
  -c)
	CLEAN=1
	shift;
	;;
  -C)
	CLEAN=2
	shift;
	;;
  -R)	
	if test $# -lt 2; then
		echo "Option -R needs the directory path as parameter"
		exit -1;
	fi;
	TMPDIR="$2"
	OWNDIR=0
	shift 2;
	;;
  -?)
	echo "Unknown option $1"
	usage
	exit 1;
	;;
  *)
	break;
	;;
  esac;
done;

function contains() {
	local j
	for j in "${@:2}"; do test "$j" == "$1"  && return 0; done;
	return 1;
}

# scripts to run
if [ "x$*" = "x" ]; then
        SCRIPTS=$THISDIR/*.trs
        SCRIPTS=`basename -a $SCRIPTS`;

	TESTSCRIPTS=""

	if test "x$EXCLUDE" != "x"; then
		exarr=( $EXCLUDE )
		scrarr=( $SCRIPTS )
		for scr in "${scrarr[@]}"; do 
			if ! contains "${scr}" "${exarr[@]}"; then
				TESTSCRIPTS="$TESTSCRIPTS $scr";
			fi
		done;
	else
		TESTSCRIPTS="$SCRIPTS"
	fi;
else
        TESTSCRIPTS="$@";
fi;

echo "TESTSCRIPTS=$TESTSCRIPTS"

########################
# tmp names
#


RUNNER="$THISDIR"/../testrunner

#SERVER="$THISDIR"/../../pcserver/fsser
SERVER="$THISDIR"/../pcserver/fsser

DEBUGFILE="$TMPDIR"/gdb.ex

########################
# prepare files
#

for i in $TESTSCRIPTS; do
	cp "$THISDIR/$i" "$TMPDIR"
done;


########################
# run scripts
#

for script in $TESTSCRIPTS; do

	echo "Run script $script"

	SOCKET=socket_$script

	# overwrite test files in each iteration, just in case
	for i in $TESTFILES; do
		cp "$THISDIR/$i" "$TMPDIR"
	done;

	# start server

	echo "Start server as:" $SERVER -s $SOCKET $VERBOSE $SERVEROPTS $TMPDIR 
	echo "DEBUG=$DEBUG"

	if test "x$DEBUG" = "x"; then
		$SERVER -s $SOCKET $VERBOSE $SERVEROPTS $TMPDIR > $TMPDIR/$script.log 2>&1 &
		SERVERPID=$!
		trap "kill -TERM $SERVERPID" INT

		# start firmware
		#gdb -ex "break socket_open" -ex "run -S $TMPDIR/$SOCKET" $FIRMWARE
		$FIRMWARE -S $TMPDIR/$SOCKET

#		# start testrunner after server, so we get the return value in the script
#		if test "x$RDEBUG" != "x"; then
#			echo > $DEBUGFILE;
#			for i in $RDEBUG; do
#				echo "break $i" >> $DEBUGFILE
#			done;
#			gdb -x $DEBUGFILE -ex "run $RVERBOSE -w -d $TMPDIR/$SOCKET $script " $RUNNER
#		else
#			$RUNNER $RVERBOSE -w -d $TMPDIR/$SOCKET $script;
#		fi;

		RESULT=$?
		echo "result: $RESULT"

		if test $RESULT -ne 0; then
			echo "Resetting clean to keep files!"
			CLEAN=$(( $CLEAN - 1 ));
			echo "CLEAN=$CLEAN"
		fi;
	else
		# start testrunner before server and in background, so gdb can take console
		#$RUNNER $RVERBOSE -w -d $TMPDIR/$SOCKET $script &
		$FIRMWARE -S $TMPDIR/$SOCKET &
		SERVERPID=$!
		trap "kill -TERM $SERVERPID" INT

		echo > $DEBUGFILE;
		for i in $DEBUG; do
			echo "break $i" >> $DEBUGFILE
		done;
		echo "running with debug:"
		cat $DEBUGFILE
		gdb -x $DEBUGFILE -ex "run -s $SOCKET $VERBOSE $SERVEROPTS $TMPDIR" $SERVER
	fi;

	#echo "Killing server (pid $SERVERPID)"
	#kill -TERM $SERVERPID

	if test "x$COMPAREFILES" != "x"; then
		testname=`basename $script .trs`
		for i in $COMPAREFILES; do 
			if test -f $THISDIR/${i}-${testname}; then
				hexdiff $THISDIR/${i}-${testname} $TMPDIR/${i}
			fi
		done;
	fi

	rm -f $TMPDIR/$SOCKET $DEBUGFILE;
	rm -f $TMPDIR/$script;
done;

if test $CLEAN -ge 2; then
	echo "Cleaning up directory $TMPDIR"

	for script in $TESTSCRIPTS; do
		rm -f $TMPDIR/$script.log
	done;

	for i in $TESTFILES; do
		rm -f $TMPDIR/$i;
	done;

	if test $OWNDIR -ge 1; then	
		rmdir $TMPDIR
	fi;
else 
	echo "Find debug info in $TMPDIR"
fi;

