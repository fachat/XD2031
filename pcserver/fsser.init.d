#! /bin/sh
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
[ -r /etc/default/fsser ] && . /etc/default/fsser

case "$1" in
start)
echo "Starting XD-2031 Server..."
PREFIX/BINDIR/fsser $DAEMON_ARGS >> /var/log/fsser &
;;
stop)
echo "Killing XD-2031 Server..."
killall fsser
;;
*)
echo "Usage: /etc/init.d/fsser {start|stop}"
exit 1
;;
esac
exit 0
