#!/bin/sh

VERBOSE=""
RVERBOSE=""
DEBUG=""
CLEAN=0

while test $# -gt 0; do 
  case $1 in 
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
  -c)
	CLEAN=1
	shift;
	;;
  -C)
	CLEAN=2
	shift;
	;;
  *)
	break;
	;;
  esac;
done;


# scripts to run
if [ "x$*" = "x" ]; then
        TESTSCRIPTS=$THISDIR/*.trs
        TESTSCRIPTS=`basename -a $TESTSCRIPTS`;
else
        TESTSCRIPTS=$@;
fi;

echo "TESTSCRIPTS=$TESTSCRIPTS"


########################
# tmp names
#

TMPDIR=`mktemp -d`

RUNNER="$THISDIR"/../testrunner

SOCKETBASE="$TMPDIR"/socket

SERVER="$THISDIR"/../../pcserver/fsser

DEBUGFILE="$TMPDIR"/gdb.ex

echo THISDIR=$THISDIR

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

	SOCKET=${SOCKETBASE}_$script

	# overwrite test files in each iteration, just in case
	for i in $TESTFILES; do
		cp "$THISDIR/$i" "$TMPDIR"
	done;

	# start server

	echo "Start server as:" $SERVER -s $SOCKET $VERBOSE $SERVEROPTS $TMPDIR 

	if test "x$DEBUG" = "x"; then
		$SERVER -s $SOCKET $VERBOSE $SERVEROPTS $TMPDIR > $TMPDIR/$script.log 2>&1 &
		SERVERPID=$!
		trap "kill -TERM $SERVERPID" INT

		# start testrunner after server, so we get the return value in the script
		$RUNNER $RVERBOSE -w -d $SOCKET $script;

		RESULT=$?
		echo "result: $RESULT"

		if test $RESULT -ne 0; then
			echo "Resetting clean to keep files!"
			CLEAN=$(( $CLEAN - 1 ));
			echo "CLEAN=$CLEAN"
		fi;
	else
		# start testrunner before server and in background, so gdb can take console
		$RUNNER $RVERBOSE -w -d $SOCKET $script &
		SERVERPID=$!
		trap "kill -TERM $SERVERPID" INT

		echo > $DEBUGFILE;
		for i in $DEBUG; do
			echo "break $i" >> $DEBUGFILE
		done;
		gdb -x $DEBUGFILE -ex "run -s $SOCKET $VERBOSE $SERVEROPTS $TMPDIR" $SERVER
	fi;

	#echo "Killing server (pid $SERVERPID)"
	#kill -TERM $SERVERPID

	rm -f $SOCKET $DEBUGFILE;
	if test $CLEAN -ge 1; then
		rm -f $TMPDIR/$script;
	fi;
done;

if test $CLEAN -ge 2; then

	for script in $TESTSCRIPTS; do
		rm -f $TMPDIR/$script.log
	done;

	for i in $TESTFILES; do
		rm -f $TMPDIR/$i;
	done;
	
	rmdir $TMPDIR
fi;

