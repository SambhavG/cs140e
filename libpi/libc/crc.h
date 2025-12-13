#ifndef __CRC_H__
#define __CRC_H__
#include <stdint.h>

uint32_t our_crc32(const void *buf, unsigned size);
uint32_t our_crc32_inc(const void *buf, unsigned size, uint32_t crc);

#endif
