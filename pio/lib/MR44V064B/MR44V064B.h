#ifndef _MR44V064B_H_
#define _MR44V064B_H_

#include "I2Cdev.h"
#include "Globals.h"

#define FRAM_adress 0x50   // 7 Byte address 
#define Controller_paramter_offset 0x00 
class MR44V064B_Base
{
    public:
    void write_array(uint8_t *data,uint16_t adress,uint16_t length);
    void read_array(uint8_t *data,uint16_t adress,uint16_t length);
    void write_controller_parameters(p_controller_t data,uint16_t adress);
    void read_controller_parameters(p_controller_t data,uint16_t adress);



    protected:


};


#endif /* _MR44V064B_H_ */