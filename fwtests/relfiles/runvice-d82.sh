
#vicedir="$HOME/8bitsvn/Projects/xs1541/vice-2.4+sock488/src"
vicedir="$HOME/user/xd2031/vice-2.4+sock488/src"
testname="$1"
warp="-warp"
drivetype=1001
imgname=rel.d82


if [ -f ${testname}.lst ]; then
	if [ ! -f ${testname}.prg ]; then
		echo "Binary file ${testname}.prg does not exist - generating from ${testname}.lst"
		${vicedir}/petcat -w4 -l 0401 ${testname}.lst > ${testname}.prg
	fi
	if [ ${testname}.lst -nt ${testname}.prg ]; then
		echo "Source file ${testname}.lst newer than binary ${testname}.prg - generating"
		${vicedir}/petcat -w4 -l 0401 ${testname}.lst > ${testname}.prg
	fi
fi
if [ ! -f ${testname}.prg ]; then
	echo "Test binary file ${testname}.prg not found - aborting"
	exit 1;
fi
		
if [ -f ${imgname}.gz ]; then 
	echo "unzipping ${imgname}.gz"
	gunzip -c ${imgname}.gz > ${imgname}-${testname}
else 
	cp ${imgname} ${imgname}-${testname}
fi;


${vicedir}/xpet $warp -model 4032 -truedrive -drive8type ${drivetype} -8 ${imgname}-${testname} -autostartprgmode 1 ./${testname}.prg

echo "find resulting image in ${imgname}-${testname} - you may need to gzip it"
echo "find runner script in 'sock488.trace' - you may need to remove the DIR stuff"
