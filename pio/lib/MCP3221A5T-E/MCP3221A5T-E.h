#ifndef _MCP3221A5T-E_H_
#define _MCP3221A5T-E_H_

#include "I2Cdev.h"


#define I2C_adress 0x4D


#define MCP3221A5T_DEFAULT_ADDRESS 0x9A


class MCP3221_Base
{
    public:
    MCP3221_Base();
    MCP3221_Base(uint8_t address);
    void MCP3221_init();

    uint16_t MCP3221_getdata();

    protected:
    uint8_t devAddr;
    uint8_t buffer[14];
};


#endif /* _MPU6050_H_ */