
#testname="$1"
#warp="-warp"
#drivetype=1001
#imgname=blk.d82
#imgname2=blk2.d82 (optional)

if [ "x$MODEL" = "x" ]; then
	MODEL="xpet -model 4032"
	SOCKLOG="sock488.trace"
	POST=""
	PETCAT_OPTS="-w4 -l 0401"
	FILTER="_______"
	DIR="PET"
else
	# Note we simulate a drive 9, as the trace facility otherwise does not
	# trace data sent from the drive 8.
	MODEL="${MODEL} -basicload -iecdevice9 +iecdevice10 +iecdevice11 +iecdevice8"
	SOCKLOG="sockiec.trace"
	POST="_64"
	PETCAT_OPTS="-w2 -l 0801"
	FILTER="directory"
	DIR=C64
fi

diskname=${imgname}-${testname}-${drivetype}
diskname2=${imgname2}-${testname}-${drivetype}
outfilename=${testname}-${drivetype}
prgname=${testname}

if [ "x${variant}" != "x" ]; then
	diskname=${diskname}-${variant}
	diskname2=${diskname2}-${variant}
	outfilename=${outfilename}-${variant}
	prgname=${testname}-${variant}
fi

if [ ${drivetype} == "8050" ]; then
	dostype="1001"
else
	dostype="${drivetype}"
fi

if [ "x$VICE" = "x" ]; then
	PETCATBIN=petcat
	VICEPETBIN="$MODEL"
else
	PETCATBIN=${VICE}/petcat
	VICEPETBIN=${VICE}/"$MODEL"
fi

if [ "x$VICEDATA" = "x" ]; then
	VICEPAR=""
else
	VICEPAR="-directory $VICEDATA/$DIR -dos${dostype} ${VICEDATA}/DRIVES/dos${dostype}"
fi
	

if [ -f ${prgname}.lst ]; then
	if [ ! -f ${prgname}${POST}.prg ]; then
		echo "Binary file ${prgname}${POST}.prg does not exist - generating from ${prgname}.lst"
		echo "Using opts: ${PETCAT_OPTS}"
		cat ${prgname}.lst | sed -e "s/${FILTER}/rem/g" | ${PETCATBIN} ${PETCAT_OPTS} > ${prgname}${POST}.prg
	fi
	if [ ${prgname}.lst -nt ${prgname}${POST}.prg ]; then
		echo "Source file ${prgname}.lst newer than binary ${prgname}.prg - generating"
		echo "Using opts: ${PETCAT_OPTS}"
		cat ${prgname}.lst | sed -e "s/${FILTER}/rem/g" | ${PETCATBIN} ${PETCAT_OPTS} > ${prgname}${POST}.prg
	fi
fi
if [ ! -f ${prgname}${POST}.prg ]; then
	echo "Test binary file ${prgname}${POST}.prg not found - aborting"
	exit 1;
fi
	
attach8=""
attach9=""
echo "Trying to use image ${imgname} as ${diskname}"	
if [ -f ${imgname}.gz ]; then 
	echo "unzipping ${imgname}.gz"
	gunzip -c ${imgname}.gz > ${diskname}
else 
	cp ${imgname} ${diskname}
fi;
attach8="-8 ${diskname}"
if [ "x$imgname2" != "x" ]; then
  echo "Trying to use image ${imgname2} as ${diskname2}"	
  if [ -f ${imgname2}.gz ]; then 
	echo "unzipping ${imgname2}.gz"
	gunzip -c ${imgname2}.gz > ${diskname2}
  else 
	cp ${imgname2} ${diskname2}
  fi;
  attach9="-9 ${diskname2}"
fi


echo "Running VICE as: ${VICEPETBIN} ${VICEPAR} $warp +sound -truedrive -drive8type ${drivetype} ${attach8} ${attach9} -autostartprgmode 1 ./${prgname}${POST}.prg"
${VICEPETBIN} ${VICEPAR} $warp +sound -truedrive -drive8type ${drivetype} ${attach8} ${attach9} -autostartprgmode 1 ./${prgname}${POST}.prg

echo "find resulting image in ${diskname} (or ${diskname2}) - you may need to gzip it with"
echo "    gzip ${diskname}"
echo "find runner script in ${SOCKLOG} - you may need to move it with"
echo "    mv ${SOCKLOG} ${outfilename}.frs"
echo "or edit the ${prgname}.lst file if it exists and run again."
