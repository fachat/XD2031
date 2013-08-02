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
ME=`readlink -f ${BASH_SOURCE[0]}`

daemon_loop() {
  while true; do
    if [ ! -c $DEVICE ] ; then
      echo $ME: Waiting for $DEVICE to appear... >> $LOGFILE
    fi
    while [ ! -c $DEVICE ]; do
      sleep 1
    done
    echo "$ME: Starting PREFIX/BINDIR/fsser as user $RUN_AS_USER" >> $LOGFILE
    echo "$ME: fsser -D -d $DEVICE $DAEMON_ARGS" >> $LOGFILE
    if [ $RUN_AS_USER = `whoami` ] ; then
      PREFIX/BINDIR/fsser -D -d $DEVICE $DAEMON_ARGS &>> $LOGFILE
    else
      su - $RUN_AS_USER -c "PREFIX/BINDIR/fsser -D -d $DEVICE $DAEMON_ARGS &>> $LOGFILE ; exit \$?" &>> $LOGFILE
    fi
    res=$?
    case $res in
      0)
        echo $ME: server returned successfully, daemon stopped. >> $LOGFILE
        exit 0
      ;;
      2)
        echo $ME: server returned EXIT_RESPAWN_ALWAYS >> $LOGFILE
      ;;
      3)
        echo $ME: server returned EXIT_RESPAWN_NEVER >> $LOGFILE
        exit $res
      ;;
      143)
        # Return value 143 = 128 + 15 (SIGTERM)
        echo $ME: server stopped by SIGTERM, daemon stopped. >> $LOGFILE
        exit 0
      ;;
      *)
        echo $ME: Server returned with $res >> $LOGFILE
        if [ "$RESPAWN" = "1" ] ; then
          echo "RESPAWN=1 --> restarting server" >> $LOGFILE
        else
          echo "RESPAWN is not equal to 1 --> daemon stopped" >> $LOGFILE
          exit $res
        fi
    esac
    sleep 1
  done
}

# Read configuration variable file if it is present
# DAEMON_ARGS -- what it says
# RUN_AS_USER -- this user will run the server
# If RUN_AS_USER is undefined, abort
#
[ -r /etc/default/fsser ] && . /etc/default/fsser

if [ -z $RUN_AS_USER ] ; then
  echo "/etc/default/fsser: RUN_AS_USER undefined!"
  exit 1
fi

if [ -z $RESPAWN ] ; then
  RESPAWN=false
fi

# Auto-detect serial device:
# If we're running on Raspberry Pi and a device with FT232 chip (XS-1541 / petSD)
# is already connected, use it. Otherwise spy for a 3.1541 or similiar connected
# to the Raspberry Pi's serial port /dev/ttyAMA0.
# If there is no /dev/ttyAMA0, the server obviously does not run on a Raspberry Pi
# and /dev/ttyUSB0 is always used
if [ -z $DEVICE ] ; then
  DEVICE="auto"
fi
if [ "$DEVICE" = "auto" ] ; then
  if [ -c "/dev/ttyAMA0" ] ; then
    DEVICE="/dev/ttyAMA0"
  else
    DEVICE="/dev/ttyUSB0"
  fi
fi

case "$1" in
start)
if pidof PREFIX/BINDIR/fsser > /dev/null ; then
  echo "Server already running."
else
  if [ `pgrep -c -f "/bin/bash $ME"` -gt 1 ] ; then
    echo "Daemon will start the server when $DEVICE appears"
  else
    echo "Starting XD-2031 daemon..."
    daemon_loop &
  fi
fi
;;
stop)
if pidof PREFIX/BINDIR/fsser > /dev/null ; then
  echo "Killing XD-2031 server..."
  kill `pidof PREFIX/BINDIR/fsser`
  sleep 1
else
  echo "No running server found."
fi
# Kill all daemons, start with the oldest.
# Exiting this script will terminate the last one.
while [ `pgrep -c -f "/bin/bash $ME"` -gt 1 ] ; do
  kill `pgrep -o -f "/bin/bash $ME"`
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
while [ `pgrep -c -f "/bin/bash $ME"` -gt 1 ] ; do
  kill `pgrep -o -f "/bin/bash $ME"`
done
echo "Starting XD-2031 daemon..."
daemon_loop &
;;
status)
if pidof PREFIX/BINDIR/fsser > /dev/null ; then
  echo Server running: PID `pidof PREFIX/BINDIR/fsser`
else
  echo Server not running
fi
if [ `pgrep -c -f "/bin/bash $ME"` -gt 1 ] ; then
  echo Daemon running: PID `pgrep -o -f "/bin/bash $ME"`
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
