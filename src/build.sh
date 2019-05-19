#!/bin/sh

./clean.sh

avr-gcc main.c memory.c -mmcu=atmega168 -Os -g -o ../bin/avr-bootldr.elf
avr-objcopy -j .text -j .data -O binary ../bin/avr-bootldr.elf ../bin/avr-bootldr.bin
avr-size ../bin/avr-bootldr.elf
stat ../bin/avr-bootldr.bin
 
