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
#include "MR44V064B.h"

#include <Ticker.h>

// defines go here
#define FIRMWAREVERSION "0.5.0"

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
#define ONE_WIRE_BUS D3 // DS18B20 on ESP pin12

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

#define Error_ammunt 20
#define Hope_flame_out_data 2
#define number_of_rasts 7
#define number_of_hop_timers 7
#define number_of_hop_flame_out_timers 2
#define Data_amount 20

#define Software_port 13001


#define CBP_ENDPOINT "/api/emanometer/v1/data"

enum RemoteAPI {
    API_Off = 0,
    API_Ubidots = 1,
    API_CraftBeerPi = 2,
    API_HTTP = 3,
    API_TCP = 4,
    API_InfluxDB = 5,
    API_Prometheus = 6,
    API_MQTT = 7,
    API_THINGSPEAK = 8,
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

extern float Temperatur, Tilt, Gravity,Pressure,carbondioxide,Setpoint, gradient;
extern uint32_t output,time_left;

extern bool saveConfig();
extern void formatSpiffs();
extern void validateInput(const char *input, char *output);

float scaleTemperature(float t);
String tempScaleLabel(void);

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

enum eManometerMode {
    ModeBottleGauge,
    ModeSpundingValve,
};

enum DisplayType {
    DisplayOff,
    DiplaySSD1306,
    DisplaySH1106
};

typedef struct Recipe
{
	uint32_t ID;
	float temperatur_mash;
	float rast_temp[number_of_rasts];
	uint16_t rast_time[number_of_rasts];
	uint8_t dekoktion[number_of_rasts];
	uint16_t hop_time[number_of_hop_timers];
	uint16_t hop_time_flame_out[number_of_hop_flame_out_timers];
	float hop_temp_flame_out[number_of_hop_flame_out_timers];
	uint8_t cooler;
	uint8_t Irish_moos;
	uint8_t yeast;
	float cooling_temp;
	uint16_t boil_time;
	float lauter_temp;
	uint32_t crc;
} Recipe_t, *p_Recipe_t;

typedef struct Event {
	uint32_t Type;
	uint32_t Timestamp;
}Event_t, *p_Event_t;
typedef struct Temperatur_Event {
	uint32_t Type;
	float Temperature;
	uint32_t Timestamp;
	uint32_t Timestamp_cooling_restart;
}Temperatur_Event_t, *p_Temperatur_Event_t;


typedef struct brew_documentation {
	Event_t Error_data[Error_ammunt];
	Temperatur_Event hop_flameout[Hope_flame_out_data];
	Event_t Data_[Data_amount];
	Event_t Hop_data[number_of_hop_timers];
	uint32_t mash_time;
	uint32_t rast_time[number_of_rasts];
	float rast_temp[number_of_rasts];
	float rast_min[number_of_rasts];
	float rast_max[number_of_rasts];
	float boil_temp;
	uint32_t irish_moos;
	uint32_t cooling_start;
	uint32_t cooling_isomer;
	uint32_t cooling_end;
	float pH_value;
	uint32_t pH_timestamp;
	uint8_t Error_index;
	uint8_t hop_flameout_index;
	uint8_t hop_index;
	uint8_t Data_index;
	uint32_t iodine_time;
	uint32_t sparge_time;
	uint32_t crc;
}brew_documentation_t, *p_brew_documentation_t;



struct FlashConfig {
  eManometerMode mode;
  DisplayType display;
  String token;
  String name = "eManometer000";
  String server;
  String url;
  String db = "eManometer";
  String username;
  String password;
  String job = "eManometer";
  String instance = "000";
  String default_ip;
  String ssid;
  String psk;
  uint8_t api;
  uint32_t interval = 15 * 60;
  uint16_t port = 80;
  TempUnits tempscale = TempCelsius;
};

extern FlashConfig g_flashConfig;

extern controller_t Controller_;
extern p_controller_t p_Controller_;
extern statistics_t Statistic_;
extern p_statistics_t p_Statistic_;
extern basic_config_t Basic_config_;
extern p_basic_config_t p_Basic_config_; 

extern p_Recipe_t p_recipe;

extern MR44V064B_Base FRAM;

extern bool zeroPointCal();

#endif
