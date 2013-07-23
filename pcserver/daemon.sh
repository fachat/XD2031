#!/bin/bash
#
# Create a XD-2031 daemon
#
install -m 755 fsser.init.d.localized /etc/init.d/fsser
if [ -f /etc/default/fsser ] ; then
	echo "Personal configuration /etc/default/fsser preserved."
else
	install -m 755 fsser.default /etc/default/fsser
fi
# If this scripts runs with sudo and there is no
# RUN_AS_USER entry in /etc/default/fsser yet, add
# the current user as default
#
# Check, if there is a RUN_AS_USER entry
if grep RUN_AS_USER /etc/default/fsser > /dev/null ; then
  echo "RUN_AS_USER is already defined."
else
  if [ -z $SUDO_USER ] ; then
    echo "Please edit /etc/default/fsser and add RUN_AS_USER=yourUserName"
  else
    printf '\nRUN_AS_USER="%s"\n' $SUDO_USER >> /etc/default/fsser
    echo "/etc/default/fsser: the server will run as user $SUDO_USER"
  fi
fi

update-rc.d fsser defaults
service fsser restart
