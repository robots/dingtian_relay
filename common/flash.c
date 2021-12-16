#include "platform.h"
#include "flash.h"

static int flash_status(void)
{
  if (FLASH->SR & FLASH_SR_BSY) {
    return FLASH_BUSY;
  }
  
  if (FLASH->SR & FLASH_SR_WRPRTERR) {
		return FLASH_ERR_WRP;
	}

  if (FLASH->SR & FLASH_SR_PGERR) {
		return FLASH_ERR_PROG;
	}

  return FLASH_OK;
}

static int flash_wait(uint32_t to)
{ 
  int status = FLASH_OK;
  
  while (to) {
		status = flash_status();

		if (status != FLASH_BUSY)
			break;

    to--;
  }
  
  if (to == 0x00) {
    return FLASH_TIMEOUT;
  }

  return status;
}

void flash_unlock(void)
{
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;
}

void flash_lock(void)
{
	FLASH->CR |= FLASH_CR_LOCK;
}

int flash_erase_page(uint32_t addr)
{
  int status = flash_wait(0x8000);
  
	if (status == FLASH_OK) {
		FLASH->CR |= FLASH_CR_PER;
		FLASH->AR = addr;
		FLASH->CR |= FLASH_CR_STRT;

    status = flash_wait(0x8000);
		if (status == FLASH_OK) {
			FLASH->CR &= ~FLASH_CR_PER;
		}
  }

  return status;
}

int flash_program(uint32_t addr, uint16_t data)
{
  int status = flash_wait(0x8000);
  
	if (status == FLASH_OK) {
		FLASH->CR |= FLASH_CR_PG;

		*(volatile uint16_t *)addr = data;

    status = flash_wait(0x8000);
		if (status == FLASH_OK) {
			FLASH->CR &= ~FLASH_CR_PG;
		}
  }

  return status;

}

int flash_program32(uint32_t addr, uint32_t data)
{
	int status;

	status = flash_program(addr, data & 0xffff);

	if (status != FLASH_OK) return status;

	return flash_program(addr+2, data >> 16);
}

void flash_rdp_program(void)
{
  int status = flash_wait(0x8000);
  
	if (status == FLASH_OK) {
		FLASH->OPTKEYR = FLASH_KEY1;
		FLASH->OPTKEYR = FLASH_KEY2;
		FLASH->CR |= FLASH_CR_OPTER;
		FLASH->CR |= FLASH_CR_STRT;

    status = flash_wait(0x8000);
		if (status == FLASH_OK) {
      FLASH->CR &= ~FLASH_CR_OPTER;
      FLASH->CR |= FLASH_CR_OPTPG; 
			OB->RDP = RDP_KEY;

			status = flash_wait(0x8000);
			if (status == FLASH_OK) {
        FLASH->CR &= ~FLASH_CR_OPTPG;
			}
		}
	}
}
