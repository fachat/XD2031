#!/bin/bash

BASEDIR="../.."

#hardcode firmware binary for now
#VERSION?=$(shell date +"%Y-%m-%d")

SWNAME=xd2031
VERSION=`date +"%Y-%m-%d"`
ZOOBINDIR=${SWNAME}-firmware-${VERSION}
THISBINDIR=${ZOOBINDIR}/${SWNAME}-${VERSION}-sockserv-pc
BINNAME=${SWNAME}-${VERSION}-sockserv-pc.elf

FIRMWARE=${BASEDIR}/firmware/${THISBINDIR}/${BINNAME}

RUNNER="$THISDIR"/../../testrunner/fwrunner

SERVER="$THISDIR"/${BASEDIR}/pcserver/fsser

# make sure we have a firmware
#(cd ${BASEDIR}/firmware; DEVICE=sockserv make)
# make sure we have a testrunner
#(cd ${BASEDIR}/testrunner; make fwrunner)


function usage() {
	echo "Running *.frs test runner scripts"
	echo "  $0 [options] [frs_scripts]"
	echo "Options:"
	echo "       -v                      verbose server log"
	echo "       -o                      server log on console"
	echo "       -d <breakpoint>         run server with gdb and set given breakpoint. Can be "
	echo "                               used multiple times"
	echo "       -V                      verbose runner log"
	echo "       -D <breakpoint>         run firmware with gdb and set given breakpoint. Can be "
	echo "                               used multiple times"
	echo "       -k                      keep all files, even non-log and non-data files in run directory"
	echo "       -c                      clean up only non-log and non-data files from run directory,"
	echo "                               keep rest (default)"
	echo "       -C                      always clean up complete run directory"
	echo "       -R <run directory>      use given run directory instead of tmp folder (note:"
	echo "                               will not be rmdir'd on -C"
	echo "       -q                      will suppress any output except whether test was successful"
	echo "       +e                      create an expected DIFF that is compared to later outcomes"
	echo "       -e                      ignore an expected DIFF and show the real results"
	echo "       +E                      create an expected ERR file that is compared to later outcomes"
	echo "       -E                      ignore an expected ERR file and show the real results"
	echo "       -h                      show this help"
}

function hexdiff() {
	diffres=0
	tmp3="$3".expected
	if ! cmp -b "$1" "$2"; then
                tmp1="$1".hex;  #`mktemp`
                tmp2="$2".hex;  #`mktemp`
		tmp4="$3".actual

		hexdump -C "$1" > $tmp1
		hexdump -C "$2" > $tmp2

		#diff -u $tmp1 $tmp2 | sed -e 's%/tmp/tmp\.[a-zA-Z0-9]\+%%g' > $tmp4 # actual
		echo "--- $tmp1" | sed -e 's%/tmp/tmp\.[a-zA-Z0-9]\+%%g' > $tmp4 # actual
		echo "+++ $tmp2" | sed -e 's%/tmp/tmp\.[a-zA-Z0-9]\+%%g' >> $tmp4 # actual
		diff -u $tmp1 $tmp2 | tail -n +3 >> $tmp4 # actual
		diffres=1	# (as cmp above already told us we're different)
		if [ $DIFFCREATE -eq 1 ]; then 
			cp $tmp4 $tmp3
		fi
		if [ $DIFFIGNORE -eq 1 -o ! -f $tmp3 ]; then
			cat $tmp4;	# actual
		else
			# compare actual with expected
			diff -u $tmp3 $tmp4 
			diffres=$?
                        if test $diffres -eq 0; then
				diffres=2
				echo "Comparing $1 with $2 gave the expected difference"
			fi
		fi;

		rm $tmp1 $tmp2 $tmp4
	else
		# if expected file exists, but no more error, then warn
		if [ -f $tmp3 ]; then
			echo "WARN: no difference found, but expected errors file $tmp3 exists"
			diffres=3
		fi;
	fi;
	return $diffres
}

VERBOSE=""
RVERBOSE=""
DEBUG=""
RDEBUG=""
CLEAN=1
QUIET=0
LOGFILE=""

DIFFCREATE=0
DIFFIGNORE=0

ERRCREATE=0
ERRIGNORE=0

TMPDIR=`mktemp -d`
OWNDIR=1	

while test $# -gt 0; do 
  case $1 in 
  -h)
	usage
	exit 0;
	;;
  -o)
	LOGFILE="-"
	shift;
	;;
  -v)
	VERBOSE="-v"
	shift;
	;;
  -V)
	RVERBOSE="-t"
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
  -k)
	CLEAN=0
	shift;
	;;
  -c)
	CLEAN=1
	shift;
	;;
  -C)
	CLEAN=2
	shift;
	;;
  -q)
	QUIET=1
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
  +e)
	DIFFCREATE=1
	shift;
	;;
  -e)
	DIFFIGNORE=1
	shift;
	;;
  +E)
	ERRCREATE=1
	shift;
	;;
  -E)
	ERRIGNORE=1
	shift;
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

########################
# test for executables

ERR=0
if test ! -e $RUNNER; then
       echo "$RUNNER does not exist! Maybe forgot to compile?"
	ERR=1
fi
if test ! -e $SERVER; then
       echo "$SERVER does not exist! Maybe forgot to compile?"
	ERR=1
fi
if test ! -e $FIRMWARE; then
       echo "$FIRMWARE does not exist! Maybe forgot to compile?"
	ERR=1
fi
if [ $ERR -ge 1 ]; then
	echo "Aborting!"
	exit 1;
fi

########################

# scripts to run
if [ "x$*" = "x" ]; then
        SCRIPTS=$THISDIR/*${FILTER}*.frs
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
        TESTSCRIPTS="";

        for i in "$@"; do
               if test -f "$i".frs ; then
                       	TESTSCRIPTS="$TESTSCRIPTS $i.frs";
               else
			if test -f "$i"-"${FILTER}".frs; then
                       		TESTSCRIPTS="$TESTSCRIPTS $i-${FILTER}.frs";
			else 
                       		TESTSCRIPTS="$TESTSCRIPTS $i";
			fi
               fi
        done;
fi;

#echo "TESTSCRIPTS=$TESTSCRIPTS"



########################
# tmp names
#


DEBUGFILE="$TMPDIR"/gdb.ex


########################
# stdout

# remember stdout for summary output
exec 5>&1

# redirect log when quiet
if test $QUIET -ge 1 ; then 
       exec 1>$TMPDIR/stdout.log
fi

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

	echo "====================== Running script $script" >&5

	SSOCKET=ssocket_$script
	CSOCKET=csocket_$script
	RUNNERLOG=runnerlog_$script

	# overwrite test files in each iteration, just in case
	for i in $TESTFILES; do
		if [ -f ${THISDIR}/${i}.gz ]; then
			gunzip -c ${THISDIR}/${i}.gz >  ${TMPDIR}/${i}
		else
			cp ${THISDIR}/${i} ${TMPDIR}/${i}
		fi;
	done;

	# start server


	if test "x$DEBUG" = "x"; then
		############################################
		# start server

		echo "Start server as:" $SERVER -s $SSOCKET $VERBOSE $SERVEROPTS $TMPDIR 
		if test "x$LOGFILE" = "x-"; then
			$SERVER -s $SSOCKET $VERBOSE $SERVEROPTS $TMPDIR &
		else
			$SERVER -s $SSOCKET $VERBOSE $SERVEROPTS $TMPDIR > $TMPDIR/$script.log 2>&1 &
		fi
		SERVERPID=$!

		# wait till server is up, just to be sure
		sleep 0.5;

		############################################
		# start firmware
		echo "Starting firmware as: $FIRMWARE $FWOPTS -S $TMPDIR/$SSOCKET -C $TMPDIR/$CSOCKET"

		did_print_message=0;

		# start testrunner after server/firmware, so we get the return value in the script
		if test "x$RDEBUG" != "x"; then

			# start test runner before server, so we can use gdb on firmware
			echo "Starting runner as: $RUNNER $RVERBOSE -w -d $TMPDIR/$CSOCKET $script"
			$RUNNER $RVERBOSE -w -d $TMPDIR/$CSOCKET $script &
			RUNNERPID=$!
			trap "kill -TERM $SERVERPID $RUNNERPID" INT

			echo > $DEBUGFILE;
			for i in $RDEBUG; do
				echo "break $i" >> $DEBUGFILE
			done;
			gdb -x $DEBUGFILE -ex "run $FWOPTS -S $TMPDIR/$SSOCKET -C $TMPDIR/$CSOCKET" $FIRMWARE

			RESULT=-1
		else
			$FIRMWARE $FWOPTS -S $TMPDIR/$SSOCKET -C $TMPDIR/$CSOCKET &
			FWPID=$!
			trap "kill -TERM $SERVERPID $FWPID" INT

			sleep 0.5; # just in case

			echo "Starting runner as: $RUNNER $RVERBOSE -w -d $TMPDIR/$CSOCKET $script"
			$RUNNER $RVERBOSE -w -d $TMPDIR/$CSOCKET $script | sed -e "s%$TMPDIR%%g" | tee $TMPDIR/$RUNNERLOG;
			RESULT=${PIPESTATUS[0]}
			#gdb -ex "break main" -ex "run $RVERBOSE -w -d $TMPDIR/$CSOCKET $script" $RUNNER
			#RESULT=$?
			
			if [ $ERRCREATE -eq 1 ]; then
				echo "creating expected errors file at $THISDIR/${script}_expected"
				cp $TMPDIR/$RUNNERLOG $THISDIR/${script}_expected
			fi
			if [ $ERRIGNORE -eq 0 ]; then
				if [ -f $THISDIR/${script}_expected ]; then
					# expected file exists
					cmp $TMPDIR/$RUNNERLOG $THISDIR/${script}_expected
					if [ $? -ne 0 ]; then
						echo ">>> Expected errors differ!" >&5
					else 
						echo ">   Errors occured as expected!" >&5
					fi
					did_print_message=1;
				fi
			fi;
		fi;

		if test $did_print_message -eq 0; then
	                if test $RESULT -eq 0; then
        	                echo "    Script Ok" >&5
                	else
                	        echo ">>> Script errors: $RESULT" >&5
                	fi
		fi

		#kill -TERM $RUNNERPID $SERVERPID

		#if test $RESULT -ne 0; then
		#	echo "Resetting clean to keep files!"
		#	CLEAN=$(( $CLEAN - 1 ));
		#	echo "CLEAN=$CLEAN"
		#fi;
	else
		# start testrunner before server and in background, so gdb can take console
		$RUNNER $RVERBOSE -w -d $TMPDIR/$CSOCKET $script &
		$FIRMWARE $FWOPTS -S $TMPDIR/$SSOCKET -C $TMPDIR/$CSOCKET &
		SERVERPID=$!
		trap "kill -TERM $SERVERPID" INT

		echo > $DEBUGFILE;
		for i in $DEBUG; do
			echo "break $i" >> $DEBUGFILE
		done;
		gdb -x $DEBUGFILE -ex "run -s $SSOCKET $VERBOSE $SERVEROPTS $TMPDIR" $SERVER
	fi;

	#echo "Killing server (pid $SERVERPID)"
	#kill -TERM $SERVERPID

	if test "x$COMPAREFILES" != "x"; then
		testname=`basename $script .frs`
		for i in $COMPAREFILES; do 
			NAME="${THISDIR}/${i}-${testname}"
			result=0;
			if test -f ${NAME}; then
				echo "Comparing file ${i} with ${NAME}"
				hexdiff ${NAME} $TMPDIR/${i} ${NAME}
				result=$?
			else
			    if test -f ${NAME}.gz; then
                                echo "Comparing file ${i} with ${NAME}.gz"
                                gunzip -c ${NAME}.gz > ${TMPDIR}/shouldbe_${i}
                                hexdiff ${TMPDIR}/shouldbe_${i} ${TMPDIR}/${i} ${NAME}
				result=$?
                                rm -f ${TMPDIR}/shouldbe_${i}
			    fi
			fi
                        if test $result -eq 1; then
                                 echo ">>> File ${i} differs!" >&5
                        elif test $result -eq 2; then
                                 echo ">   File ${i} differs (as expected)!" >&5
                        elif test $result -eq 3; then
                                 echo ">>> File ${i} does not differ, but diff was expected!" >&5
			else
                                 echo "    File ${i} compare ok!" >&5
		 	fi
		done;
	fi

	if test $CLEAN -ge 1; then
		rm -f $TMPDIR/$SSOCKET $TMPDIR/$CSOCKET $DEBUGFILE;
		rm -f $TMPDIR/$script;
	fi

done;

if test $CLEAN -ge 2; then
	echo "Cleaning up directory $TMPDIR"

	for script in $TESTSCRIPTS; do
		rm -f $TMPDIR/$script.log $TMPDIR/runnerlog_$script
	done;

	# gzipped test files are unzipped
	for i in $TESTFILES; do
		rm -f $TMPDIR/$i;
	done;

        rm -f $TMPDIR/stdout.log  

        # only remove work dir if we own it (see option -R)
	if test $OWNDIR -ge 1; then	
		rmdir $TMPDIR
	fi;
else 
	echo "Find debug info in $TMPDIR" >&5
fi;

