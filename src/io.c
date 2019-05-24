/*
 * io.c
 *
 *  Created on: May 24, 2019
 *      Author: Cristian Ionita
 *
 *      Desc: Most of the code here was took from ATmegaBOOT.c
 */

#include <stdint.h>
#include <avr/io.h>
#include "command.h"



#define BAUD_RATE	19200
#define F_CPU		1000000L
#define MAX_TIME_COUNT (F_CPU>>3) // <- 125ms timeout at 1MHz CPU
char bootuart = 1;
extern volatile uint8_t io_buf[];




void io_init(void)
{

#ifdef __AVR_ATmega128__
	/* no bootuart was selected, default to uart 0 */
	if(!bootuart) {
		bootuart = 1;
	}
#endif


	/* initialize UART(s) depending on CPU defined */
#if defined(__AVR_ATmega128__) || defined(__AVR_ATmega1280__)
	if(bootuart == 1) {
		UBRR0L = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
		UBRR0H = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
		UCSR0A = 0x00;
		UCSR0C = 0x06;
		UCSR0B = _BV(TXEN0)|_BV(RXEN0);
	}
	if(bootuart == 2) {
		UBRR1L = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
		UBRR1H = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
		UCSR1A = 0x00;
		UCSR1C = 0x06;
		UCSR1B = _BV(TXEN1)|_BV(RXEN1);
	}
#elif defined __AVR_ATmega163__
	UBRR = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
	UBRRHI = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
	UCSRA = 0x00;
	UCSRB = _BV(TXEN)|_BV(RXEN);
#elif defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__) || defined (__AVR_ATmega328__)

#ifdef DOUBLE_SPEED
	UCSR0A = (1<<U2X0); //Double speed mode USART0
	UBRR0L = (uint8_t)(F_CPU/(BAUD_RATE*8L)-1);
	UBRR0H = (F_CPU/(BAUD_RATE*8L)-1) >> 8;
#else
	UBRR0L = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
	UBRR0H = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
#endif

	UCSR0B = (1<<RXEN0) | (1<<TXEN0);
	UCSR0C = (1<<UCSZ00) | (1<<UCSZ01);

	/* Enable internal pull-up resistor on pin D0 (RX), in order
	to supress line noise that prevents the bootloader from
	timing out (DAM: 20070509) */
	DDRD &= ~_BV(PIND0);
	PORTD |= _BV(PIND0);
#elif defined __AVR_ATmega8__
	/* m8 */
	UBRRH = (((F_CPU/BAUD_RATE)/16)-1)>>8; 	// set baud rate
	UBRRL = (((F_CPU/BAUD_RATE)/16)-1);
	UCSRB = (1<<RXEN)|(1<<TXEN);  // enable Rx & Tx
	UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);  // config USART; 8N1
#else
	/* m16,m32,m169,m8515,m8535 */
	UBRRL = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
	UBRRH = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
	UCSRA = 0x00;
	UCSRC = 0x06;
	UCSRB = _BV(TXEN)|_BV(RXEN);
#endif

#if defined __AVR_ATmega1280__
	/* Enable internal pull-up resistor on pin D0 (RX), in order
	to supress line noise that prevents the bootloader from
	timing out (DAM: 20070509) */
	/* feature added to the Arduino Mega --DC: 080930 */
	DDRE &= ~_BV(PINE0);
	PORTE |= _BV(PINE0);
#endif



}



void uart_send_byte(uint8_t c)
{
#if defined(__AVR_ATmega128__) || defined(__AVR_ATmega1280__)
	if(bootuart == 1) {
		while (!(UCSR0A & _BV(UDRE0)));
		UDR0 = c;
	}
	else if (bootuart == 2) {
		while (!(UCSR1A & _BV(UDRE1)));
		UDR1 = c;
	}
#elif defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__) || defined (__AVR_ATmega328__)
	while (!(UCSR0A & _BV(UDRE0)));
	UDR0 = c;
#else
	/* m8,16,32,169,8515,8535,163 */
	while (!(UCSRA & _BV(UDRE)));
	UDR = c;
#endif
}



int8_t uart_recv_byte(uint8_t *c)
{
#if defined(__AVR_ATmega128__) || defined(__AVR_ATmega1280__)
	uint32_t count = 0;
	if(bootuart == 1) {
		while(!(UCSR0A & _BV(RXC0))) {
			/* 20060803 DojoCorp:: Addon coming from the previous Bootloader*/
			/* HACKME:: here is a good place to count times*/
			count++;
			if (count > MAX_TIME_COUNT)
				return -1;
		}

		*c = UDR0;
		return 0;
	}
	else if(bootuart == 2) {
		while(!(UCSR1A & _BV(RXC1))) {
			/* 20060803 DojoCorp:: Addon coming from the previous Bootloader*/
			/* HACKME:: here is a good place to count times*/
			count++;
			if (count > MAX_TIME_COUNT)
				return -1;
		}

		*c = UDR1;
	}
	return -1;

#elif defined(__AVR_ATmega168__)  || defined(__AVR_ATmega328P__) || defined (__AVR_ATmega328__)
	uint32_t count = 0;
	while(!(UCSR0A & _BV(RXC0))){
		/* 20060803 DojoCorp:: Addon coming from the previous Bootloader*/
		/* HACKME:: here is a good place to count times*/
		count++;
		if (count > MAX_TIME_COUNT)
			return -1;
	}
	*c = UDR0;
#else
	/* m8,16,32,169,8515,8535,163 */
	uint32_t count = 0;
	while(!(UCSRA & _BV(RXC))){
		/* 20060803 DojoCorp:: Addon coming from the previous Bootloader*/
		/* HACKME:: here is a good place to count times*/
		count++;
		if (count > MAX_TIME_COUNT)
			return -1;
	}
	*c = UDR;
#endif
	return 0;
}

void io_send(void *pvdata, uint16_t uslen)
{
	uint8_t *p = pvdata;
	while (uslen)
	{
		uart_send_byte(*p);
		uslen--;
		p++;
	}
}


uint16_t io_recv(void)
{
	uint16_t len;

	for (len = 0; len < 256 + 16; len++)
	{
		if (uart_recv_byte((uint8_t*) io_buf + len) == -1)
		{
			break;
		}
	}

	return len;
}



