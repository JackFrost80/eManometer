#include "I2Cdev.h"
#include "Globals.h"
#include "MR44V064B.h"

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

void MR44V064B_Base::read_controller_parameters(p_controller_t data,uint16_t adress)
{
    MR44V064B_Base::read_array((uint8_t *) data, adress, sizeof(controller_t));

}
void MR44V064B_Base::read_statistics(p_statistics_t data,uint16_t adress)
{
    MR44V064B_Base::read_array((uint8_t *) data, adress, sizeof(statistics_t));

}

void MR44V064B_Base::write_controller_parameters(p_controller_t data,uint16_t adress)
{
    MR44V064B_Base::write_array((uint8_t *) data, adress, sizeof(controller_t));

}
void MR44V064B_Base::write_statistics(p_statistics_t data,uint16_t adress)
{
    MR44V064B_Base::write_array((uint8_t *) data, adress, sizeof(statistics_t));

}

void MR44V064B_Base::write_basic_config(p_basic_config_t data,uint16_t adress)
{
    MR44V064B_Base::write_array((uint8_t *) data, adress, sizeof(basic_config_t));

}
void MR44V064B_Base::read_basic_config(p_basic_config_t data,uint16_t adress)
{
    MR44V064B_Base::read_array((uint8_t *) data, adress, sizeof(basic_config_t));

}