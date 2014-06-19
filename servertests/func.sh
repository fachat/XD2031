#!/bin/sh


########################
# tmp names
#

TMPDIR=`mktemp -d`

RUNNER="$THISDIR"/../testrunner

SOCKETBASE="$TMPDIR"/socket

SERVER="$THISDIR"/../../pcserver/fsser

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

	echo "Start server as:" $SERVER -s $SOCKET $SERVEROPTS $TMPDIR 

	$SERVER -s $SOCKET $SERVEROPTS $TMPDIR > $TMPDIR/$script.log 2>&1 &
	SERVERPID=$!

	trap "kill -TERM $SERVERPID" INT

	echo "SERVERPID=$SERVERPID"

	echo "Waiting for socket $SOCKET"
	while test ! -S $SOCKET; do sleep 1s; done;

	########################
	# start testrunner

	#gdb -ex "break parse_buf" -ex "break execute_script" -ex "run -D -d $SOCKET $script" $RUNNER
	#$RUNNER -D -d $SOCKET $script
	$RUNNER -d $SOCKET $script

	RESULT=$?
	echo "result: $RESULT"

	echo "Killing server (pid $SERVERPID)"
	kill -TERM $SERVERPID

done;
