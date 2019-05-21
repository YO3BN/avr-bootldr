/*
 * avr-bootldr
 * main.c
 *
 *  Created on: May 18, 2019
 *      Author: Cristian Ionita
 */



#include <stdint.h>
#include <avr/io.h>
#include <avr/signature.h>
#include <avr/boot.h>

#include "command.h"


#define HW_VER		0x01
#define SW_MAJOR	0x00
#define SW_MINOR	0x01

#define SWAP_US(a)	((((a) & 0xff00) >> 8) | (((a) & 0x00ff) << 8))


/* FIXME:: using dummy 5, but for some MCU this is reserved and should not be used */
#ifndef SIGRD
#define SIGRD 5
#warning "!!!!!!!! MCU does not have SIGRD. Using dummy 5 !!!!!!!!"
#endif

void (*app_start)(void) = 0x0000;
static volatile uint8_t uart_buf[265];
volatile uint16_t prog_address;	/* bytes for EEPROM ; words for FLASH */


void program_flash_page(uint8_t *buf, uint16_t uslen);
void program_eeprom_page(const void *pvdata, uint16_t uslen);


void uart_send(void *pvdata, uint16_t uslen)
{

}

void uart_init(void)
{

}


void uart_recv(void)
{
/* TODO:: timeout
 * TODO:: recv stop if count >= sizeof buf
 */
}

static void quick_fail_response(void)
{
	struct response
	{
		uint8_t insync;
		uint8_t status;
	} *presponse;

	presponse = (struct response*) uart_buf;

	presponse->insync = Resp_STK_INSYNC;
	presponse->status = Resp_STK_FAILED;

	uart_send(presponse, sizeof(*presponse));
}


static void quick_ok_response(void)
{
	struct response
	{
		uint8_t insync;
		uint8_t status;
	} *presponse;

	presponse = (struct response*) uart_buf;

	presponse->insync = Resp_STK_INSYNC;
	presponse->status = Resp_STK_OK;

	uart_send(presponse, sizeof(*presponse));
}


static void quick_byte_response(uint8_t byte)
{
	struct response
	{
		uint8_t insync;
		uint8_t byte;
		uint8_t status;
	} *presponse;

	presponse = (struct response*) uart_buf;

	presponse->byte = byte;
	presponse->insync = Resp_STK_INSYNC;
	presponse->status = Resp_STK_OK;

	uart_send(presponse, sizeof(*presponse));
}


/*
 * The PC sends this command to check if the starterkit is present on
 * the communication channel.
 */
static void get_sign_on(void)
{
	struct response
	{
		uint8_t insync;
		char sign_on_message[7];
		uint8_t status;
	} *presponse;

	presponse = (struct response*) uart_buf;

	/*
	 * The following sequence makes .txt and .data segments smaller than using:
	 * strcpy(presponse->sign_on_message, STK_SIGN_ON_MESSAGE);
	 */
	presponse->sign_on_message[0] = 'A';
	presponse->sign_on_message[1] = 'V';
	presponse->sign_on_message[2] = 'R';
	presponse->sign_on_message[3] = ' ';
	presponse->sign_on_message[4] = 'S';
	presponse->sign_on_message[5] = 'T';
	presponse->sign_on_message[6] = 'K';

	presponse->insync = Resp_STK_INSYNC;
	presponse->status = Resp_STK_OK;

	uart_send(presponse, sizeof(*presponse));
}


/*
 * Load 16-bit address down to starterkit. This command is used to set the address for the
 * next read or write operation to FLASH or EEPROM. Must always be used prior to
 * Cmnd_STK_PROG_PAGE or Cmnd_STK_READ_PAGE.
 */
static void load_address(void)
{
	struct request
	{
		uint8_t cmd;
		union
		{
			struct
			{
				uint8_t addr_low;
				uint8_t addr_high;
			}byte;
			uint16_t address;
		} u16;
		uint8_t eop;
	} *prequest;

	prequest = (struct request*) uart_buf;

	/* TODO:: check for right endian ordering!! */
	prog_address = prequest->u16.address;

	quick_ok_response();
}


/*
 * Get the value of a valid parameter from the STK500 starterkit.
 * If the parameter is not used, the same parameter will be returned
 * together with a Resp_STK_FAILED response to indicate the error.
 * See the parameters section for valid parameters and their meaning.
 */
static void get_parameter(void)
{
	struct request
	{
		uint8_t cmd;
		uint8_t parameter;
		uint8_t eop;
	} *prequest;

	prequest = (struct request*) uart_buf;

	switch (prequest->parameter)
	{
	case Parm_STK_HW_VER:
		quick_byte_response(HW_VER);
		break;

	case Parm_STK_SW_MAJOR:
		quick_byte_response(SW_MAJOR);
		break;

	case Parm_STK_SW_MINOR:
		quick_byte_response(SW_MINOR);
		break;

	/*
	 * Other parameters not supported yet.
	 * Deviation from standard protocol...
	 * We send 0x00, took from ATmegaBOOT.c
	 */
	default:
		quick_byte_response(0);
		break;
	}
}


/*
 * Download a block of data to the starterkit and program it in FLASH or EEPROM of the
 * current device. The data block size should not be larger than 256 bytes.
 */
static void program_page(void)
{
	struct request
	{
		uint8_t cmd;
		union
		{
			struct
			{
				uint8_t high;
				uint8_t low;
			} bytes;
			uint16_t size;
		} u16;
		uint8_t memtype;
		uint8_t data[257];	/* 256 + EOP byte */
	} *prequest;

	prequest = (struct request*) uart_buf;
	/* Inplace endianness fix. */
	prequest->u16.size = SWAP_US(prequest->u16.size);

	/* Memory to program */
	if (prequest->memtype == 'F')
	{
		/* Check page boundary. */
		if (((prog_address * 2) % SPM_PAGESIZE) &&
				(prequest->u16.size > SPM_PAGESIZE))
		{
			quick_fail_response();
			return;
		}
		/* FIXME:: enddianness!!! */
		program_flash_page(prequest->data, prequest->u16.size);
	}
	else if (prequest->memtype == 'E')
	{
		/* TODO:: check page boundary */
		/* FIXME:: enddianness!!! */
		program_eeprom_page(prequest->data, prequest->u16.size);
	}

	quick_ok_response();
}

/*
 * Read signature bytes.
 */
static void read_signature_bytes(void)
{
	struct response
	{
		uint8_t insync;
		uint8_t sign_high;
		uint8_t sign_middle;
		uint8_t sign_low;
		uint8_t status;
	} *presponse;

	presponse = (struct response*) uart_buf;

	/* TODO:: check endian ordering */
	/* FIXME:: not all atmegas have SIGRD */
	presponse->sign_high	= boot_signature_byte_get(0x0004);
	presponse->sign_middle	= boot_signature_byte_get(0x0002);
	presponse->sign_low		= boot_signature_byte_get(0x0000);

	presponse->insync = Resp_STK_INSYNC;
	presponse->status = Resp_STK_OK;

	uart_send(presponse, sizeof(*presponse));
}


int main(void)
{
//	if (!uart_connected())
//	{
//		start_app();
//	}
//
//	uart_init();

	for (;;)
	{
		//	uart_recv();

		switch (uart_buf[0])
		{

		/* Check if Starterkit Present */
		case Cmnd_STK_GET_SIGN_ON:
			get_sign_on();
			break;

		/* Load Address */
		case Cmnd_STK_LOAD_ADDRESS:
			load_address();
			break;

		/* Get Parameter Value */
		case Cmnd_STK_GET_PARAMETER:
			get_parameter();
			break;

		/* Program Page */
		case Cmnd_STK_PROG_PAGE:
			program_page();
			break;

		/* Read Device Signature */
		case Cmnd_STK_READ_SIGN:
			read_signature_bytes();
			break;

		/* Get Synchronization */
		case Cmnd_STK_GET_SYNC:
			quick_ok_response();
			break;

		default:
			quick_fail_response();
			break;
		}
	}

	return 0;
}

