#!/bin/sh
#
# Create a XD-2031 daemon
#
cp fsser.init.d.localized /etc/init.d/fsser
cp fsser.default /etc/default/fsser
update-rc.d fsser defaults
service fsser stop
service fsser start
