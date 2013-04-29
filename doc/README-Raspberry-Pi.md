Raspberry Pi / 3.1541 README
============================

What is a 3.1541?
-----------------

A 3.1541 is basically a simple XS-1541 without the FT232 serial-over-
USB-adapter. Instead, the AVR controller is connected to the Raspberry
Pi's UART /dev/ttyAMA0 via level-shifter. In other words: it's virtually
only an Atmel AVR micro-controller connected between the Raspberry Pi 
and the serial IEC bus.


Turning off the UART functioning as a serial console
----------------------------------------------------

By default, Raspbian or similar Linux distributions use this UART as a 
serial console.  Since kernel messages would naturally disturb communication 
between XD-2031 server (Raspberry Pi) and XD-2031 device (3.1541), this
feature must disabled first. The following steps are required only once:

Backup /boot/cmdline.txt before you edit it just in case of screw-ups:

	sudo cp /boot/cmdline.txt /boot/cmdline.txt~

Edit /boot/cmdline.txt with root rights and remove any parameters
involving the serial port "ttyAMA0", which in this example is:

	console=ttyAMA0,115200 kgdboc=ttyAMA0,115200

Which gives here:

	dwc_otg.lpm_enable=0 console=tty1 root=/dev/mmcblk0p2 rootfstype=ext4 elevator=deadline rootwait

Saves your changes.

Edit (again with root rights) the file /etc/inittab.
Search for the ttyAMA0 parameter. You will find a line like this:

	T0:23:respawn:/sbin/getty -L ttyAMA0 115200 vt100

Uncomment that line by inserting a '#' at the beginning:

	#T0:23:respawn:/sbin/getty -L ttyAMA0 115200 vt100

Save your changes, then reboot:

	sudo shutdown -r now


Installation of XD-2031 Server Software
---------------------------------------

The same procedures or any other Debian / Ubuntu style Linux are
applicable here.


Updating the 3.1541 firmware
----------------------------

Distributed binaries contain both the firmware and the install scripts: 
[download firmware binaries](http://xd2031.petsd.net/firmware.php)

Make sure to have the files 3.1541up.sh and XD2031-xs1541.hex handy in your 
current directory, then start the update script which does all the magic:

	./3.1541up.sh


