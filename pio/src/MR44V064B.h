#ifndef _MR44V064B_H_
#define _MR44V064B_H_

#include <stdint.h>
#include "Globals.h"

#define FRAM_adress 0x50   // 7 Byte address 
#define Controller_paramter_offset 0x00 
#define Statistics_offset sizeof(controller_t) + Controller_paramter_offset
#define basic_config_offset sizeof(statistics_t) + Statistics_offset
class MR44V064B_Base
{
    public:
    void write_array(uint8_t *data,uint16_t adress,uint16_t length);
    void read_array(uint8_t *data,uint16_t adress,uint16_t length);
    void write_controller_parameters(p_controller_t data,uint16_t adress);
    void read_controller_parameters(p_controller_t data,uint16_t adress);
    void write_statistics(p_statistics_t data,uint16_t adress);
    void read_statistics(p_statistics_t data,uint16_t adress);
    void write_basic_config(p_basic_config_t data,uint16_t adress);
    void read_basic_config(p_basic_config_t data,uint16_t adress);



    protected:


};


#endif /* _MR44V064B_H_ */