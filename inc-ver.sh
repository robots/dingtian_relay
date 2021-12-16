
VERFILE=.version
BUILDNUMFILE=.build

VER=`cat ${VERFILE}`

VER=$((${VER} + 1))
echo $VER > $VERFILE
echo 0 > $BUILDNUMFILE

