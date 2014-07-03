
vicedir="/home/fachat/8bitsvn/Projects/xs1541/vice-2.4+sock488/src"
testname="$1"

imgname=blk.d64

cp ${imgname} ${imgname}-${testname}

${vicedir}/xpet -model 4032 -truedrive -drive8type 2031 -8 ${imgname}-${testname} -autostartprgmode 1 ./${testname}.prg

echo "find runner script in 'sock488.trace' - you may need to remove the DIR stuff"
