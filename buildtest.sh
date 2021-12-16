. source-me

VERFILE=.version
BUILDNUMFILE=.build

VER=`cat ${VERFILE}`

BLD=`cat ${BUILDNUMFILE}`
#BLD=$((${BLD} + 1))
#echo $BLD > $BUILDNUMFILE

printf -v VERHEX '%02x' $VER
printf -v BLDHEX '%02x' $BLD

BOARDS="dingtian_relay_boot"

mkdir -p bin

for b in $BOARDS; do

	cd fw_main
	#echo $b
#	make M=$b clean
	make M=$b bin || exit 1
	cp FLASH_RUN/$b/$b.bin ../bin/
	cd ..

	arrIN=(${b//_/ })
	if [[ x${arrIN[2]} == x"boot" ]]; then
		echo "./tools/genfw/genfw.py --fw-file ./bin/$b.bin --out-file ./bin/$b-${VERHEX}${BLDHEX}.bfw --board ${arrIN[1]} --crypt-mode CBC --crypt-iv random"
		python2 ./tools/genfw/genfw.py --fw-file ./bin/$b.bin --out-file ./bin/$b-${VERHEX}${BLDHEX}.bfw --board v1 --crypt-mode CBC --crypt-iv random
		rm ./bin/$b.bin
	fi
done
