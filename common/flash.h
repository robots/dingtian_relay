#ifndef FLASH_h_
#define FLASH_h_

enum {
	FLASH_OK,
	FLASH_BUSY,
	FLASH_ERR_WRP,
	FLASH_ERR_PROG,
	FLASH_TIMEOUT,
};

void flash_unlock(void);
int flash_erase_page(uint32_t addr);
int flash_program(uint32_t addr, uint16_t data);
int flash_program32(uint32_t addr, uint32_t data);
void flash_lock(void);


#endif
