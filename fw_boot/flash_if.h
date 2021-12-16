#ifndef FLASH_IF_h
#define FLASH_IF_h

#include "platform.h"
#include "flash.h"

#define USER_FLASH_SIZE   (USER_FLASH_END_ADDRESS - USER_FLASH_FIRST_PAGE_ADDRESS)
#define FLASH_PAGE_SIZE   0x800

void flash_if_write(uint32_t* Address, uint32_t* Data, uint16_t DataLength);
int flash_if_erase_page(uint32_t addr);
int8_t flash_if_erase(uint32_t StartSector);
void flash_if_init(void);

#endif
