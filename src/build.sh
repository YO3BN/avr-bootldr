#!/bin/sh


if test -f 'config.txt'
then
	. ./config.txt
else
	echo "Cannot read config.txt file"
	exit 1
fi


./clean.sh

avr-gcc main.c memory.c com.c -mmcu=atmega1284 -Os -g -o ../bin/avr-bootldr.elf -Wall -Wl,--relax -Wl,--section-start=.text=0x1f000 -Wl,--section-start=.version=0x1fffe -pipe $CFLAGS
avr-objcopy -j .text -j .data -O ihex ../bin/avr-bootldr.elf ../bin/avr-bootldr.hex
avr-size ../bin/avr-bootldr.elf

 
