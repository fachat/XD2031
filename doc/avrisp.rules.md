Firmware programming with AVRDUDE
=================================

If in-system-programming with AVRDUDE works with root rights, but doesn't
with user rights, you might want to add a udev rule.

Ubuntu / Linux Mint
-------------------

This procedure has been tested with Linux Mint 14 and should work out of
the box with Ubuntu 12.10 too.

Copy avrisp.rules to /etc/udev/avrisp.rules. It adds rules enabling access
for members of the plugdev group to the Atmel JTAG ICE mkII, AVRISP mkII
and AVR Dragon.

Now create a virtual link to the file and give it a rule priority:

        cd /etc/udev/rules.d
        sudo ln ../avrisp.rules 60-avrisp.rules

Check, you're in the plugdev group:

        groups

Restart udev:

        sudo restart udev

Unplug the programmer and plug it back.
make flash / make fuses should work nicely now.


Fedora
------

With root rights:

        cp avrisp.rules /etc/udev/rules.d/60-avrisp.rules
        usermod -a -G plugdev <YOUR-USER-NAME>
        reboot
