#include <stdint.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

extern volatile struct
{
	char enabled;
	uint16_t address;	/* bytes for EEPROM ; words for FLASH */
} programming;

void write_flash_page(uint8_t *buf, uint16_t uslen)
{
	//TODO:: even up the uslen?
	uint16_t i, flash_word;

	eeprom_busy_wait();
	boot_page_erase(programming.address);

	/* Wait until the memory is erased */
	boot_spm_busy_wait();

	for (i = 0; (i < SPM_PAGESIZE) || (i <= uslen); i += 2)
	{
		/* Set up little-endian word */
		flash_word = *buf++;
		flash_word += (*buf++) << 8;
		/* Fill the hardware buffer */
		boot_page_fill(programming.address + i, flash_word);
	}
	/* Store buffer in flash page */
	boot_page_write(programming.address);
	/* Wait until the memory is written */
	boot_spm_busy_wait();
	/*
	 * TODO:: maybe move this at the end of programming/app_start section.
	 * Re-enable RWW-section again.
	 * We need this if we want to jump back to the application after bootloading.
	 */
	boot_rww_enable();
}


void read_flash_page(void *pvdata, uint16_t uslen)
{
	uint8_t *p = pvdata;
	//TODO move into load_address()??
	programming.address *= 2;

	while (uslen)
	{
#if defined (RAMPZ)
		*p = pgm_read_byte_far(programming.address + 0x10000);
#else
		*p = pgm_read_byte(programming.address);
#endif
		p++;
		uslen--;
	}
}


void write_eeprom_page(const void *pvdata, uint16_t uslen)
{
	eeprom_write_block(pvdata, (void*) &programming.address, uslen);
}


void read_eeprom_page(void *pvdata, uint16_t uslen)
{
	//TODO move into load_address()??
	programming.address *= 2;
	eeprom_read_block(pvdata, (void*) programming.address, uslen);
}
