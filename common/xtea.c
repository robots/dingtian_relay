#include <stdint.h>
#include <string.h>

#include "xtea.h"

const uint32_t xtea_iv_ff[2] = {0xffffffff, 0xffffffff};
uint32_t xtea_iv[2];
uint8_t xtea_mode;
const uint32_t *xtea_key;

static void xtea_dec_(unsigned int num_rounds, uint32_t v[2], uint32_t const key[4]);
static void _xor(uint32_t *out, uint32_t *in1, uint32_t *in2);

void xtea_setkey(const uint32_t *key)
{
	xtea_key = key;
}

void xtea_setmode(const uint8_t mode)
{
	xtea_mode = mode;
}

void xtea_setiv(const uint32_t *iv)
{
	memcpy(xtea_iv, iv, 8);
}

void xtea_dec(uint32_t *data)
{
	if (xtea_mode == XTEA_MODE_NONE) {
		return;
	} else if (xtea_mode == XTEA_MODE_ECB) {
		xtea_dec_(32, (uint32_t *)data, xtea_key);
	} else if (xtea_mode == XTEA_MODE_CBC) {
		uint8_t iv_tmp[8];

		memcpy(iv_tmp, data, 8);

		xtea_dec_(32, (uint32_t *)data, xtea_key);
		_xor(data, data, xtea_iv);

		memcpy(xtea_iv, iv_tmp, 8);
	} else if (xtea_mode == XTEA_MODE_CFB) {
	}
}

/*
void xtea_enc_(unsigned int num_rounds, uint32_t v[2], uint32_t const key[4])
{
	unsigned int i;
	uint32_t v0=v[0], v1=v[1], sum=0, delta=0x9E3779B9;

	for (i=0; i < num_rounds; i++) {
		v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
		sum += delta;
		v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
	}

	v[0]=v0; v[1]=v1;
}
*/

static void xtea_dec_(unsigned int num_rounds, uint32_t v[2], uint32_t const key[4])
{
	unsigned int i;
	uint32_t delta=0x9E3779B9, sum=delta*num_rounds;

	//uint32_t v0=v[0], v1=v[1];
	uint32_t v0, v1;

	memcpy(&v0, &v[0], 4);
	memcpy(&v1, &v[1], 4);

	for (i=0; i < num_rounds; i++) {
		v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
		sum -= delta;
		v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
	}

	memcpy(&v[0], &v0, 4);
	memcpy(&v[1], &v1, 4);
	//v[0]=v0; v[1]=v1;
}


static void _xor(uint32_t *out, uint32_t *in1, uint32_t *in2)
{
	for (int i = 0; i < 8/4; i++) {
		out[i] = in1[i] ^ in2[i];
	}
}

