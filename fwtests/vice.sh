
#testname="$1"
#warp="-warp"
#drivetype=1001
#imgname=blk.d82


diskname=${imgname}-${testname}-${drivetype}

if [ "x$VICE" = "x" ]; then
	PETCATBIN=petcat
	VICEPETBIN=xpet
else
	PETCATBIN=${VICE}/petcat
	VICEPETBIN=${VICE}/xpet
fi
	

if [ -f ${testname}.lst ]; then
	if [ ! -f ${testname}.prg ]; then
		echo "Binary file ${testname}.prg does not exist - generating from ${testname}.lst"
		${PETCATBIN} -w4 -l 0401 ${testname}.lst > ${testname}.prg
	fi
	if [ ${testname}.lst -nt ${testname}.prg ]; then
		echo "Source file ${testname}.lst newer than binary ${testname}.prg - generating"
		${PETCATBIN} -w4 -l 0401 ${testname}.lst > ${testname}.prg
	fi
fi
if [ ! -f ${testname}.prg ]; then
	echo "Test binary file ${testname}.prg not found - aborting"
	exit 1;
fi
		
if [ -f ${imgname}.gz ]; then 
	echo "unzipping ${imgname}.gz"
	gunzip -c ${imgname}.gz > ${diskname}
else 
	cp ${imgname} ${diskname}
fi;


${VICEPETBIN} $warp +sound -model 4032 -truedrive -drive8type ${drivetype} -8 ${diskname} -autostartprgmode 1 ./${testname}.prg

echo "find resulting image in ${diskname} - you may need to gzip it with"
echo "    gzip ${diskname}"
echo "find runner script in 'sock488.trace' - you may need to move it with"
echo "    mv sock488.trace ${testname}-${drivetype}.frs"
echo "or edit the ${testname}.lst file if it exists and run again."
