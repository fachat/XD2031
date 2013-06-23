#!/bin/bash
# Fix 2011 petSD fuses
# 
# ONLY FOR USE WITH PETSD!
#
ISPCFG=programmer.cfg
if test ! -s $ISPCFG
then
	echo Could not find $ISPCFG, aborting
	exit 1
fi
. ./$ISPCFG
echo ""
echo ""
echo ""
echo "petSD bad fuses fixer"
echo "====================="
echo ""
echo "This tool changes the AVR fuses"
echo "for compatibility with sd2iec and XD-2031."
echo ""
echo "FOR USE ONLY WITH PETSD!"
echo "APPLYING TO OTHER DEVICES MAY BRICK THEM!"
echo ""
read -p "If you are shure, enter 'yes' to continue: " sure
if [ "$sure" == "yes" ]
   then
      avrdude -c $PROGRAMMER -P $CONNECTED_TO -p m1284p -v -y -U lfuse:w:0xf7:m -U hfuse:w:0xd2:m -U efuse:w:0xff:m
   else
      echo "Aborted."
      exit 1
fi
echo ""
echo "*******************************************************************"
echo ""
echo "If the last output was..."
echo ""
echo "      avrdude: safemode: lfuse reads as F7"
echo "      avrdude: safemode: hfuse reads as D2"
echo "      avrdude: safemode: efuse reads as FF"
echo "      avrdude: safemode: Fuses OK"
echo ""
echo "      avrdude done.  Thank you."
echo ""
echo "...you're now ready for XD-2031!"
echo ""
