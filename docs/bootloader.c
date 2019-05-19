/*
stk500boot.c  20030810

Copyright (c) 2003, Jason P. Kyle
All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Licence can be viewed at http://www.fsf.org/licenses/gpl.txt


Target = Atmel AVR m128,m64,m32,m16,m8,m162,m163,m169,m8515,m8535
ATmega161 has a very small boot block so isn't supported.

Tested with m128,m8,m163 - feel free to let me know how/if it works for you.
*/

#define F_CPU 14745600UL

#include <inttypes.h>
#include <avr/io.h>
#include <avr/boot.h>
#include <compat/deprecated.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

#define 	eeprom_rb(addr)   eeprom_read_byte ((uint8_t *)(addr))
#define 	eeprom_rw(addr)   eeprom_read_word ((uint16_t *)(addr))
#define 	eeprom_wb(addr, val)   eeprom_write_byte ((uint8_t *)(addr), (uint8_t)(val))


#define BAUD_RATE		115200UL

#define DECRYPT 0
#define ENCRYPT 1
//#define DES_ENCRYPTION

#define HW_VER	0x02
#define SW_MAJOR	0x01
#define SW_MINOR	0x0e


/* Adjust to suit whatever pin your hardware uses to enter the bootloader */
/*
#define BL_DDR DDRB
#define BL_PORT PORTB
#define BL_PIN PINB
#define BL PINB5
*/

/*
 * Charon II. Development Board bootloader entry point - CEBA 14.09.03
 */
#define BL_DDR DDRE
#define BL_PORT PORTE
#define BL_PIN PINE
#define BL PORTE4



#define SIG1	0x1E	// Yep, Atmel is the only manufacturer of AVR micros.  Single source :(

	#define SIG2	0x97
	#define SIG3	0x02
	#define PAGE_SIZE	0x80U	//128 words
	#define UART0



void out_putchar(char);
char getch(void);
void getNch(uint8_t);
void byte_response(uint8_t);
void nothing_response(void);


union address_union {
	uint16_t word;
	uint8_t  byte[2];
} address;

union length_union {
	uint16_t word;
	uint8_t  byte[2];
} length;

struct flags_struct {
	unsigned eeprom : 1;
	unsigned rampz  : 1;
} flags;

uint8_t buff[256];
uint8_t address_high;

uint8_t pagesz=0x80;

void (*app_start)(void) = 0x0000;



//set the leds to the value in binary
#define set_leds(stat) PORTA = (PORTA & 0x0F) | ((stat * 0x10) & 0xF0)

//returns the state of all leds
#define get_leds() ((PORTA & 0xF0)/0x10)
#define gp_jumper_short() bit_is_clear(PING,0)

#include <util/delay.h>


void wait(unsigned int a)
{							
	while(a--)_delay_ms(1);
	return;
}

#define bluetooth_connection_established() bit_is_clear(PINB,0)


#define get_leds() ((PORTA & 0xF0)/0x10)
#define toggle_decoleds() PORTG ^= _BV(1)



int main(void)
{
uint8_t ch,ch2;
uint16_t w;


	wait(50);
	

	DDRG = _BV(1);	//the decorational LEDs
	PORTG = _BV(0);//the port whose sole purpose is to have a gpio jumper


	DDRA = 0xF0;
	wait(50);

	if(!gp_jumper_short())app_start();



	asm volatile("nop\n\t");
/*	if(pgm_read_byte_near(0x0000) != 0xFF) {		// Don't start application if it isn't programmed yet
		if(bit_is_set(BL_PIN,BL)) app_start();	// Do we start the application or enter bootloader? RAMPZ=0
	}*/

	UBRR1L = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
	UBRR1H = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
	UCSR1A = 0x00;
	UCSR1C = _BV(UCSZ11) | _BV(UCSZ10);
	UCSR1B = _BV(RXEN1) | _BV(TXEN1);

	UBRR0L = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
	UBRR0H = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
	UCSR0A = 0x00;
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
	UCSR0B = _BV(RXEN0) | _BV(TXEN0);
	
	
	
	DDRD |= _BV(3);
	DDRE |= _BV(1);
	
	set_leds(0x09); 	
	
	
	out_putchar('\0');


 for (;;) {
   ch = getch();
	if(ch=='0') {		// Hello is anyone home?
		nothing_response();
	}
		// Request programmer ID
	else if(ch=='1') {		//Yes i've heard of the switch statement, a bunch of else if's -> smaller code
		if (getch() == ' ') {
			out_putchar(0x14);
			out_putchar('A');	//Not using PROGMEM string due to boot block in m128 being beyond 64kB boundry
			out_putchar('V');	//Would need to selectively manipulate RAMPZ, and it's only 9 characters anyway so who cares.
			out_putchar('R');
			out_putchar(' ');
			out_putchar('I');
			out_putchar('S');
			out_putchar('P');
			out_putchar(0x10);
		}
	}
	else if(ch=='@') {		// AVR ISP/STK500 board commands  DON'T CARE so default nothing_response
		ch2 = getch();
		if (ch2>0x85) getch();
		nothing_response();
	}
	else if(ch=='A') {		// AVR ISP/STK500 board requests
		ch2 = getch();
		if(ch2==0x80) byte_response(HW_VER);		// Hardware version
		else if(ch2==0x81) byte_response(SW_MAJOR);	// Software major version
		else if(ch2==0x82) byte_response(SW_MINOR);	// Software minor version
		else if(ch2==0x98) byte_response(0x03);		// Unknown but seems to be required by avr studio 3.56
		else byte_response(0x00);					// Covers various unnecessary responses we don't care about
	}
	else if(ch=='B') {		// Device Parameters  DON'T CARE, DEVICE IS FIXED
		getNch(20);
		nothing_response();
	}
	else if(ch=='E') {		// Parallel programming stuff  DON'T CARE
		getNch(5);
		nothing_response();
	}
	else if(ch=='P') {		// Enter programming mode
		nothing_response();
	}
	else if(ch=='Q') {		// Leave programming mode
		nothing_response();
	}
	else if(ch=='R') {		// Erase device, don't care as we will erase one page at a time anyway.
		nothing_response();
	}
	else if(ch=='U') {		//Set address, little endian. EEPROM in bytes, FLASH in words
							//Perhaps extra address bytes may be added in future to support > 128kB FLASH.
							//This might explain why little endian was used here, big endian used everywhere else.
		address.byte[0] = getch();
		address.byte[1] = getch();
		nothing_response();
	}
	else if(ch=='V') {		// Universal SPI programming command, disabled.  Would be used for fuses and lock bits.
		getNch(4);
		byte_response(0x00);
	}
	else if(ch=='d') {		// Write memory, length is big endian and is in bytes
		length.byte[1] = getch();
		length.byte[0] = getch();
		flags.eeprom = 0;
		if (getch() == 'E') flags.eeprom = 1;
		for (w=0;w<length.word;w++) {
		  buff[w] = getch();	// Store data in buffer, can't keep up with serial data stream whilst programming pages
		}
		if (getch() == ' ') {
			if (flags.eeprom) {		//Write to EEPROM one byte at a time
				for(w=0;w<length.word;w++) {
					eeprom_wb(address.word,buff[w]);
					address.word++;
				}
			}
			else {					//Write to FLASH one page at a time
				if (address.byte[1]>127) address_high = 0x01;	//Only possible with m128, m256 will need 3rd address byte. FIXME
				else address_high = 0x00;
#ifdef __AVR_ATmega128__
				RAMPZ = address_high;
#endif
				address.word = address.word << 1;	//address * 2 -> byte location
//				if ((length.byte[0] & 0x01) == 0x01) length.word++;	//Even up an odd number of bytes
				if ((length.byte[0] & 0x01)) length.word++;	//Even up an odd number of bytes
				cli();									//Disable interrupts, just to be sure
				while(bit_is_set(EECR,EEWE));			//Wait for previous EEPROM writes to complete
				asm volatile("clr	r17				\n\t"	//page_word_count
							 "lds	r30,address		\n\t"	//Address of FLASH location (in bytes)
							 "lds	r31,address+1	\n\t"
							 "ldi	r28,lo8(buff)		\n\t"	//Start of buffer array in RAM
							 "ldi	r29,hi8(buff)		\n\t"
							 "lds	r24,length		\n\t"	//Length of data to be written (in bytes)
							 "lds	r25,length+1	\n\t"
							 "length_loop:			\n\t"	//Main loop, repeat for number of words in block
							 "cpi	r17,0x00	\n\t"	//If page_word_count=0 then erase page
							 "brne	no_page_erase	\n\t"
							 "wait_spm1:			\n\t"
							 "lds	r16,%0		\n\t"	//Wait for previous spm to complete
							 "andi	r16,1\n\t"
							 "cpi	r16,1\n\t"
							 "breq	wait_spm1\n\t"
							 "ldi	r16,0x03		\n\t"	//Erase page pointed to by Z
							 "sts	%0,r16		\n\t"
							 "spm					\n\t"
#ifdef __AVR_ATmega163__
	 						 ".word 0xFFFF					\n\t"
							 "nop							\n\t"
#endif
							 "wait_spm2:			\n\t"
							 "lds	r16,%0		\n\t"	//Wait for previous spm to complete
							 "andi	r16,1\n\t"
							 "cpi	r16,1\n\t"
							 "breq	wait_spm2\n\t"

							 "ldi	r16,0x11				\n\t"	//Re-enable RWW section
					 		 "sts	%0,r16				\n\t"
					 		 "spm							\n\t"
#ifdef __AVR_ATmega163__
	 						 ".word 0xFFFF					\n\t"
							 "nop							\n\t"
#endif
							 "no_page_erase:		\n\t"
							 "ld	r0,Y+			\n\t"		//Write 2 bytes into page buffer
							 "ld	r1,Y+			\n\t"

							 "wait_spm3:			\n\t"
							 "lds	r16,%0		\n\t"	//Wait for previous spm to complete
							 "andi	r16,1\n\t"
							 "cpi	r16,1\n\t"
							 "breq	wait_spm3\n\t"
							 "ldi	r16,0x01		\n\t"	//Load r0,r1 into FLASH page buffer
							 "sts	%0,r16		\n\t"
							 "spm					\n\t"

							 "inc	r17				\n\t"	//page_word_count++
							 "cpi r17,%1	\n\t"
							 "brlo	same_page		\n\t"	//Still same page in FLASH
							 "write_page:			\n\t"
							 "clr	r17				\n\t"	//New page, write current one first
							 "wait_spm4:			\n\t"
							 "lds	r16,%0		\n\t"	//Wait for previous spm to complete
							 "andi	r16,1\n\t"
							 "cpi	r16,1\n\t"
							 "breq	wait_spm4\n\t"
#ifdef __AVR_ATmega163__
							 "andi	r30,0x80		\n\t"	// m163 requires Z6:Z1 to be zero during page write
#endif
							 "ldi	r16,0x05		\n\t"	//Write page pointed to by Z
							 "sts	%0,r16		\n\t"
							 "spm					\n\t"
#ifdef __AVR_ATmega163__
	 						 ".word 0xFFFF					\n\t"
							 "nop							\n\t"
							 "ori	r30,0x7E		\n\t"		// recover Z6:Z1 state after page write (had to be zero during write)
#endif
							 "wait_spm5:			\n\t"
							 "lds	r16,%0		\n\t"	//Wait for previous spm to complete
							 "andi	r16,1\n\t"
							 "cpi	r16,1\n\t"
							 "breq	wait_spm5\n\t"
							 "ldi	r16,0x11				\n\t"	//Re-enable RWW section
					 		 "sts	%0,r16				\n\t"
					 		 "spm							\n\t"
#ifdef __AVR_ATmega163__
	 						 ".word 0xFFFF					\n\t"
							 "nop							\n\t"
#endif
							 "same_page:			\n\t"
							 "adiw	r30,2			\n\t"	//Next word in FLASH
							 "sbiw	r24,2			\n\t"	//length-2
							 "breq	final_write		\n\t"	//Finished
							 "rjmp	length_loop		\n\t"
							 "final_write:			\n\t"
							 "cpi	r17,0			\n\t"
							 "breq	block_done		\n\t"
							 "adiw	r24,2			\n\t"	//length+2, fool above check on length after short page write
							 "rjmp	write_page		\n\t"
							 "block_done:			\n\t"
							 "clr	__zero_reg__	\n\t"	//restore zero register
							 : "=m" (SPMCR) : "M" (PAGE_SIZE) : "r0","r16","r17","r24","r25","r28","r29","r30","r31");

/* Should really add a wait for RWW section to be enabled, don't actually need it since we never */
/* exit the bootloader without a power cycle anyhow */
			}
			out_putchar(0x14);
			out_putchar(0x10);
		}
	}
	else if(ch=='t') {		//Read memory block mode, length is big endian.
		length.byte[1] = getch();
		length.byte[0] = getch();
#if defined __AVR_ATmega128__
		if (address.word>0x7FFF) flags.rampz = 1;		// No go with m256, FIXME
		else flags.rampz = 0;
#endif
		if (getch() == 'E') flags.eeprom = 1;
		else {
			flags.eeprom = 0;
			address.word = address.word << 1;	//address * 2 -> byte location
		}
		if (getch() == ' ') {		// Command terminator
			out_putchar(0x14);
			for (w=0;w < length.word;w++) {		// Can handle odd and even lengths okay
				if (flags.eeprom) {	// Byte access EEPROM read
					out_putchar(eeprom_rb(address.word));
					address.word++;
				}
				else {
					if (!flags.rampz) out_putchar(pgm_read_byte_near(address.word));
#if defined __AVR_ATmega128__
					else out_putchar(pgm_read_byte_far(address.word + 0x10000));	// Hmmmm, yuck  FIXME when m256 arrvies
#endif
					address.word++;
				}
			}
			out_putchar(0x10);
		}
	}
	else if(ch=='u') {		// Get device signature bytes
		if (getch() == ' ') {
			out_putchar(0x14);
			out_putchar(SIG1);
			out_putchar(SIG2);
			out_putchar(SIG3);
			out_putchar(0x10);
		}
	}
	else if(ch=='v') {		// Read oscillator calibration byte
		byte_response(0x00);
	}
  }
}


void out_putchar(char ch)
{
	unsigned char a = get_leds()+1; 
	set_leds(a);

	while (!(inb(UCSR1A) & _BV(UDRE1)));
	UDR1=ch;
}


char getch(void)
{
	unsigned char a = get_leds()+1; 
	set_leds(a);
 //goto_small:
	while(!(inb(UCSR1A) & _BV(RXC1)));
	a = UDR1;
	//if(!bluetooth_connection_established()) goto goto_small;
	return a;
}


void getNch(uint8_t count)
{
uint8_t i;
	for(i=0;i<count;i++) 
	{
	 goto_small:
		while(!(inb(UCSR1A) & _BV(RXC1)));
		if(!bluetooth_connection_established())
		{
			wait(50);
			goto goto_small;
		}
		inb(UDR1);
	}
}


void byte_response(uint8_t val)
{
	if (getch() == ' ') {
		out_putchar(0x14);
		out_putchar(val);
		out_putchar(0x10);
	}
}

void nothing_response(void)
{
	if (getch() == ' ') {
		out_putchar(0x14);
		out_putchar(0x10);
	}
}

