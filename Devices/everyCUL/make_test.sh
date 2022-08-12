avr-gcc -c -mmcu=atmega4809 -I. -gstabs -Os -Wall -Wstrict-prototypes -std=gnu99  test.c -o test.o
avr-gcc -mmcu=atmega4809 -I. -gstabs -Os -Wall -Wstrict-prototypes -std=gnu99 test.o --output test.elf -lm
avr-objcopy -O ihex -R .eeprom -R .fuse test.elf nanoCUL.hex
avr-objcopy -O binary -j .fuse test.elf nanoCUL.fuse.bin



