#include "platform.h"
#include "flash_if.h"

#if DEBUG
#include "console.h"
#endif

#ifdef STM32F4
const uint16_t flash_if_sector[] = {
	0x0000,
	0x0008,
	0x0010,
	0x0018,
	0x0020,
	0x0028,
	0x0030,
	0x0038,
	0x0040,
	0x0048,
	0x0050,
	0x0058,
	0x0080,
	0x0088,
	0x0090,
	0x0098,
	0x00A0,
	0x00A8,
	0x00B0,
	0x00B8,
	0x00C0,
	0x00C8,
	0x00D0,
	0x00D8,
};
#endif

void flash_if_init(void)
{
	flash_unlock();
}

int flash_if_erase_page(uint32_t addr)
{
	int fs = FLASH_OK;

#ifdef STM32F1
	if ((addr & (FLASH_PAGE_SIZE-1)) == 0) {
		fs = flash_erase_page(addr);
	}
#endif
#ifdef STM32F4
	if (addr & 0xffff) {
		return fs;
	}
	addr = addr - 0x08000000;
	addr >>= 16;
#if DEBUG
	console_printf(CON_ERR, "addr %d \r\n", addr);
#endif

	if (addr == 0) {
		return fs;
	}

	if (addr == 1) {
		fs = flash_erase_sector(flash_if_sector[4], VoltageRange_3);
		return fs;
	} else if ((addr % 2) == 1) {
		return fs;
	} else {
#if DEBUG
			console_printf(CON_ERR, "Erasing sector %d \r\n", 4+(addr/2));
#endif
		fs = flash_erase_sector(flash_if_sector[4 + (addr / 2)], VoltageRange_3);
	}
#endif
	return fs;
}

void flash_if_write(uint32_t *FlashAddress, uint32_t *Data, uint16_t DataLength)
{
	uint32_t i = 0;

	for (i = 0; i < DataLength; i++) {
		if (*FlashAddress <= (USER_FLASH_END_ADDRESS-4)) {
			if (flash_if_erase_page(*FlashAddress) != FLASH_OK) {
#if DEBUG
				console_printf(CON_ERR, "Erase page %08x failed \r\n", *FlashAddress);
#endif
			}

			if (flash_program32(*FlashAddress, *(uint32_t*)(Data + i)) == FLASH_OK) {
				uint32_t s = *(uint32_t *)(Data+i);
				uint32_t *d = (uint32_t *) *FlashAddress;
				if (s != *d) {
#if DEBUG
					console_printf(CON_ERR, "Verify addres failed %08x: is %08x should be %08x\r\n", *FlashAddress, *d, s);
#endif
				}
			} else {
#if DEBUG
				console_printf(CON_ERR, "Program address %08x failed \r\n", *FlashAddress);
#endif
			}
			*FlashAddress += 4;
		} else {
			return;
		}
	}
}
