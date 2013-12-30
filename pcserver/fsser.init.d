#!/bin/bash
#
# Copy this file to /etc/init.d/fsser
#
### BEGIN INIT INFO
# Provides:          fsser
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: XD-2031 Server
# Description:       github.com/fachat/XD2031
#
### END INIT INFO

# Author: Nils Eilers <nils.eilers@gmx.de>

LOGFILE=/var/log/fsser
ME=$0

start_daemon() {
    # Make sure, the logfile exists and the user can access it
    touch $LOGFILE || exit 1
    chown $RUN_AS_USER $LOGFILE || exit 1
    chgrp $RUN_AS_USER $LOGFILE || exit 1
    if [ $RUN_AS_USER = `whoami` ] ; then
        PREFIX/BINDIR/fsserd &>> $LOGFILE &
    else
        su - $RUN_AS_USER -c "PREFIX/BINDIR/fsserd &>> $LOGFILE" &>> $LOGFILE &
    fi
}

# Read configuration variable file if it is present
# RUN_AS_USER -- this user will run the server and the daemon
# If RUN_AS_USER is undefined, abort
#
[ -r /etc/default/fsser ] && . /etc/default/fsser
if [ -z $RUN_AS_USER ] ; then
  echo "/etc/default/fsser: RUN_AS_USER undefined!"
  exit 1
fi

case "$1" in
start)
if pidof PREFIX/BINDIR/fsser > /dev/null ; then
  echo "Server already running."
else
  if [ `pgrep -c .*fsserd` -gt 1 ] ; then
    echo "Daemon already running, will start the server when the configured device is available."
  else
    echo "Starting XD-2031 daemon..."
    start_daemon
  fi
fi
;;
stop)
if pidof PREFIX/BINDIR/fsser > /dev/null ; then
  echo "Killing XD-2031 server..."
  kill `pidof PREFIX/BINDIR/fsser` || exit 1
  sleep 1
else
  echo "No running server found."
fi
# Kill all daemons, start with the oldest.
while [ `pgrep -c .*fsserd` -gt 0 ] ; do
  kill `pgrep -o .*fsserd` || exit 1
done
echo Daemon stopped
;;
restart)
echo "Restarting XD-2031 Server..."
# Kill any servers. If one was running, this will end the daemon as well
if pidof PREFIX/BINDIR/fsser > /dev/null ; then
  kill `pidof PREFIX/BINDIR/fsser`
  sleep 1
fi
# If there is a daemon left, he can not start a server
# and probably has a bad configuration.
# That's why old daemons are killed as well to force
# re-reading the configuration
while [ `pgrep -c .*fsserd` -gt 0 ] ; do
  kill `pgrep -o .*fsserd`
done
echo "Starting XD-2031 daemon..."
start_daemon
;;
status)
if pidof PREFIX/BINDIR/fsser > /dev/null ; then
  echo Server running: PID `pidof PREFIX/BINDIR/fsser`
else
  echo Server not running
fi
if [ `pgrep -c .*fsserd` -gt 0 ] ; then
  echo Daemon running: PID `pgrep -o .*fsserd`
else
  echo Daemon stopped
fi
;;
*)
echo "Usage: /etc/init.d/fsser {start|stop|restart|status}"
exit 1
;;
esac
exit 0
