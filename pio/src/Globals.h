/**************************************************************

    "iSpindel"
    All rights reserverd by S.Lang <universam@web.de>

 **************************************************************/

#ifndef _GLOBALS_H
#define _GLOBALS_H

#pragma once
#include <Arduino.h>
#include <Hash.h>
#include <array>

#include <Ticker.h>

// defines go here
#define FIRMWAREVERSION "0.7.6b"

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
    (const uint8_t[]) { D3 }
#define RESOLUTION 12 // 12bit resolution == 750ms update rate
#define OWinterval (760 / (1 << (12 - RESOLUTION)))
#define CFGFILE "/config.json"
#define TKIDSIZE 40

#define MEDIANROUNDSMAX 49
#define MEDIANROUNDSMIN 29
#define MEDIANAVRG MEDIANROUNDSMIN
#define MEDIAN_MAX_SIZE MEDIANROUNDSMAX

#define CBP_ENDPOINT "/api/hydrometer/v1/data"

enum RemoteAPI {
    API_Off = 0,
    API_Ubidots = 1,
    API_Thingspeak = 2,
    API_CraftBeerPi = 3,
    API_HTTP = 4,
    API_TCP = 5,
    API_InfluxDB = 6,
    API_Prometheus = 7,
    API_MQTT = 8
};

extern std::vector<String> RemoteAPILabels;

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

extern statistics_t Statistic_;
extern p_statistics_t p_Statistic_;
extern basic_config_t Basic_config_;
extern p_basic_config_t p_Basic_config_; 

enum TempUnits {
    TempCelsius = 0,
    TempFahrenheit = 1,
    TempKelvin = 2
};

extern const std::vector<String> TempLabelsShort;
extern const std::vector<String> TempLabels;

struct FlashConfig {
  char my_token[TKIDSIZE * 2];
  char my_name[TKIDSIZE] = "iGauge000";
  char my_server[TKIDSIZE];
  char my_url[TKIDSIZE * 2];
  char my_db[TKIDSIZE] = "iGauge";
  char my_username[TKIDSIZE];
  char my_password[TKIDSIZE];
  char my_job[TKIDSIZE] = "iGauge";
  char my_instance[TKIDSIZE] = "000";

  String my_ssid;
  String my_psk;
  uint8_t my_api;
  uint32_t my_sleeptime = 15 * 60;
  uint16_t my_port = 80;
  TempUnits my_tempscale = TempCelsius;
  int8_t my_OWpin = -1;
};

extern FlashConfig g_flashConfig;

extern controller_t Controller_;
extern p_controller_t p_Controller_;
extern statistics_t Statistic_;
extern p_statistics_t p_Statistic_;
extern basic_config_t Basic_config_;
extern p_basic_config_t p_Basic_config_; 

#endif
