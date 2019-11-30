#ifndef _MR44V064B_H_
#define _MR44V064B_H_

#include <stdint.h>

typedef struct controller {
	float Kp;
    float Setpoint;
    float dead_zone;
    float min_open_time;
    double setpoint_carbondioxide;
	uint16_t cv;
    uint16_t cycle_time;
    uint16_t calc_time;
    uint32_t open_time;
    uint32_t close_time;
    bool compressed_gas_bottle;
    uint32_t crc32;
} controller_t,*p_controller_t;

typedef struct statistics {
    float opening_time;
    uint32_t times_open;
    uint32_t crc32;
}
statistics_t,*p_statistics_t;

typedef struct basic_config {
    uint8_t use_regulator;
    uint16_t zero_value_sensor;
    float value_red;
    float value_blue;
    float value_turkis;
    float value_green;
    double faktor_pressure;
    uint32_t crc32;
}
basic_config_t,*p_basic_config_t;

#define FRAM_adress 0x50   // 7 Byte address 
#define Controller_paramter_offset 0x00 
#define Statistics_offset sizeof(controller_t) + Controller_paramter_offset
#define basic_config_offset sizeof(statistics_t) + Statistics_offset

#define FRAM_CONFIG_SIZE (basic_config_offset + sizeof(basic_config_t))

class MR44V064B_Base
{
    public:
    void write_array(uint8_t *data,uint16_t adress,uint16_t length);
    void read_array(uint8_t *data,uint16_t adress,uint16_t length);

    /* last 4 bytes is crc32. Returns true when the read crc matches the calculated one */
    bool read_array_crc32(uint8_t *data,uint16_t adress,uint16_t length);

    /* last 4 bytes of buffer will be replaced by crc32 of previous bytes */
    void write_array_crc32(uint8_t *data,uint16_t adress,uint16_t length);

    void write_controller_parameters(p_controller_t data,uint16_t adress);
    bool read_controller_parameters(p_controller_t data,uint16_t adress);
    void write_statistics(p_statistics_t data,uint16_t adress);
    bool read_statistics(p_statistics_t data,uint16_t adress);
    void write_basic_config(p_basic_config_t data,uint16_t adress);
    bool read_basic_config(p_basic_config_t data,uint16_t adress);
    void reset_settings();


    bool test_fram();


    protected:


};


#endif /* _MR44V064B_H_ */