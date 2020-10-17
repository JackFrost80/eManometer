#include <Wire.h>
#include "Globals.h"
#include "MR44V064B.h"

uint32_t crc32_for_byte(uint32_t r) {
  for(int j = 0; j < 8; ++j)
    r = (r & 1? 0: (uint32_t)0xEDB88320L) ^ r >> 1;
  return r ^ (uint32_t)0xFF000000L;
}

uint32_t crc32_(uint32_t crc, uint8_t byte)
/*******************************************************************/
{
int8_t i;
  crc = crc ^ byte;
  for(i=7; i>=0; i--)
    crc=(crc>>1)^(0xedb88320ul & (-(crc&1)));
  return(crc);
}

void crc32_array_(uint8_t *data, uint16_t n_bytes, uint32_t* crc) {
  *crc = 0xffffffff;
  for(uint16_t i=0;i<n_bytes;i++)
  {
      *crc = crc32_(*crc,*data);
      data++;
  }
}


void MR44V064B_Base::write_array(uint8_t *data,uint16_t adress,uint16_t length)
{
    Wire.beginTransmission(FRAM_adress);
    Wire.write((uint8_t)(adress>>8));
    Wire.write((uint8_t) (adress & 0xFF));
    for(uint16_t i =0;i<length;i++)
    {
        Wire.write(*data);
        data++;
    }
    Wire.endTransmission();
}

void MR44V064B_Base::read_array(uint8_t *data,uint16_t adress,uint16_t length)
{
    Wire.beginTransmission(FRAM_adress);
    Wire.write((uint8_t)(adress>>8));
    Wire.write((uint8_t) (adress & 0xFF));
    Wire.endTransmission();
    Wire.requestFrom(FRAM_adress,length,1);
    while(Wire.available())
    {
        *data = Wire.read();
        data++;
    }
}

bool MR44V064B_Base::read_array_crc32(uint8_t *data,uint16_t adress,uint16_t length)
{
    read_array(data, adress, length);

    uint32_t calculated_crc = 0xffffffff;

    crc32_array_(data, length - sizeof(uint32_t), &calculated_crc);

    uint32_t* ptr = (uint32_t*) (data + length - sizeof(uint32_t));

    uint32_t crc = *ptr;

    return calculated_crc == crc;
}

void MR44V064B_Base::write_array_crc32(uint8_t *data,uint16_t adress,uint16_t length)
{
    uint32_t calculated_crc = 0xffffffff;

    crc32_array_(data, length - sizeof(uint32_t), &calculated_crc);

    uint32_t* ptr = (uint32_t*) (data + length - sizeof(uint32_t));

    *ptr = calculated_crc;

    write_array(data, adress, length);
}

bool MR44V064B_Base::read_controller_parameters(p_controller_t data,uint16_t adress)
{
   return read_array_crc32((uint8_t *) data, adress, sizeof(controller_t));
}

bool MR44V064B_Base::read_statistics(p_statistics_t data,uint16_t adress)
{
    return read_array_crc32((uint8_t *) data, adress, sizeof(statistics_t));
}

void MR44V064B_Base::write_controller_parameters(p_controller_t data,uint16_t adress)
{
    MR44V064B_Base::write_array_crc32((uint8_t *) data, adress, sizeof(controller_t));
}

void MR44V064B_Base::write_statistics(p_statistics_t data,uint16_t adress)
{
    MR44V064B_Base::write_array_crc32((uint8_t *) data, adress, sizeof(statistics_t));
}

void MR44V064B_Base::write_basic_config(p_basic_config_t data,uint16_t adress)
{
    MR44V064B_Base::write_array_crc32((uint8_t *) data, adress, sizeof(basic_config_t));
}

bool MR44V064B_Base::read_basic_config(p_basic_config_t data,uint16_t adress)
{
    return read_array_crc32((uint8_t *) data, adress, sizeof(basic_config_t));
}

void MR44V064B_Base::reset_settings()
{
    Wire.beginTransmission(FRAM_adress);
    Wire.write(0);
    Wire.write(0);
    for(uint16_t i = 0; i < FRAM_CONFIG_SIZE; i++)
    {
        Wire.write(0);
    }
    Wire.endTransmission();
}

bool MR44V064B_Base::test_fram()
{
    uint32_t testaddr = 0xfffc;
    uint32_t zero = 0;
    uint32_t deadbeef = 0xDEADBEEF;

    write_array((uint8_t*) &zero, testaddr, 4);

    zero = 0xdeadbeef;
    read_array((uint8_t*) &zero, testaddr, 4);

    if (zero != 0) {
        return false;
    }

    write_array((uint8_t*) &deadbeef, testaddr, 4);

    deadbeef = 0;

    read_array((uint8_t*) &deadbeef, testaddr, 4);

    if (deadbeef != 0xDEADBEEF) {
        return false;
    }

    return true;
}