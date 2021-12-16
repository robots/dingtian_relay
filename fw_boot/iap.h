#ifndef IAP_h_
#define IAP_h_

#include <stdint.h>

#define IAP_MAGIC  0x46574252
#define IAP_FLAG_COMPRESS  0x80

typedef void (*iap_data_cb_t)(uint32_t* Address, uint32_t* Data, uint16_t DataLength);

struct iap_header_t {
	uint32_t magic;
	uint8_t brdver;
	uint8_t xtea_mode;
	uint16_t res;
	uint32_t fw_size;
	uint32_t fw_crc;
	uint32_t iv[2];
};

extern struct iap_header_t *iap_header;
extern iap_data_cb_t iap_data_cb;

int iap_check_filesystem(void);

void iap_init_console(void);
void iap_set_datacb(iap_data_cb_t cb);
int iap_write_done(void);
int iap_check_fw(void);
int iap_init(struct iap_header_t *ih);
void iap_set_flash_addr(uint32_t addr);
void iap_write_data(char * ptr, uint32_t len, int last);

#endif
