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

send 03 .len 02 00 52 45 4c 31 00 54 3d 4c 31 32 37 00
expect 07 .len 00

EOF


########################
# start server
#

echo "Start server as:" $SERVER -d $SOCKET -A0:fs=$TMPIMG $TMPDIR 

#$SERVER -d $SOCKET -A0:fs=$TMPIMG $TMPDIR > $SERVERLOG 2>&1 &
$SERVER -d $SOCKET -A0:fs=$TMPIMG $TMPDIR  &
SERVERPID=$!

trap "kill -TERM $SERVERPID" INT

echo "SERVERPID=$SERVERPID"

########################
# start testrunner

gdb -ex "break parse_buf" -ex "break execute_script" -ex "run -D -d $SOCKET $SCRIPT" $RUNNER

echo "Killing server (pid $SERVERPID)"
kill -TERM $SERVERPID

