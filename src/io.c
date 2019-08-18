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



#define BAUD	9600
#define F_CPU	1000000
#include <util/setbaud.h>

#define MAX_TIME_COUNT (F_CPU>>4) // <- 125ms timeout at 1MHz CPU

extern volatile uint8_t io_buf[];


void io_init(void)
{

    /* write the baudrate to the USART Baud Rate Register */
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;

#if USE_2X
    UCSR0A |= _BV(U2X0);
#else
    UCSR0A &= ~(_BV(U2X0));
#endif

    /* CONFIGURE: 8 BIT DATA, 1 STOP BIT, NO PARITY */
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
    UCSR0B = _BV(RXEN0) | _BV(TXEN0);
}



void uart_send_byte(uint8_t c)
{
    /* wait for empty transmit buffer */
    while (!(UCSR0A & (1 << UDRE0)));

    /* put data into buffer, sends data */
    UDR0 = c;
}


int8_t uart_recv_byte(uint8_t *c)
{
	uint32_t count = 0;
	while (!(UCSR0A & _BV(RXC0)))
	{
		count++;
		if (count > MAX_TIME_COUNT)
			return -1;
	}
	*c = UDR0;
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

