while getopts d: flag
do
    case "${flag}" in
        d) serialDevice=${OPTARG};;
    esac
done

python3 reset.py ${serialDevice}
avrdude -D -e -v -patmega4808 -Cavrdude.conf -P${serialDevice} -cjtag2updi -b 115200 -Uflash:w:nanoCUL.hex:i -Ufuses:w:nanoCUL.fuse.bin:r -u
