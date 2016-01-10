
#testname="$1"
#warp="-warp"
#drivetype=1001
#imgname=blk.d82

if [ "x$MODEL" = "x" ]; then
	MODEL="xpet -model 4032"
	SOCKLOG="sock488.trace"
	POST=""
	PETCAT_OPTS="-w4 -l 0401"
	FILTER="_______"
else
	# Note we use simons basic here because we use DIRECTORY
	MODEL="x64 -basicload"
	SOCKLOG="sockiec.trace"
	POST="_64"
	PETCAT_OPTS="-w2 -l 0801"
	FILTER="directory"
fi

diskname=${imgname}-${testname}-${drivetype}

if [ "x$VICE" = "x" ]; then
	PETCATBIN=petcat
	VICEPETBIN="$MODEL"
else
	PETCATBIN=${VICE}/petcat
	VICEPETBIN=${VICE}/"$MODEL"
fi
	

if [ -f ${testname}.lst ]; then
	if [ ! -f ${testname}${POST}.prg ]; then
		echo "Binary file ${testname}${POST}.prg does not exist - generating from ${testname}.lst"
		echo "Using opts: ${PETCAT_OPTS}"
		cat ${testname}.lst | sed -e "s/${FILTER}/rem/g" | ${PETCATBIN} ${PETCAT_OPTS} > ${testname}${POST}.prg
	fi
	if [ ${testname}.lst -nt ${testname}${POST}.prg ]; then
		echo "Source file ${testname}.lst newer than binary ${testname}.prg - generating"
		echo "Using opts: ${PETCAT_OPTS}"
		cat ${testname}.lst | sed -e "s/${FILTER}/rem/g" | ${PETCATBIN} ${PETCAT_OPTS} > ${testname}${POST}.prg
	fi
fi
if [ ! -f ${testname}${POST}.prg ]; then
	echo "Test binary file ${testname}${POST}.prg not found - aborting"
	exit 1;
fi
		
if [ -f ${imgname}.gz ]; then 
	echo "unzipping ${imgname}.gz"
	gunzip -c ${imgname}.gz > ${diskname}
else 
	cp ${imgname} ${diskname}
fi;


echo "Running VICE as: ${VICEPETBIN} $warp +sound -truedrive -drive8type ${drivetype} -8 ${diskname} -autostartprgmode 1 ./${testname}${POST}.prg"
${VICEPETBIN} $warp +sound -truedrive -drive8type ${drivetype} -8 ${diskname} -autostartprgmode 1 ./${testname}${POST}.prg

echo "find resulting image in ${diskname} - you may need to gzip it with"
echo "    gzip ${diskname}"
echo "find runner script in 'sock488.trace' - you may need to move it with"
echo "    mv ${SOCKLOG} ${testname}-${drivetype}.frs"
echo "or edit the ${testname}.lst file if it exists and run again."
