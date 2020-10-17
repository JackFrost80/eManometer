#ifndef CRC_H
#define CRC_H

#include <stdio.h>

uint32_t crc32(uint32_t crc, uint8_t byte);
void crc32_array(uint8_t *data, uint16_t n_bytes, uint32_t* crc);

#endif