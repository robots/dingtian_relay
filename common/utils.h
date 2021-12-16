#ifndef UTILS_h_
#define UTILS_h_

#include <stdint.h>
#include "lwip/ip_addr.h"

uint8_t parse_ipaddr(char* str, struct ip4_addr *addr);
uint8_t is_hex(char c);
uint8_t hex2dec(char c);
unsigned closest_power_of_two(unsigned value);

#endif
