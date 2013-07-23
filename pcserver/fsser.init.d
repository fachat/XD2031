#!/bin/sh
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

# Read configuration variable file if it is present
# DAEMON_ARGS -- what it says
# RUN_AS_USER -- this user will run the server
# If it is undefined, it will run with root rights
#
[ -r /etc/default/fsser ] && . /etc/default/fsser

case "$1" in
start)
if pidof fsser > /dev/null ; then
  echo "Server already running."
else
  echo "Starting XD-2031 Server..."
  PREFIX/BINDIR/fsser -D $DAEMON_ARGS >> /var/log/fsser &
fi
;;
stop)
if pidof fsser > /dev/null ; then
  echo "Killing XD-2031 Server..."
  kill `pidof fsser`
  sleep 1
else
  echo "No running server found."
fi
;;
restart)
echo "Restarting XD-2031 Server..."
if pidof fsser > /dev/null ; then
  kill `pidof fsser`
  sleep 1
else
  echo "No running server found, starting now."
fi
PREFIX/BINDIR/fsser -D $DAEMON_ARGS >> /var/log/fsser &
;;
*)
echo "Usage: /etc/init.d/fsser {start|stop|restart}"
exit 1
;;
esac
exit 0
