#include <stdio.h>
#include "CRC.h"

uint32_t crc32(uint32_t crc, uint8_t byte)
{
	int8_t i;
	crc = crc ^ byte;
	for(i=7; i>=0; i--)
	crc=(crc>>1)^(0xedb88320ul & (-(crc&1)));
	return(crc);
}

void crc32_array(uint8_t *data, uint16_t n_bytes, uint32_t* crc) 
{
	*crc = 0xffffffff;
	for(uint16_t i=0;i<n_bytes;i++)
	{
		*crc = crc32(*crc,*data);
		data++;
	}
}