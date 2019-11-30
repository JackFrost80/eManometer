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
#define FIRMWAREVERSION "0.8.1"

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

extern float Temperatur, Tilt, Gravity,Pressure,carbondioxide;

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

extern MR44V064B_Base FRAM;

extern bool zeroPointCal();

#endif
