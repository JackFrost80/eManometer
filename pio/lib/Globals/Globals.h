/**************************************************************

    "iSpindel"
    All rights reserverd by S.Lang <universam@web.de>

 **************************************************************/

#ifndef _GLOBALS_H
#define _GLOBALS_H

#pragma once
//#define USE_DMP false
#include <Arduino.h>
#include <Hash.h>

#include <Ticker.h>

#include <I2Cdev.h>


// defines go here
#define FIRMWAREVERSION "0.7.6b"

#define API_FHEM true
#define API_UBIDOTS true
#define API_GENERIC true
#define API_TCONTROL true
#define API_INFLUXDB true
#define API_PROMETHEUS true
#define API_MQTT true

#ifndef DEBUG
#define DEBUG true
#endif

#ifdef NO_CONSOLE
#define CONSOLE(x) \
    do             \
    {              \
    } while (0)
#define CONSOLELN CONSOLE
#define CONSOLEF CONSOLE
#else
#define CONSOLE(...)               \
    do                             \
    {                              \
        Serial.print(__VA_ARGS__); \
    } while (0)
#define CONSOLELN(...)               \
    do                               \
    {                                \
        Serial.println(__VA_ARGS__); \
    } while (0)
#endif

#define PORTALTIMEOUT 300

#define ADCDIVISOR 191.8
#define ONE_WIRE_BUS D6 // DS18B20 on ESP pin12
#define OW_PINS \
    (const uint8_t[]) { D3, D6 }
#define RESOLUTION 12 // 12bit resolution == 750ms update rate
#define OWinterval (760 / (1 << (12 - RESOLUTION)))
#define CFGFILE "/config.json"
#define TKIDSIZE 40

#define MEDIANROUNDSMAX 49
#define MEDIANROUNDSMIN 29
#define MEDIANAVRG MEDIANROUNDSMIN
#define MEDIAN_MAX_SIZE MEDIANROUNDSMAX

#define CBP_ENDPOINT "/api/hydrometer/v1/data"

#define DTUbiDots 0
#define DTThingspeak 1
#define DTCraftBeerPi 2
#define DTHTTP 3
#define DTTcontrol 4
#define DTFHEM 5
#define DTTCP 6
#define DTiSPINDELde 7
#define DTInfluxDB 8
#define DTPrometheus 9
#define DTMQTT 10

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 1
// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#define WIFIENADDR 1
#define RTCVALIDFLAG 0xCAFEBABE

// sleep management
#define RTCSLEEPADDR 5
#define MAXSLEEPTIME 3600UL //TODO
#define EMERGENCYSLEEP (my_sleeptime * 3 < MAXSLEEPTIME ? MAXSLEEPTIME : my_sleeptime * 3)
#define LOWBATT 3.3

#define UNINIT 0


extern float Temperatur, Tilt, Gravity,Pressure,carbondioxide;





extern bool saveConfig();
extern void formatSpiffs();

float scaleTemperature(float t);
String tempScaleLabel(void);

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
uint8_t type_of_display;
uint8_t use_regulator;
uint16_t zero_value_sensor;
uint16_t value_red;
float value_blue;
float value_turkis;
float value_green;
double faktor_pressure;
uint32_t crc32;
}
basic_config_t,*p_basic_config_t;


#endif
