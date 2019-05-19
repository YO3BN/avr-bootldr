/*
 * avr-bootldr
 * main.c
 *
 *  Created on: May 18, 2019
 *      Author: Cristian Ionita
 */


#include <inttypes.h>
#include <string.h>
#include <avr/io.h>
#include <avr/signature.h>
#include <avr/boot.h>

#include "command.h"


#define HW_VER		0x01
#define SW_MAJOR	0x00
#define SW_MINOR	0x01


#ifndef SIGRD
#define SIGRD 5
#warning "!!!!!!!! MCU does not have SIGRD. Using dummy 5 !!!!!!!!"
#endif

uint8_t uart_buf[280];
void (*app_start)(void) = 0x0000;


struct __attribute__ ((packed))
{
	uint16_t address;	/* bytes for EEPROM ; words for FLASH */
	char loaded;
} programming_attributes;



void uart_send(void *pvdata, uint16_t uslen)
{

}


void program_flash_page(uint32_t page, uint8_t *buf)
{
    uint16_t i, w;

    eeprom_busy_wait ();
    boot_page_erase (page);
    boot_spm_busy_wait ();      // Wait until the memory is erased.
    for (i=0; i<SPM_PAGESIZE; i+=2)
    {
        // Set up little-endian word.
        w = *buf++;
        w += (*buf++) << 8;

        boot_page_fill (page + i, w);
    }
    boot_page_write (page);     // Store buffer in flash page.
    boot_spm_busy_wait();       // Wait until the memory is written.
    // Reenable RWW-section again. We need this if we want to jump back
    // to the application after bootloading.
    boot_rww_enable ();
}


void program_flash(void *pvdata, uint16_t uslen)
{

}


void program_eeprom(void *pvdata, uint16_t uslen)
{

}

#if 0 /* not yet needed */
void quick_fail_response(void)
{
	struct __attribute__ ((packed)) response
	{
		uint8_t insync;
		uint8_t status;
	} *presponse;

	presponse = (struct response*) uart_buf;

	presponse->insync = Resp_STK_INSYNC;
	presponse->status = Resp_STK_FAILED;

	uart_send(presponse, sizeof(*presponse));
}
#endif

void quick_ok_response(void)
{
	struct __attribute__ ((packed)) response
	{
		uint8_t insync;
		uint8_t status;
	} *presponse;

	presponse = (struct response*) uart_buf;

	presponse->insync = Resp_STK_INSYNC;
	presponse->status = Resp_STK_OK;

	uart_send(presponse, sizeof(*presponse));
}

/*
 * Use this command to try to regain synchronization when sync is lost.
 * Send this command until Resp_STK_INSYNC is received.
 */
void inline get_sync(void)
{
	quick_ok_response();
}


/*
 * The PC sends this command to check if the starterkit is present on
 * the communication channel.
 */
void get_sign_on(void)
{
	struct __attribute__ ((packed)) response
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
void load_address(void)
{
	struct __attribute__ ((packed)) request
	{
		uint8_t cmd;
		union
		{
			uint8_t addr_low;
			uint8_t addr_high;
			uint16_t address;
		} u16;
		uint8_t eop;
	} *prequest;

	prequest = (struct request*) uart_buf;

	programming_attributes.loaded = 1;

	/* TODO:: check for right endian ordering!! */
	programming_attributes.address = prequest->u16.address;

	quick_ok_response();
}


/*
 * Get the value of a valid parameter from the STK500 starterkit.
 * If the parameter is not used, the same parameter will be returned
 * together with a Resp_STK_FAILED response to indicate the error.
 * See the parameters section for valid parameters and their meaning.
 */
void get_parameter(void)
{
	struct __attribute__ ((packed)) request
	{
		uint8_t cmd;
		uint8_t parameter;
		uint8_t eop;
	} *prequest;

	struct __attribute__ ((packed)) response
	{
		uint8_t insync;
		uint8_t value;
		uint8_t status;
	} *presponse;

	prequest = (struct request*) uart_buf;
	presponse = (struct response*) uart_buf;

	/* Assume we are successful, replaced otherwise */
	presponse->insync = Resp_STK_INSYNC;
	presponse->status = Resp_STK_OK;

	switch (prequest->parameter)
	{
	case Parm_STK_HW_VER:
		presponse->value = HW_VER;
		break;

	case Parm_STK_SW_MAJOR:
		presponse->value = SW_MAJOR;
		break;

	case Parm_STK_SW_MINOR:
		presponse->value = SW_MINOR;
		break;

	/* Other parameters not supported yet */
	default:
		presponse->status = Resp_STK_FAILED;
		break;
	}

	uart_send(presponse, sizeof(*presponse));
}


/*
 * Download a block of data to the starterkit and program it in FLASH or EEPROM of the
 * current device. The data block size should not be larger than 256 bytes.
 */
void program_page(void)
{
	struct __attribute__ ((packed)) request
	{
		uint8_t cmd;
		union
		{
			uint8_t bytes_high;
			uint8_t bytes_low;
			uint16_t size;
		} u16;
		uint8_t memtype;
		uint8_t data[257];	/* Including EOP byte */
	} *prequest;


	prequest = (struct request*) uart_buf;

	/* Memory to program */
	if (prequest->memtype == 'F')
	{
		program_flash(prequest->data, prequest->u16.size);
	}
	else if (prequest->memtype == 'E')
	{
		program_eeprom(prequest->data, prequest->u16.size);
	}

	quick_ok_response();
}

/*
 * Read signature bytes.
 */
void read_signature_bytes(void)
{
	struct __attribute__ ((packed)) response
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


/*
 * Read Oscillator calibration byte.
 */
void read_osc_cal(void)
{
	struct __attribute__ ((packed)) response
	{
		uint8_t insync;
		uint8_t osc_cal_byte;
		uint8_t status;
	} *presponse;

	presponse = (struct response*) uart_buf;

	presponse->insync = Resp_STK_INSYNC;
	presponse->status = Resp_STK_OK;

	/* TODO:: not sure if this should be the answer */
	presponse->osc_cal_byte = boot_signature_byte_get(0x0001);

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
//	uart_recv();

	switch (uart_buf[0])
	{

	/* Check if Starterkit Present */
	case Cmnd_STK_GET_SIGN_ON:
		get_sign_on();
		break;

	/* Get Synchronization */
	case Cmnd_STK_GET_SYNC:
		get_sync();
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

	/* Read Oscillator Calibration Byte */
	case Cmnd_STK_READ_OSCCAL:
		read_osc_cal();
		break;

	default:
		quick_ok_response();
		break;
	}

	return 0;
}

