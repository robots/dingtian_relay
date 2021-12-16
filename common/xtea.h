#ifndef XTEA_h_
#define XTEA_h_

enum {
	XTEA_MODE_NONE,
	XTEA_MODE_ECB,
	XTEA_MODE_CBC,
	XTEA_MODE_CFB,
};

extern const uint32_t xtea_iv_ff[2];

void xtea_setkey(const uint32_t *key);
void xtea_setmode(const uint8_t mode);
void xtea_setiv(const uint32_t *iv);

void xtea_dec(uint32_t *data);

#endif
