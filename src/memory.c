#include <inttypes.h>
#include <avr/boot.h>

extern volatile uint16_t prog_address;

void program_flash_page(uint32_t page, uint8_t *buf, uint16_t uslen)
{
	uint16_t i, flash_word;

	eeprom_busy_wait();
	boot_page_erase(page);

	/* Wait until the memory is erased */
	boot_spm_busy_wait();

	for (i = 0; i < SPM_PAGESIZE; i += 2)
	{
		/* Set up little-endian word */
		flash_word = *buf++;
		flash_word += (*buf++) << 8;

		boot_page_fill(page + i, flash_word);
	}
	/* Store buffer in flash page */
	boot_page_write(page);
	/* Wait until the memory is written */
	boot_spm_busy_wait();
	/*
	 * Re-enable RWW-section again.
	 * We need this if we want to jump back to the application after bootloading.
	 */
	boot_rww_enable();
}


void program_flash(void *pvdata, uint16_t uslen)
{

}


void program_eeprom(const void *pvdata, uint16_t uslen)
{
	eeprom_write_block(pvdata, &prog_address, uslen);
}
