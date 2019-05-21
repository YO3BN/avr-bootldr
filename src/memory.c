#include <stdint.h>
#include <avr/boot.h>

extern volatile uint16_t prog_address;

void program_flash_page(uint8_t *buf, uint16_t uslen)
{
	//TODO:: even up the uslen
	//TODO:: make this function work
	uint16_t i, flash_word;

	eeprom_busy_wait();
	boot_page_erase(prog_address);

	/* Wait until the memory is erased */
	boot_spm_busy_wait();

	for (i = 0; (i < SPM_PAGESIZE) || (i <= uslen); i += 2)
	{
		/* Set up little-endian word */
		flash_word = *buf++;
		flash_word += (*buf++) << 8;
		/* Fill the hardware buffer */
		boot_page_fill(prog_address + i, flash_word);
	}
	/* Store buffer in flash page */
	boot_page_write(prog_address);
	/* Wait until the memory is written */
	boot_spm_busy_wait();
	/*
	 * TODO:: maybe move this at the end of programming/app_start section.
	 * Re-enable RWW-section again.
	 * We need this if we want to jump back to the application after bootloading.
	 */
	boot_rww_enable();
}

void program_eeprom_page(const void *pvdata, uint16_t uslen)
{
	eeprom_write_block(pvdata, (void*) &prog_address, uslen);
}
