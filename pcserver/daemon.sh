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
update-rc.d fsser defaults
service fsser stop
service fsser start
