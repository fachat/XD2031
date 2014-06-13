#!/bin/sh


########################
# tmp names
#

TMPDIR=`mktemp -d`

TMPIMG="$TMPDIR"/img.d64

THISDIR=`dirname $0`

RUNNER="$THISDIR"/../testrunner

SOCKET="$TMPDIR"/socket

SERVER="$THISDIR"/../../pcserver/fssock

SERVERLOG="$TMPDIR"/server.log

SCRIPT="$TMPDIR"/script

echo TMPDIR=$TMPDIR
echo THISDIR=$THISDIR

########################
# prepare files
#

cp "$THISDIR/rel1.d64" "$TMPIMG"

cat > $SCRIPT << EOF
#
init

message hello world

# open REL file
send 03 .len 02 00 52 45 4c 31 00 54 3d 4c 31 32 37 00
expect 09 .len 02 02 7f 00
#expect 08 .len 02 02 7f 00

EOF


########################
# start server
#

echo "Start server as:" $SERVER -d $SOCKET -A0:fs=$TMPIMG $TMPDIR 

#$SERVER -d $SOCKET -A0:fs=$TMPIMG $TMPDIR > $SERVERLOG 2>&1 &
$SERVER -v -d $SOCKET -A0:fs=$TMPIMG $TMPDIR  &
SERVERPID=$!

trap "kill -TERM $SERVERPID" INT

echo "SERVERPID=$SERVERPID"

echo "Waiting for socket"
while test ! -S $SOCKET; do sleep 1s; done;

########################
# start testrunner

#gdb -ex "break parse_buf" -ex "break execute_script" -ex "run -D -d $SOCKET $SCRIPT" $RUNNER
$RUNNER -D -d $SOCKET $SCRIPT

echo "result: $?"

echo "Killing server (pid $SERVERPID)"
kill -TERM $SERVERPID

