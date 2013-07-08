#!/bin/sh
#
# Create a XD-2031 daemon
#
install -m 755 fsser.init.d.localized /etc/init.d/fsser
install -m 755 fsser.default /etc/default/fsser
update-rc.d fsser defaults
service fsser stop
service fsser start
