#!/bin/sh

echo "Run this script as superuser (e.g. with sudo)"
echo "to install rules to access your cbmdrive device"
echo "as normal user."
echo
echo "1) a group 'cbmdrive' is created"
echo "2) the current user is added to the group 'cbmdrive',"
echo "   but needs to re-login to be effective"
echo "3) copies the udev rules files into /etc/udev/rules.d"
echo "   to set the permissions for the devices for the"
echo "   'cbmdrive' group; note: needs to re-attach to be"
echo "   effective."

GRP="cbmdrive"
DIR=$(dirname $0)
ME=$(logname)

groupadd -r ${GRP}

usermod -a -G ${GRP} ${ME} 

cp -i ${DIR}/45-${GRP}-*.rules /etc/udev/rules.d


