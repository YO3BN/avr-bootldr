/*
 * avr-bootldr
 * main.c
 *
 *  Created on: May 18, 2019
 *      Author: Cristian Ionita
 */



#include <stdint.h>
#include <string.h>
#include <avr/io.h>
#include <avr/boot.h>
#include <avr/interrupt.h>

#include "command.h"


#define HW_VER		0x01
#define SW_MAJOR	0x02
#define SW_MINOR	0x03

#define SWAP_US(a)	((((a) & 0xff00) >> 8) | (((a) & 0x00ff) << 8))



void (*app_start)(void) = 0x0000;
volatile uint8_t io_buf[256 + 16];
volatile struct
{
	char mode;
	uint16_t address;	/* bytes for EEPROM ; words for FLASH */
} programming;


void write_flash_page(uint8_t *buf, uint16_t uslen);
void write_eeprom_page(const void *pvdata, uint16_t uslen);

void read_flash_page(void *pvdata, uint16_t uslen);
void read_eeprom_page(void *pvdata, uint16_t uslen);


void io_send(void *pvdata, uint16_t uslen);
uint16_t io_recv(void);
void io_init(void);



static void quick_fail_response(void)
{
	struct response
	{
		uint8_t insync;
		uint8_t status;
	} *response_ptr;

	response_ptr = (struct response*) io_buf;

	response_ptr->insync = Resp_STK_INSYNC;
	response_ptr->status = Resp_STK_FAILED;

	io_send(response_ptr, sizeof(*response_ptr));
}


static void quick_ok_response(void)
{
	struct response
	{
		uint8_t insync;
		uint8_t status;
	} *response_ptr;

	response_ptr = (struct response*) io_buf;

	response_ptr->insync = Resp_STK_INSYNC;
	response_ptr->status = Resp_STK_OK;

	io_send(response_ptr, sizeof(*response_ptr));
}


static void quick_byte_response(uint8_t byte)
{
	struct response
	{
		uint8_t insync;
		uint8_t byte;
		uint8_t status;
	} *response_ptr;

	response_ptr = (struct response*) io_buf;

	response_ptr->byte = byte;
	response_ptr->insync = Resp_STK_INSYNC;
	response_ptr->status = Resp_STK_OK;

	io_send(response_ptr, sizeof(*response_ptr));
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
	} *response_ptr;

	response_ptr = (struct response*) io_buf;

	/*
	 * The following sequence makes .txt and .data segments smaller than using:
	 * strcpy(response_ptr->sign_on_message, STK_SIGN_ON_MESSAGE);
	 */
	response_ptr->sign_on_message[0] = 'A';
	response_ptr->sign_on_message[1] = 'V';
	response_ptr->sign_on_message[2] = 'R';
	response_ptr->sign_on_message[3] = ' ';
	response_ptr->sign_on_message[4] = 'S';
	response_ptr->sign_on_message[5] = 'T';
	response_ptr->sign_on_message[6] = 'K';

	response_ptr->insync = Resp_STK_INSYNC;
	response_ptr->status = Resp_STK_OK;

	io_send(response_ptr, sizeof(*response_ptr));
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
				uint8_t low;
				uint8_t high;
			}addr;
			uint16_t address;
		} u16;
		uint8_t eop;
	} *request_ptr;

	request_ptr = (struct request*) io_buf;

	/* TODO:: check for right endian ordering!! */
	programming.address = request_ptr->u16.address;

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
	uint8_t byte;
	struct request
	{
		uint8_t cmd;
		uint8_t parameter;
		uint8_t eop;
	} *request_ptr;

	request_ptr = (struct request*) io_buf;

	switch (request_ptr->parameter)
	{
	case Parm_STK_HW_VER:
		byte = HW_VER;
		break;

	case Parm_STK_SW_MAJOR:
		byte = SW_MAJOR;
		break;

	case Parm_STK_SW_MINOR:
		byte = SW_MINOR;
		break;

	/*
	 * Deviation from standard protocol...
	 * We send 0x00, took from ATmegaBOOT.c
	 */
	default:
		byte = 0;
		break;
	}

	quick_byte_response(byte);
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
	} *request_ptr;

	request_ptr = (struct request*) io_buf;

	if (!programming.mode)
	{
		goto fail;
	}

	/* Inplace endianness fix. */
	request_ptr->u16.size = SWAP_US(request_ptr->u16.size);

	/* Memory to program */
	if (request_ptr->memtype == 'F')
	{
		/* Check page boundary. */
		if (((programming.address * 2) % SPM_PAGESIZE) != 0 ||
				(request_ptr->u16.size != SPM_PAGESIZE))
		{
			goto fail;
		}
		/* FIXME:: endianness!!! */
		write_flash_page(request_ptr->data, request_ptr->u16.size);
	}
	else if (request_ptr->memtype == 'E')
	{
		/* TODO:: Check page boundary. */
		if (0)
		{
			goto fail;
		}
		/* FIXME:: endianness!!! */
		write_eeprom_page(request_ptr->data, request_ptr->u16.size);
	}

	quick_ok_response();
	return;

fail:
	quick_fail_response();
}

static void read_page(void)
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
			}bytes;
			uint16_t size;
		}u16;
		uint8_t memtype;
		uint8_t data[257];	/* 256 + EOP byte */
	} *request_ptr;

	struct response
	{
		uint8_t insync;
		uint8_t data[257];	/* 256 + EOP byte */
	} *response_ptr;

	request_ptr = (struct request*) io_buf;

	/* Inplace endianness fix. */
	request_ptr->u16.size = SWAP_US(request_ptr->u16.size);

	// TODO:: size check!!

	/* Memory to program */
	if (request_ptr->memtype == 'F')
	{
		read_flash_page(request_ptr->data, request_ptr->u16.size);
	}
	else if (request_ptr->memtype == 'E')
	{
		read_eeprom_page(request_ptr->data, request_ptr->u16.size);
	}

	/*
	 * The data is stored into buffer already, therefore we only have shift the
	 * response pointer one byte ahead of data[] just to add the SYNC byte in packet.
	 */
	response_ptr = (struct response*) &request_ptr->memtype;
	response_ptr->insync = Resp_STK_INSYNC;
	response_ptr->data[request_ptr->u16.size] = Sync_CRC_EOP;

	io_send(response_ptr, request_ptr->u16.size);
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
		uint8_t sign_mid;
		uint8_t sign_low;
		uint8_t status;
	} *response_ptr;

	response_ptr = (struct response*) io_buf;

	/* TODO:: check endian ordering */
	response_ptr->sign_high	= SIGNATURE_0;
	response_ptr->sign_mid	= SIGNATURE_1;
	response_ptr->sign_low	= SIGNATURE_2;

	response_ptr->insync = Resp_STK_INSYNC;
	response_ptr->status = Resp_STK_OK;

	io_send(response_ptr, sizeof(*response_ptr));
}


int main(void)
{
	/*
	 * Since we are not using interrupts,
	 * we shall disable them.
	 */
	cli();

//	memset((void*) &programming, 0, sizeof programming); //<- redundant memset; already done by __clear_bss


//	if (!io_connected())
//	{
//		start_app();
//	}
//
	io_init();

	for (;;)
	{
		if (io_recv() < 2)
		{
			continue;
		}

		switch (io_buf[0])
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

		/* Read Page */
		case Cmnd_STK_READ_PAGE:
			read_page();
			break;

		/* Read Device Signature */
		case Cmnd_STK_READ_SIGN:
			read_signature_bytes();
			break;

		/* Get Synchronization */
		case Cmnd_STK_GET_SYNC:
		/* Set Device Parameters */
		case Cmnd_STK_SET_DEVICE:
			quick_ok_response();
			break;

		/* Enter Programming Mode */
		case Cmnd_STK_ENTER_PROGMODE:
			programming.mode = 1;
			quick_ok_response();
			break;

		/* Leave Programming Mode */
		case Cmnd_STK_LEAVE_PROGMODE:
			programming.mode = 0;
			quick_ok_response();
			break;

		default:
			quick_fail_response();
			break;
		}
	}

	return 0;
}

