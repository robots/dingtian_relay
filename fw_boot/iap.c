#include "platform.h"

#include <string.h>

#include "config.h"
#include "console.h"
#include "flash_if.h"
#include "ee_config.h"
#include "xtea.h"

#if HAVE_SPIFFS
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "spiffs.h"
#include "spiflash.h"

#define LOG_PAGE_SIZE       256
#endif

#if IAP_HAVE_COMP
#include "iap_comp.h"
#endif
#include "iap.h"

uint32_t iap_flash_addr;
static uint8_t LeftBytesTab[8] __attribute__ ((aligned (4)));
static uint8_t LeftBytes = 0;

static const uint32_t xtea_key[4] = XTEA_KEY;

uint32_t iap_sizeleft;

struct iap_header_t *iap_header = (struct iap_header_t *)USER_FLASH_INFOBLOCK_ADDRESS;

iap_data_cb_t iap_data_cb = flash_if_write;

#if DEBUG
static struct console_command_t iap_cmd;

void iap_init_console(void)
{
#ifdef STM32F1
	RCC->AHBENR |= RCC_AHBENR_CRCEN;
#endif
#ifdef STM32F4
	RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
#endif

	console_add_command(&iap_cmd);
}

static void iap_lock(void)
{
	flash_unlock();
	// disable readout
#ifdef STM32F1
	flash_rdp_program();
#endif
#ifdef STM32F4
	FLASH_OB_RDPConfig(OB_RDP_Level_1);
#endif
//	// enable write protect on pages ... 0 to 29 .... 30*2kB = 60kB (bootloader)
//	FLASH_EnableWriteProtection(0x7FFF);
}
#endif

void iap_set_datacb(iap_data_cb_t cb)
{
	iap_data_cb = cb;
}


int iap_check_fw(void)
{
	uint32_t crc;

	if (iap_header->magic != IAP_MAGIC) {
		return 0;
	}

#ifdef STM32F1
	RCC->AHBENR |= RCC_AHBENR_CRCEN;
#endif
#ifdef STM32F4
	RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
#endif

	CRC->CR = 1;
	uint32_t *addr = (uint32_t *)USER_FLASH_FIRST_PAGE_ADDRESS;
	for (uint32_t cnt = 0; cnt < iap_header->fw_size/4; cnt++) {
		CRC->DR = __RBIT(addr[0]);
		addr++;
	}
	crc = __RBIT(CRC->DR) ^ 0xffffffff;

	if (crc == iap_header->fw_crc) {
		return 1;
	} else {
		return 0;
	}
}

#if HAVE_SPIFFS
u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
u8_t spiffs_fds[256];

static s32_t my_spiffs_read(u32_t addr, u32_t size, u8_t *dst) {
	spiflash_read(addr, dst, size);
	return SPIFFS_OK;
}

static void iap_crc_cb(uint32_t *addr, uint32_t *data, uint16_t data_len)
{
	if (*addr == USER_FLASH_FIRST_PAGE_ADDRESS) {
		RCC->AHB1ENR |= RCC_AHB1Periph_CRC;
		CRC->CR = 1;
	}

	for (uint32_t i = 0; i < data_len; i++) {
		CRC->DR = __RBIT(data[i]);
		*addr += 4;
	}
}

int iap_check_filesystem(void)
{
	int fd;
	int ret = 0;
	struct iap_header_t ih;

	if (spiflash_init() == 1) {
		return 1;
	}

	spiffs spiffs_fs;
	spiffs_config cfg;

	cfg.phys_addr = 0; // start spiffs at start of spi flash
	cfg.phys_size = spiflash_get_size();

	cfg.log_block_size = 65536;
	cfg.log_page_size = LOG_PAGE_SIZE;

	cfg.hal_read_f = my_spiffs_read;

	ret = SPIFFS_mount(&spiffs_fs, &cfg, spiffs_work_buf, spiffs_fds, sizeof(spiffs_fds), NULL, 0, 0);
	if (ret != 0) {
		return 2;
	}
#if 0
	{
		spiffs_DIR d;
		struct spiffs_dirent e;

		SPIFFS_opendir(&spiffs_fs, "/", &d);
		while ((SPIFFS_readdir(&d, &e))) {
			console_printf(CON_ERR, "%s [%04x] size:%i\n", e.name, e.obj_id, e.size);
		}
		SPIFFS_closedir(&d);

		console_printf(CON_ERR, "\n");
	}
#endif
	// open file
	fd = SPIFFS_open(&spiffs_fs, "bridge_v4_boot.bfw", SPIFFS_RDONLY, 0);
	if (fd < 0) {
		console_printf(CON_ERR, "filesystem: open %d\n", fd);
		return 3;
	}

	console_printf(CON_ERR, "filesystem: found file!\n");

	// read header
	if (SPIFFS_read(&spiffs_fs, fd, &ih, sizeof(struct iap_header_t)) != sizeof(struct iap_header_t)) {
		return 4;
	}

	// check header
	if ((ih.magic != IAP_MAGIC) || (ih.brdver != BOARD_ID) || (ih.fw_size % 8 != 0) || !((ih.xtea_mode & 0x03) > 0 && (ih.xtea_mode & 0x03) < XTEA_MODE_CFB)) {
		return 5;
	}

	iap_header = &ih;

	// check/write firmware image...
	int checked = 0;

again:
	if (checked == 1) {
		// check was ok, prepare for write
		FLASH_If_Init();

		uint32_t addr = USER_FLASH_INFOBLOCK_ADDRESS;
		FLASH_If_Write(&addr, (uint32_t *)&ih, (sizeof(struct iap_header_t)+3)/4);
	}

	iap_set_flash_addr(USER_FLASH_FIRST_PAGE_ADDRESS);
	xtea_setmode(ih.xtea_mode & 0x03);
	if (memcmp(ih.iv, xtea_iv_ff, 8) == 0) {
		uint32_t iv[2];
		memcpy(((uint8_t *)iv)+2, ee_config.mac, 6);
		((uint8_t *)iv)[0] = BOARD_ID;
		((uint8_t *)iv)[1] = 0xa5;
		xtea_setiv(iv);
	} else {
		xtea_setiv(ih.iv);
	}
	if (ih.xtea_mode & IAP_FLAG_COMPRESS) {
		return 6;
	}

	xtea_setkey(xtea_key);
	if (checked == 0) {
		iap_set_datacb(iap_crc_cb);
	} else {
		iap_set_datacb(FLASH_If_Write);
	}
	iap_sizeleft = ih.fw_size;
	LeftBytes = 0;

	// read and process fwfile.
	char buffer[256];
	SPIFFS_lseek(&spiffs_fs, fd, sizeof(struct iap_header_t), SPIFFS_SEEK_SET);

	do {
		int count = SPIFFS_read(&spiffs_fs, fd, &buffer, 256);
		if (count <= 0) {
			break;
		}

		iap_write_data(buffer, count, count==256?0:1);

		if (iap_sizeleft == 0) {
			break;
		}
	} while (1);

	if (checked == 0) {
		uint32_t crc = __RBIT(CRC->DR) ^ 0xffffffff;
		if (crc != ih.fw_crc) {
			console_printf(CON_ERR, "filesystem: crc expected %08x got %08x\n", ih.fw_crc, crc);
			return 7;
		}

		// image checked ok
		checked = 1;
		console_printf(CON_ERR, "filesystem: checked ok!\n");
		goto again;
	}

	console_printf(CON_ERR, "filesystem: flashed ok!\n");
	return 0;
}
#endif

int iap_init(struct iap_header_t *ih)
{
	int res = 1;

	if (ih->magic == IAP_MAGIC) {
		if (ih->brdver == BOARD_ID) {
			if (ih->fw_size % 8 == 0) {

#if DEBUG
				if ((ih->xtea_mode & 0x03) < XTEA_MODE_CFB)
#else
				if ((ih->xtea_mode & 0x03) > 0 && (ih->xtea_mode & 0x03) < XTEA_MODE_CFB)
#endif
				{
#if DEBUG
console_printf(CON_ERR, "header ok, xteamode=%d\n",ih->xtea_mode);
#endif

					/* init flash */
					flash_if_init();

					if (flash_if_erase_page(USER_FLASH_INFOBLOCK_ADDRESS) == FLASH_OK) {
						uint32_t addr = USER_FLASH_INFOBLOCK_ADDRESS;
						flash_if_write(&addr, (uint32_t *)ih, (sizeof(struct iap_header_t)+3)/4);

						iap_set_datacb(flash_if_write);
						iap_set_flash_addr(USER_FLASH_FIRST_PAGE_ADDRESS);

						xtea_setmode(ih->xtea_mode & 0x03);
						if (memcmp(ih->iv, xtea_iv_ff, 8) == 0) {
							uint32_t iv[2];
							memcpy(((uint8_t *)iv)+2, ee_config.mac, 6);
							((uint8_t *)iv)[0] = BOARD_ID;
							((uint8_t *)iv)[1] = 0xa5;
							xtea_setiv(iv);
						} else {
							xtea_setiv(ih->iv);
						}
						if (ih->xtea_mode & IAP_FLAG_COMPRESS) {
#if IAP_HAVE_COMP
							iap_comp_init();
#else
							return 1;
#endif
						}
						xtea_setkey(xtea_key);
						iap_sizeleft = ih->fw_size;
						LeftBytes = 0;
#if DEBUG
						console_printf(CON_ERR, "Header done\n");
#endif

						// we are ready to go
						res = 0;
					}
#if DEBUG
				} else {
console_printf(CON_ERR, "unsupported xteamode\n");
#endif
				}
#if DEBUG
			} else {
console_printf(CON_ERR, "Invalid size\n");
#endif
			}
#if DEBUG
		} else {
console_printf(CON_ERR, "Invalid boardid\n");
#endif
		}
#if DEBUG
	} else {
console_printf(CON_ERR, "Invalid magic\n");
#endif
	}

	return res;
}

int iap_write_done(void)
{
	return !iap_sizeleft;
}

void iap_set_flash_addr(uint32_t addr)
{
	iap_flash_addr = addr;
}

void iap_write_data(char * ptr, uint32_t len, int last)
{
	uint32_t count, i=0, j=0;

	if (iap_sizeleft == 0)
		return;

#if DEBUG
	//console_printf(CON_ERR, "iap_write sizeleft %d, data in %d\n", iap_sizeleft, len);
#endif

	/* check if any left bytes from previous packet transfer*/
	/* if it is the case do a concat with new data to create a 32-bit word */
	if (LeftBytes) {
		while (LeftBytes < 8) {
			if(len > j) {
				LeftBytesTab[LeftBytes++] = *(ptr+j);
			} else {
				// packet was too small to fill this chunk, retry another time
				return;
			}
			j++;
		}

		xtea_dec((uint32_t *)LeftBytesTab);
		if (iap_header->xtea_mode & IAP_FLAG_COMPRESS) {
#if IAP_HAVE_COMP
			if (iap_comp_decompress(&iap_flash_addr, LeftBytesTab, 8) > 0) {
				iap_sizeleft = 0;
			}
#endif
		} else {
			iap_data_cb(&iap_flash_addr, (uint32_t *)(LeftBytesTab), 2);
			iap_sizeleft -= 4*2;
		}

		LeftBytes = 0;

		if (iap_sizeleft == 0)
			return;

		if (len > j) {
			/* update data pointer */
			ptr += j;
			len -= j;
		}
	}

	/* write received bytes into flash */
	count = len / 8;

	/* check if remaining bytes < 4 */
	i = len % 8;
	if (i > 0) {
		if (!last) {
			/* store bytes in LeftBytesTab */
			LeftBytes = 0;
			for(;i > 0; i--) {
				LeftBytesTab[LeftBytes++] = ptr[len-i];
			}
		} else {
			count++;
		}
	}

	if (count) {
		uint32_t *p = (uint32_t *)ptr;
		for (uint32_t c = 0; c < count; c++) {
			xtea_dec(p);
			if (iap_header->xtea_mode & IAP_FLAG_COMPRESS) {
#if IAP_HAVE_COMP
				if (iap_comp_decompress(&iap_flash_addr, (uint8_t *)p, 8) > 0) {
					iap_sizeleft = 0;
					return;
				}
#endif
			} else {
				iap_data_cb(&iap_flash_addr, p, 2);
				iap_sizeleft -= 4*2;
			}
			if (iap_sizeleft == 0)
				break;
			p += 2;
		}
	}
}

#if DEBUG
static uint8_t iap_cmd_handler(struct console_session_t *cs, char **args)
{
	char out[CONSOLE_CMD_OUTBUF];
	uint32_t len = 0;

	if (args[0] == NULL) {
		len = tfp_sprintf(out, "Valid: sum\n");
		console_session_output(cs, out, len);
		return 1;
	}

	if (strcmp(args[0], "sum") == 0) {
		uint32_t crc;
		CRC->CR = 1;
		uint32_t *addr = (uint32_t *)USER_FLASH_FIRST_PAGE_ADDRESS;
		for (uint32_t cnt = 0; cnt < iap_header->fw_size/4; cnt++)
		{
			CRC->DR = __RBIT(addr[0]);
			addr++;
		}
		crc = __RBIT(CRC->DR) ^ 0xffffffff;
		len = tfp_sprintf(out, "Crc in header is: %04x\n", iap_header->fw_crc);
		len += tfp_sprintf(out+len, "Crc of image is: %04x\n", crc);
		console_session_output(cs, out, len);
	} else if (strcmp(args[0], "sum") == 0) {
		len += tfp_sprintf(out+len, "Locking\n");
		console_session_output(cs, out, len);

		iap_lock();
	} else if (strcmp(args[0], "erase") == 0) {

		flash_if_init();
		flash_if_erase_page(USER_FLASH_INFOBLOCK_ADDRESS);
	} else if (strcmp(args[0], "status") == 0) {
#ifdef STM32F1
		len += tfp_sprintf(out+len, "User option byte %04x\n", FLASH_GetUserOptionByte());
		len += tfp_sprintf(out+len, "Write protection byte %04x\n", FLASH_GetWriteProtectionOptionByte());
#endif
		console_session_output(cs, out, len);
	}

	return 0;
}

static struct console_command_t iap_cmd = {
	"iap",
	iap_cmd_handler,
	"iap",
	"iap",
	NULL
};
#endif
