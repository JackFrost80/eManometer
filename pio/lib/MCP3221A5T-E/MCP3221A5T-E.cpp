#include "MCP3221A5T-E.h"
#include <Wire.h>
#include <Arduino.h>

#define MCP3221_I2C_ADDRESS 0x4D

MCP3221_Base::MCP3221_Base()
{
    devAddr = MCP3221_I2C_ADDRESS;
}

/** Specific address constructor.
 * @param address I2C address
 * @see MPU6050_DEFAULT_ADDRESS
 * @see MPU6050_ADDRESS_AD0_LOW
 * @see MPU6050_ADDRESS_AD0_HIGH
 */
MCP3221_Base::MCP3221_Base(uint8_t address)
{
    devAddr = address ;
}

void MCP3221_Base::MCP3221_init()
{
    #if 0
    pinMode(D1,INPUT);
    digitalWrite(D1,0);
    for(uint8 i =0;i<18;i++)
    {
        
        delayMicroseconds(10);
        pinMode(D1,OUTPUT);
        delayMicroseconds(10);
        pinMode(D1,INPUT);

    }
    Wire.begin(D2, D1);
    Wire.setClock(100000);
    Wire.setClockStretchLimit(2 * 230);
    #endif
    
}

uint16_t MCP3221_Base::MCP3221_getdata()
{
    uint8_t count = 0;
    Wire.requestFrom(devAddr,2,1);
    while(Wire.available())
    {
        buffer[count] = Wire.read();
        count++;
    }
    return (((int16_t)buffer[0]) << 8) | buffer[1];
}
