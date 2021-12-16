
#include <stdlib.h>
#include <string.h>

#include "utils.h"

uint8_t parse_ipaddr(char* str, struct ip4_addr *addr)
{
	uint32_t temp_addr = 0;
	char *tmp = str;
	char *endptr;

	uint8_t part = 0;

	for (part = 0; part < 4; part++) {
		uint32_t o = strtol(tmp, &endptr, 10);

		if (o > 255) {
			return 1; // error
		}

		temp_addr |= (o & 0xff) << (part * 8);

		if ((part < 3) && (endptr[0] != '.')) {
			return 1; // error
		}

		tmp = endptr+1;
	}

	addr->addr = temp_addr;

	return 0;
}

uint8_t is_hex(char c)
{

	if (('0' <= c && c <= '9') ||	('a' <= c && c <= 'f') ||	('A' <= c && c <= 'F')) {
		return 1;
	}

	return 0;
}

uint8_t hex2dec(char c)
{
	if ('0' <= c && c <= '9') {
		return c - '0';
	} else if ('a' <= c && c <= 'f') {
		return c - 'a' + 10;
	} else if ('A' <= c && c <= 'F') {
		return c - 'A' + 10;
	}

	return 0;
}

unsigned closest_power_of_two(unsigned value)
{
    unsigned above = (value - 1); // handle case where input is a power of two
    above |= above >> 1;          // set all of the bits below the highest bit
    above |= above >> 2;
    above |= above >> 4;
    above |= above >> 8;
    above |= above >> 16;
    ++above;                      // add one, carrying all the way through
                                  // leaving only one bit set.
    unsigned below = above >> 1;  // find the next lower power of two.
		return below;

//    return (above - value) < (value - below) ? above : below;
}

void hex2buf(char *out, uint8_t *buf, uint32_t len)
{
	while(len--) {
		tfp_sprintf(out, "%02x ", buf[0]);
		buf++;
		out+=3;
	}
}
