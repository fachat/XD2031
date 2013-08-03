#!/bin/bash
#
# Create a XD-2031 daemon
#

LOGFILE=/var/log/fsser

# Determine the user who runs the server
if [ -z "$XD2031_USER" ] ; then
  XD2031_USER=$SUDO_USER
fi
if [ -z $XD2031_USER ] ; then
  echo "Error: unable to determine real user."
  echo "Re-run with"
  echo "        XD2031_USER=yourUserName make daemon"
  echo "or"
  echo "        sudo make daemon"
  exit 1
fi
echo "/etc/default/fsser: the server will run as user $XD2031_USER"

# Install server
install -m 755 fsser.init.d.localized /etc/init.d/fsser

# If a personal configuration already exists, don't overwrite it
if [ -f /etc/default/fsser ] ; then
  echo "Personal configuration /etc/default/fsser preserved."
else
  install -m 755 fsser.default /etc/default/fsser
fi

# Check, if the personal configuration has already the user defined
# Otherwise add it
if grep RUN_AS_USER /etc/default/fsser > /dev/null ; then
  echo "RUN_AS_USER is already defined."
else
  printf '\nRUN_AS_USER="%s"\n' $XD2031_USER >> /etc/default/fsser
fi

# Make sure, the user can write to the log file
touch $LOGFILE
chown $XD2031_USER $LOGFILE
chgrp $XD2031_USER $LOGFILE

# Update runlevel links
update-rc.d fsser defaults

# (Re)start daemon/server
service fsser restart
