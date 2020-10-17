/**************************************************************

"iSpindel"
All rights reserverd by S.Lang <universam@web.de>

**************************************************************/

// includes go here
#include <PubSubClient.h>
#include "Globals.h"
// #endif
#include "OneWire.h"
#include "Wire.h"
// #include <Ticker.h>
#include "DallasTemperature.h"
#include "DoubleResetDetector.h" // https://github.com/datacute/DoubleResetDetector
#include "webserver.h"
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

#include <FS.h>          //this needs to be first
#include "MCP3221A5T-E.h"
#include "MR44V064B.h"
#include "OLED.h"

#include "Sender.h"
#include <Adafruit_I2CDevice.h>
#include "display_adafruit_ssd1306.h"
#include "display_ssd1306_custom.h"
#include "display_HD44780.h"
#include "timer.h"
#include "Sender.h"
#include "median.h"
#include "mean.h"

#include "LiquidCrystal_PCF8574.h"

#include <list>
#include <map>
// !DEBUG 1

timer_mgr g_timer_mgr;

DisplayInterface* disp = nullptr;
DeviceAddress tempDeviceAddress;

uint32_t open = 0;
uint32_t close = 0;
uint32_t open_time = 0.0;
uint32_t close_time = 0;
uint32_t next_calc = 0;
uint32_t next_vale_calc = 0;
float opening_time = 0.0;
uint16_t times_open = 0;

static bool ds_failure = false;
static timeout timer_init_ds;

#define MosFET D0
#define LED_RED D4
#define LED_GREEN D5
#define LED_BLUE D6
#define Hz_1 0x04
#define Hz_0_25 0x01
#define Hz_0_5  0x02

uint32_t blink = 0;

uint8_t FRAM_present = 0;

// der kurze Median Filter (5 Elemente), ist da um etwaige Ausreißer rauszufiltern
// die bei einem normalen Mittelwert ziemlich durchschlagen
FastRunningMedian<uint16_t, 5>* median_pressure;
FastRunningMedian<int, 5>* median_temp;

// definitions go here
MCP3221_Base ADC_;
OneWire *oneWire = nullptr;
DallasTemperature DS18B20;
MR44V064B_Base FRAM; 

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

Webserver* webserver;

FlashConfig g_flashConfig;

uint32_t DSreqTime = 0;



mean<int> adc_mean;
mean<int> temp_mean;

float  Temperatur, Pressure, carbondioxide; 

int detectTempSensor();

void i2cscan()
{
  byte error, address;
  int nDevices;
 
  Serial.println("Scanning...");
 
  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0)
    {
      Serial.printf("I2C device found at address 0x%x\n", address);
 
      nDevices++;
    }
    else if (error==4)
    {
      Serial.printf("I2C scan: unknown error at address 0x%x\n", address);
    }    
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}

void i2c_reset_and_init()
{
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
    
}

void set_controller_inital_values(p_controller_t values)
{
  values->Kp = 0.1;
  values->dead_zone = 0.05;
  values->min_open_time = 20;
  values->cycle_time = 20000;
  values->calc_time = 250;
  values->Setpoint = 1.7;  // 1,7 bar Setpoint
  values->compressed_gas_bottle = false;
  values->maximum_openings = 0;
  p_Controller_->setpoint_carbondioxide =  5.0;
}

void set_black()
{
  digitalWrite(LED_RED,0);
  digitalWrite(LED_BLUE,0);
  digitalWrite(LED_GREEN,0);
}
void set_green()
{
  digitalWrite(LED_RED,0);
  digitalWrite(LED_BLUE,0);
  digitalWrite(LED_GREEN,1);
}
void set_red()
{
  digitalWrite(LED_RED,1);
  digitalWrite(LED_BLUE,0);
  digitalWrite(LED_GREEN,0);
}
void set_blue()
{
  digitalWrite(LED_RED,0);
  digitalWrite(LED_BLUE,1);
  digitalWrite(LED_GREEN,0);
}
void set_turkis()
{
  digitalWrite(LED_RED,0);
  digitalWrite(LED_BLUE,1);
  digitalWrite(LED_GREEN,1);
}
void set_yellow()
{
  digitalWrite(LED_RED,1);
  digitalWrite(LED_BLUE,0);
  digitalWrite(LED_GREEN,1);
}
void set_violet()
{
  digitalWrite(LED_RED,1);
  digitalWrite(LED_BLUE,1);
  digitalWrite(LED_GREEN,0);
}
void set_white()
{
  digitalWrite(LED_RED,1);
  digitalWrite(LED_BLUE,1);
  digitalWrite(LED_GREEN,1);
}

void blink_red()
{
  set_black();
  uint32 helper = blink & 0x01UL;
  if(helper == 0)
    digitalWrite(LED_RED, 0);
  else
    digitalWrite(LED_RED, 1);
}

void blink_yellow()
{
  uint32 helper = blink & 0x01UL;
  if(helper == 1) {
    digitalWrite(LED_RED,1);
    digitalWrite(LED_BLUE,0);
    digitalWrite(LED_GREEN,1);
  }
  else {
    digitalWrite(LED_RED, 0);
    digitalWrite(LED_BLUE,0);
    digitalWrite(LED_GREEN,0);
  }
}

 // old stuff

float scaleTemperature(float t)
{
  switch (g_flashConfig.tempscale) {
    case TempFahrenheit:
      return (1.8f * t + 32); 
    case TempKelvin:
      return t + 273.15f;
    case TempCelsius:
      // fall through
    default:
      return t;
  }
}

String tempScaleLabel(void)
{
  return TempLabelsShort[g_flashConfig.tempscale];
}

bool readConfig()
{
  CONSOLE(F("mounting FS..."));

  if (SPIFFS.begin())
  {
    CONSOLELN(F(" mounted!"));
    if (SPIFFS.exists(CFGFILE))
    {
      // file exists, reading and loading
      CONSOLELN(F("reading config file"));
      File configFile = SPIFFS.open(CFGFILE, "r");
      if (configFile)
      {
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, buf.get());

        if (!error)
        {
          if (doc.containsKey("Name"))
            g_flashConfig.name = doc["Name"].as<String>();
          if (doc.containsKey("Token"))
            g_flashConfig.token = doc["Token"].as<String>();
          if (doc.containsKey("Server"))
            g_flashConfig.server = doc["Server"].as<String>();
          if (doc.containsKey("Sleep"))
            g_flashConfig.interval = doc["Sleep"];
          if (doc.containsKey("API"))
            g_flashConfig.api = doc["API"];
          if (doc.containsKey("Port"))
            g_flashConfig.port = doc["Port"];
          if (doc.containsKey("URL"))
            g_flashConfig.url = doc["URL"].as<String>();
          if (doc.containsKey("DB"))
            g_flashConfig.db = doc["DB"].as<String>();
          if (doc.containsKey("Username"))
            g_flashConfig.username = doc["Username"].as<String>();
          if (doc.containsKey("Password"))
            g_flashConfig.password = doc["Password"].as<String>();
          if (doc.containsKey("Job"))
            g_flashConfig.job = doc["Job"].as<String>();
          if (doc.containsKey("Instance"))
            g_flashConfig.instance = doc["Instance"].as<String>();
          if (doc.containsKey("TS"))
            g_flashConfig.tempscale = (TempUnits) doc["TS"].as<uint8_t>();
          if (doc.containsKey("SSID"))
            g_flashConfig.ssid = doc["SSID"].as<String>();
          if (doc.containsKey("PSK"))
            g_flashConfig.psk = doc["PSK"].as<String>();
          if (doc.containsKey("Mode"))
            g_flashConfig.mode = (eManometerMode) doc["Mode"].as<uint8_t>();
          if (doc.containsKey("Display"))
            g_flashConfig.display = (DisplayType) doc["Display"].as<uint8_t>();
#ifdef DEBUG
          CONSOLELN(F("parsed config:"));
          serializeJson(doc, Serial);
          CONSOLELN();
#endif
          return true;
        }
        else
        {
          CONSOLELN(F("ERROR: failed to load json config"));
          return false;
        }
      }
      CONSOLELN(F("ERROR: unable to open config file"));
    }
  }
  else
  {
    CONSOLELN(F(" ERROR: failed to mount FS!"));
    return false;
  }
  return true;
}

bool shouldStartConfig(bool validConf)
{
  // The ESP reset info is sill buggy. see http://www.esp8266.com/viewtopic.php?f=32&t=8411
  // The reset reason is "5" (woken from deep-sleep) in most cases (also after a power-cycle)
  // I added a single reset detection as workaround to enter the config-mode easier
  CONSOLE(F("Boot-Mode: "));
  CONSOLELN(ESP.getResetReason());

  bool _dblreset = drd.detectDoubleReset();

  bool _wifiCred = (WiFi.SSID() != "");
  uint8_t c = 0;
  if (!_wifiCred)
    WiFi.begin();
  while (!_wifiCred)
  {
    if (c > 10)
      break;
    CONSOLE('.');
    delay(100);
    c++;
    _wifiCred = (WiFi.SSID() != "");
  }
  if (!_wifiCred)
    CONSOLELN(F("\nERROR no Wifi credentials"));

  int test = digitalRead(D7);

  bool config = false;

  if (!validConf) {
    CONSOLELN(F("going to config mode. Reaseon: no valid config"));
    config = true;
  }
  if (_dblreset) {
    CONSOLELN(F("going to config mode. Reaseon: double reset detected"));
    config = true;
  }
  if (!_wifiCred) {
    CONSOLELN(F("going to config mode. Reaseon: no WIFI credentials"));
    config = true;
  }
  if (test == 0) {
    CONSOLELN(F("going to config mode. Reaseon: button held down"));
    config = true;
  }

  if (config) 
    return true;
  else {
    CONSOLELN(F("normal mode"));
    return false;
  }
}

String htmlencode(String str)
{
  String encodedstr = "";
  char c;

  for (uint16_t i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);

    if (isalnum(c))
    {
      encodedstr += c;
    }
    else
    {
      encodedstr += "&#";
      encodedstr += String((uint8_t)c);
      encodedstr += ';';
    }
  }
  return encodedstr;
}

void formatSpiffs()
{
  CONSOLE(F("\nneed to format SPIFFS: "));
  SPIFFS.end();
  SPIFFS.begin();
  CONSOLELN(SPIFFS.format());
}

bool saveConfig()
{
  CONSOLE(F("saving config..."));

  // if SPIFFS is not usable
  if (!SPIFFS.begin() || !SPIFFS.exists(CFGFILE) ||
      !SPIFFS.open(CFGFILE, "w"))
    formatSpiffs();

  DynamicJsonDocument doc(1024);

  doc["Name"] = g_flashConfig.name;
  doc["Token"] = g_flashConfig.token;
  doc["Sleep"] = g_flashConfig.interval;
  doc["Server"] = g_flashConfig.server;
  doc["API"] = g_flashConfig.api;
  doc["Port"] = g_flashConfig.port;
  doc["URL"] = g_flashConfig.url;
  doc["DB"] = g_flashConfig.db;
  doc["Username"] = g_flashConfig.username;
  doc["Password"] = g_flashConfig.password;
  doc["Job"] = g_flashConfig.job;
  doc["Instance"] = g_flashConfig.instance;
  doc["TS"] = (uint8_t) g_flashConfig.tempscale;

  // Store current Wifi credentials
  doc["SSID"] = WiFi.SSID();
  doc["PSK"] = WiFi.psk();

  doc["Mode"] = (int) g_flashConfig.mode;
  doc["Display"] = (int) g_flashConfig.display;

  File configFile = SPIFFS.open(CFGFILE, "w+");
  if (!configFile)
  {
    CONSOLELN(F("failed to open config file for writing"));
    SPIFFS.end();
    return false;
  }
  else
  {
#ifdef DEBUG
    serializeJson(doc, Serial);
#endif
    serializeJson(doc, configFile);
    configFile.close();
    SPIFFS.end();
    CONSOLELN(F("saved successfully"));
    return true;
  }
}

bool processResponse(String response)
{
  DynamicJsonDocument doc(1024);

  DeserializationError error = deserializeJson(doc, response);
  if (error)

    if (!error && doc.containsKey("interval"))
    {
      uint32_t interval = doc["interval"];
      if (interval != g_flashConfig.interval &&
          interval < 24 * 60 * 60 &&
          interval > 10)
      {
        g_flashConfig.interval = interval;
        CONSOLE(F("Received new Interval config: "));
        CONSOLELN(interval);
        return saveConfig();
      }
    }
  return false;
}

bool forwardUbidots()
{
    SenderClass sender;
    sender.add("temperature", scaleTemperature(Temperatur));
    sender.add("pressure", Pressure);
    sender.add("co2", carbondioxide);
    sender.add("interval", g_flashConfig.interval);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("nun-openings",p_Statistic_->times_open);
    sender.add("open-time",p_Statistic_->opening_time/1000);
    CONSOLELN(F("\ncalling Ubidots"));
    return sender.sendUbidots(g_flashConfig.token, g_flashConfig.name);
}

bool forwardMQTT()
{
    SenderClass sender;
    sender.add("temperature", scaleTemperature(Temperatur));
    sender.add("temp_units", tempScaleLabel());
    sender.add("pressure", Pressure);
    sender.add("co2", carbondioxide);
    sender.add("interval", g_flashConfig.interval);
    sender.add("RSSI", WiFi.RSSI());

    if (g_flashConfig.mode == ModeSpundingValve) {
      sender.add("num-openings",p_Statistic_->times_open);
      sender.add("open-time",p_Statistic_->opening_time/1000);
    }
    sender.add("RSSI", WiFi.RSSI());
    CONSOLELN(F("\ncalling MQTT"));
    return sender.sendMQTT(g_flashConfig.server, g_flashConfig.port, g_flashConfig.username, g_flashConfig.password, g_flashConfig.name);
}

bool forwardInfluxDB()
{
    SenderClass sender;
    sender.add("temperature", scaleTemperature(Temperatur));
    sender.add("temp_units", tempScaleLabel());
    sender.add("pressure", Pressure);
    sender.add("co2", carbondioxide);
    sender.add("interval", g_flashConfig.interval);
    sender.add("RSSI", WiFi.RSSI());

    if (g_flashConfig.mode == ModeSpundingValve) {
      sender.add("num-openings",p_Statistic_->times_open);
      sender.add("open-time",p_Statistic_->opening_time/1000);
    }
    sender.add("RSSI", WiFi.RSSI());
    CONSOLELN(F("\ncalling InfluxDB"));
    CONSOLELN(String(F("Sending to db: ")) + g_flashConfig.db + String(F(" w/ credentials: ")) + g_flashConfig.username + String(F(":")) + g_flashConfig.password);
    return sender.sendInfluxDB(g_flashConfig.server, g_flashConfig.port, g_flashConfig.db, g_flashConfig.name, g_flashConfig.username, g_flashConfig.password);
}

bool forwardPrometheus()
{
    SenderClass sender;
    sender.add("temperature", Temperatur);
    sender.add("pressure", Pressure);
    sender.add("co2", carbondioxide);
    sender.add("interval", g_flashConfig.interval);
    sender.add("RSSI", WiFi.RSSI());

    if (g_flashConfig.mode == ModeSpundingValve) {
      sender.add("num-openings",p_Statistic_->times_open);
      sender.add("open-time",p_Statistic_->opening_time/1000);
    }
    sender.add("RSSI", WiFi.RSSI());
    CONSOLELN(F("\ncalling Prometheus Pushgateway"));
    return sender.sendPrometheus(g_flashConfig.server, g_flashConfig.port, g_flashConfig.job, g_flashConfig.instance);
}

bool forwardGeneric()
{
    SenderClass sender;
    sender.add("name", g_flashConfig.name);
    sender.add("ID", ESP.getChipId());
    if (g_flashConfig.token[0] != 0)
      sender.add("token", g_flashConfig.token);
    sender.add("temperature", scaleTemperature(Temperatur));
    sender.add("temp_units", tempScaleLabel());
    sender.add("type","eManometer");
    sender.add("pressure", Pressure);
    sender.add("co2", carbondioxide);
    sender.add("interval", g_flashConfig.interval);
    sender.add("RSSI", WiFi.RSSI());

    if (g_flashConfig.mode == ModeSpundingValve) {
      sender.add("num-openings",p_Statistic_->times_open);
      sender.add("open-time",p_Statistic_->opening_time/1000);
    }

    switch(g_flashConfig.api) {
      case API_HTTP:
        return sender.sendGenericPost(g_flashConfig.server, g_flashConfig.url, g_flashConfig.port);
      case API_CraftBeerPi:
        return sender.sendGenericPost(g_flashConfig.server, CBP_ENDPOINT, 5000);
      case API_TCP: {
        String response = sender.sendTCP(g_flashConfig.server, g_flashConfig.port);
        return processResponse(response);
      }
    }

  return false;
}

bool forwardThingSpeak()
{
    SenderClass sender;
    sender.add("temperature", Temperatur);
    sender.add("pressure", Pressure);
    sender.add("co2", carbondioxide);
    sender.add("interval", g_flashConfig.interval);
    sender.add("RSSI", WiFi.RSSI());

    if (g_flashConfig.mode == ModeSpundingValve) {
      sender.add("num-openings",p_Statistic_->times_open);
      sender.add("open-time",p_Statistic_->opening_time/1000);
    }

    CONSOLELN(F("\ncalling ThingSpeak"));
    return sender.sendThingSpeak(g_flashConfig.token);
}

bool uploadData()
{
  switch(g_flashConfig.api) {
    case API_Off:
      return true;
    case API_Ubidots:
      return forwardUbidots();
    case API_MQTT:
      return forwardMQTT();
    case API_InfluxDB:
      return forwardInfluxDB();
    case API_Prometheus:
      return forwardPrometheus();
    case API_THINGSPEAK:
      return forwardThingSpeak();
    case API_HTTP:
    case API_CraftBeerPi:
    case API_TCP:
      return forwardGeneric();
    default:
      return false;
  }
}

void requestTemp()
{
  if (!DSreqTime)
  {
    DS18B20.requestTemperatures();
    DSreqTime = millis();
  }
}

bool initDS18B20()
{
  int owPin = detectTempSensor();
  if (owPin == -1)
  {
    CONSOLELN(F("ERROR: cannot find a OneWire Temperature Sensor!"));
    return false;
  }

  pinMode(ONE_WIRE_BUS, OUTPUT);
  digitalWrite(ONE_WIRE_BUS, LOW);
  delay(100);
  oneWire = new OneWire(ONE_WIRE_BUS);
  DS18B20 = DallasTemperature(oneWire);
  DS18B20.begin();

  bool device = DS18B20.getAddress(tempDeviceAddress, 0);
  if (!device)
  {
    owPin = detectTempSensor();
    if (owPin == -1)
    {
      CONSOLELN(F("ERROR: cannot find a OneWire Temperature Sensor!"));
      return false;
    }
    else
    {
      delete oneWire;
      oneWire = new OneWire(ONE_WIRE_BUS);
      DS18B20 = DallasTemperature(oneWire);
      DS18B20.begin();
      DS18B20.getAddress(tempDeviceAddress, 0);
    }
  }

  DS18B20.setWaitForConversion(false);
  DS18B20.setResolution(tempDeviceAddress, RESOLUTION);
  requestTemp();

  return true;
}

bool isDS18B20ready()
{
  return millis() - DSreqTime > OWinterval;
}

bool getTemperature(int& temp, bool block = false)
{
  // we need to wait for DS18b20 to finish conversion
  if (!DSreqTime ||
      (!block && !isDS18B20ready()))
    return false;

  // if we need the result we have to block
  while (!isDS18B20ready())
    delay(10);
  DSreqTime = 0;

  temp = DS18B20.getTemp(tempDeviceAddress);

  return true;
}

int detectTempSensor()
{
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius;

  CONSOLE(F("scanning for OW device on pin: "));
  CONSOLELN(ONE_WIRE_BUS);
  OneWire ds(ONE_WIRE_BUS);

  if (!ds.search(addr))
  {
    CONSOLELN(F("No devices found!"));
    ds.reset_search();
    delay(250);
    return -1;
  }

  CONSOLE("Found device with ROM =");
  for (i = 0; i < 8; i++)
  {
    CONSOLE(' ');
    CONSOLE(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7])
  {
    CONSOLELN(" CRC is not valid!");
    return -1;
  }
  CONSOLELN();

  // the first ROM byte indicates which chip
  switch (addr[0])
  {
  case 0x10:
    CONSOLELN("  Chip = DS18S20"); // or old DS1820
    type_s = 1;
    break;
  case 0x28:
    CONSOLELN("  Chip = DS18B20");
    type_s = 0;
    break;
  case 0x22:
    CONSOLELN("  Chip = DS1822");
    type_s = 0;
    break;
  default:
    CONSOLELN("Device is not a DS18x20 family device.");
    return -1;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1); // start conversion, with parasite power on at the end

  delay(900); // maybe 750ms is enough, maybe not
  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE); // Read Scratchpad

  CONSOLE("  Data = ");
  CONSOLE(present, HEX);
  CONSOLE(" ");
  for (i = 0; i < 9; i++)
  { // we need 9 bytes
    data[i] = ds.read();
    CONSOLE(data[i], HEX);
    CONSOLE(" ");
  }
  CONSOLE(" CRC=");
  CONSOLELN(OneWire::crc8(data, 8), HEX);

  // Convert the data to actual temperature
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s)
  {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10)
    {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  }
  else
  {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00)
      raw = raw & ~7; // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20)
      raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40)
      raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  CONSOLE(F("  Temperature = "));
  CONSOLE(celsius);
  CONSOLELN(F(" Celsius, "));
  return ONE_WIRE_BUS;
}

void connectBackupCredentials()
{
  WiFi.disconnect();
  WiFi.begin(g_flashConfig.ssid.c_str(), g_flashConfig.psk.c_str());
  //wifiManager->startConfigPortal("iSpindel",NULL,true);
  CONSOLELN(F("Rescue Wifi credentials"));
  delay(100);
}

void calc_valve_time(p_controller_t values, uint32_t *open, uint32_t *close,uint32_t *open_time,uint32_t *close_time,float pressure)
{
  float error = 0;
  if(values->compressed_gas_bottle )
    error = values->Setpoint - pressure;
  else
    error =  pressure - values->Setpoint;
  if(error < values->dead_zone) 
    error = 0;
  float helper = error * values->Kp;
  //if(helper < values->min_open_time)
    //helper = values->min_open_time;

  if(helper >1)
    helper = 1;
  if(helper >0)
  {

    values->open_time = helper * values->cycle_time;
    if(values->open_time < values->min_open_time)
      values->open_time = values->min_open_time;
    values->close_time = values->cycle_time - values->open_time;
   }
  else
  {
    values->open_time = 0;
    values->close_time = values->cycle_time;
  }
  

}

void disp_print_header()
{
  disp->setLine(0);
  disp->print("eManometer ");
  disp->print(FIRMWAREVERSION);
}

void setup_ledTest()
{
  pinMode(MosFET, OUTPUT);
  Serial.begin(115200);
  // Set LED Pins to output
  pinMode(LED_RED , OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(MosFET, OUTPUT);
  //Set all off
  digitalWrite(LED_RED ,0);
  digitalWrite(LED_GREEN,0);
  digitalWrite(LED_BLUE,0);
  // Check colors :
  digitalWrite(LED_RED ,1);
  delay(125);
  digitalWrite(LED_RED ,0);
  digitalWrite(LED_GREEN,1);
  delay(125);
  digitalWrite(LED_GREEN ,0);
  digitalWrite(LED_BLUE,1);
  delay(125);
  digitalWrite(LED_BLUE,0);
}

int fram_err = 0;

void setup_FRAM_init()
{
  Wire.beginTransmission(0x50);
  uint8_t error = Wire.endTransmission();

  if (error != 0) {
    Serial.print("ERROR: FRAM I2C Address probe failed");
    fram_err = 1;
  }

  if (!FRAM.test_fram()) {
    Serial.print("ERROR: Failed to write and read back test string to FRAM. Please check your hardware");
    fram_err = 2;
  }

  if (!FRAM.read_controller_parameters(p_Controller_,Controller_paramter_offset)) {
      CONSOLELN("FRAM Controller Config CRC mismatch..resetting to defaults");
      set_red();
      set_controller_inital_values(p_Controller_);

  }
  else {
    set_green();
  }

  if (!FRAM.read_statistics(p_Statistic_,Statistics_offset)) {
    CONSOLELN("FRAM Statistics Config CRC mismatch..resetting to defaults");
    p_Statistic_->opening_time = 0.0;
    p_Statistic_->times_open = 0;
  }

  if (!FRAM.read_basic_config(p_Basic_config_,basic_config_offset)) {
    CONSOLELN("FRAM Basic Config CRC mismatch resetting to defaults");
    p_Basic_config_->faktor_pressure = 0.0030517578125;
    p_Basic_config_->use_regulator = 0;
    p_Basic_config_->value_blue = 60;
    p_Basic_config_->value_green = 105;
    p_Basic_config_->value_red = 4; // 4 bar
    p_Basic_config_->value_turkis = 95;
    p_Basic_config_->zero_value_sensor = 0x163;
  }
}

void setup_displayInit()
{
  switch (g_flashConfig.display) {
    case DiplaySSD1306:
      disp = new Display_Adafruit_SSD1306();
      break;
    case DisplaySH1106:
      disp = new Display_SSD1306_Custom(2);
      break;
    case DisplayHD44780:
      disp = new Display_HD44780();
      break;
    case DiplaySSD1306e:
      disp = new Display_SSD1306_Custom(1);
      break;
    default:
      break;
  }

  if (disp) {
    disp->init();
    disp->clear();
    disp_print_header();
    disp->sync();
  }
}

void setup()
{
  Serial.begin(115200);
  i2c_reset_and_init(); // Init I2C and Reset broken transmission.
  i2cscan();

  delay(100);

  setup_ledTest();

  //ADC_.MCP3221_init(); // Init I2C and Reset broken transmission.

  Wire.beginTransmission(0x50);
  FRAM_present = Wire.endTransmission();

  if(FRAM_present == 0)
    setup_FRAM_init();
  else
  {
    p_Basic_config_->faktor_pressure = 0.0030517578125;
    p_Basic_config_->use_regulator = 0;
    p_Basic_config_->value_blue = 60;
    p_Basic_config_->value_green = 105;
    p_Basic_config_->value_red = 4; // 4 bar
    p_Basic_config_->value_turkis = 95;
    p_Basic_config_->zero_value_sensor = 0x163;
    set_controller_inital_values(p_Controller_);
  }
  

  //Initialize Ticker every 0.5s
   // timer1_attachInterrupt(onTimerISR);
   // timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
   // timer1_write(600000); //120000 us
  CONSOLELN(F("\nFW " FIRMWAREVERSION));
  CONSOLELN(ESP.getSdkVersion());

  webserver = new Webserver();
  
  bool validConf = readConfig();
  if (!validConf)
    CONSOLELN(F("\nERROR config corrupted"));
  
  setup_displayInit();

  // decide whether we want configuration mode or normal mode
  if (shouldStartConfig(validConf)) {
    webserver->startWifiManager();
  }
  else {
    WiFi.mode(WIFI_STA);
  }

  WiFi.setOutputPower(20.5);


  uint16_t raw_data = 0x200;
   
  if(FRAM_present == 0)
    raw_data = ADC_.MCP3221_getdata();
  //CONSOLE("raw data: ");
  //CONSOLELN(raw_data);
  //raw_data -= 0x163; // set 0,5V to zero;
  //Pressure = (float((double)raw_data * 0.0030517578125));

  // call as late as possible since DS needs converge time
  int temp = 20;

  if (initDS18B20())
    getTemperature(temp, true);
  else {
    ds_failure = true;
    set_timer(timer_init_ds, 1000);
  }
  
  median_temp = new FastRunningMedian<int, 5>(temp);
  median_pressure = new FastRunningMedian<uint16_t, 5>(raw_data);
  adc_mean.init(10, raw_data);
  temp_mean.init(10, temp);

  if (WiFi.status() != WL_CONNECTED)
  {
    unsigned long startedAt = millis();
    CONSOLE(F("After waiting "));
    // int connRes = WiFi.waitForConnectResult();
    uint8_t wait = 0;
    while (WiFi.status() == WL_DISCONNECTED)
    {
      delay(100);
      wait++;
      if (wait > 50)
        break;
    }
    auto waited = (millis() - startedAt);
    CONSOLE(waited);
    CONSOLE(F("ms, result "));
    CONSOLELN(WiFi.status());
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    CONSOLE(F("IP: "));
    CONSOLELN(WiFi.localIP());
  }
  else
  {
    connectBackupCredentials();
    CONSOLELN(F("failed to connect"));
  }

  webserver->setConfSSID(htmlencode(g_flashConfig.ssid));
  webserver->setConfPSK(htmlencode(g_flashConfig.psk));

  webserver->startWebserver();
}

void controller()
{
  unsigned long currentMillis = millis();
  static float pressure_start = 0;
  static bool opened = false;
  if (currentMillis - next_calc >= p_Controller_->calc_time) {
    next_calc = currentMillis;
    calc_valve_time(p_Controller_,&open,&close,&open_time,&close_time,Pressure);
    pressure_start = Pressure;
  }
  
 if (currentMillis - open >= open_time) {  // Open valve
    if(p_Controller_->maximum_openings > 0 && p_Controller_->compressed_gas_bottle == true)
    {
      if(times_open > p_Controller_->maximum_openings)
      {
         digitalWrite(MosFET ,1); // Force close due to often openings with compressd gass
      }
      else
      {
         digitalWrite(MosFET ,0);
         opened = true;
      }
    }
    else
    {
      digitalWrite(MosFET ,0);
    }
  
  }
  
  

  //if((currentMillis - open >= p_Controller_->open_time) && (currentMillis - close <= p_Controller_->close_time)) // Close Valve
  //{
    //digitalWrite(MosFET ,0);
  //}
  if(currentMillis - next_vale_calc + 1 >= p_Controller_->cycle_time)  // Calc new values 1 second delay for correct meassurement of pressure
  {
    if(opened)
    {
      float delta_volume = (Pressure - pressure_start) * p_Controller_->theoretical_volume * 273.15 / (273.15 + Temperatur);
      p_Statistic_->volume_co2 += delta_volume;
      p_Statistic_->mass_co2 += delta_volume / 22.4 * 44 * 1000;
      p_Statistic_->theoretical_carbonisation = p_Statistic_->mass_co2 / p_Controller_->volume_barrel;
      opened = false;
    }
    else
    {
      pressure_start = Pressure;
    }
    
    open_time = p_Controller_->open_time;
    open = currentMillis;
    
    //close = currentMillis + p_Controller_->close_time;
    next_vale_calc = currentMillis;
    
    
    if(p_Controller_->open_time > 0)
    {
      p_Statistic_->opening_time += open_time;
      p_Statistic_->times_open++;
      FRAM.write_statistics(p_Statistic_,Statistics_offset);
      digitalWrite(MosFET ,1);
    }
    
  }
  uint8_t input = digitalRead(D7);
  if(input == 0)
    {
      p_Statistic_->opening_time = 0.0;
      p_Statistic_->times_open = 0;
      p_Statistic_->volume_co2 = 0;
      p_Statistic_->mass_co2 = carbondioxide * p_Controller_->volume_barrel;
      FRAM.write_statistics(p_Statistic_,Statistics_offset);
    }
}

void loop()
{
  static unsigned long prev = micros();
  unsigned long now = micros();
  unsigned long looptime = now - prev;

  static timeout timer_apicall(g_flashConfig.interval * 1000);
  static timeout timer_display;
  static int temp_raw = 0;

  drd.loop();
  webserver->process();
  g_timer_mgr.check_timers();

  bool configMode = WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;

  if (timer_expired(timer_apicall)) {
    //requestTemp();
    uploadData();
    
    set_timer(timer_apicall, g_flashConfig.interval * 1000);
  }

  if(isDS18B20ready() && !ds_failure)
  {
    getTemperature(temp_raw, true);
    
    if (temp_raw != DEVICE_DISCONNECTED_RAW) {
      ds_failure = false;
      median_temp->addValue(temp_raw);

      temp_mean.add(median_temp->getMedian());
      Temperatur = DS18B20.rawToCelsius(temp_mean.get());

      requestTemp();
    }
    else {
      ds_failure = true;
      Serial.println("DS18B20 disconnected? Trying to reinit in 1 sec");
      set_timer(timer_init_ds, 1000);
    }
  }

  if (timer_expired(timer_init_ds) && ds_failure) {
      Serial.println("DS18B20 disconnected? Trying to reinit");
      pinMode(ONE_WIRE_BUS, OUTPUT);
      digitalWrite(ONE_WIRE_BUS, LOW);
      delay(100);
      if (oneWire)
        oneWire->reset();

      if (initDS18B20()) {
        requestTemp();
        ds_failure = false;
      }
      else {
        set_timer(timer_init_ds, 1000);
      }
  }

  uint16_t adc_val = 0x200;
  if(FRAM_present == 0)
    adc_val = ADC_.MCP3221_getdata();
 // median_pressure->addValue(adc_val);
 // adc_mean.add(median_pressure->getMedian());
  adc_mean.add(adc_val);
  int pressure_adc = adc_mean.get();

  Pressure = (pressure_adc - p_Basic_config_->zero_value_sensor) * p_Basic_config_->faktor_pressure;

  bool valid_reading = true;
  // Sensor probably unplugged when the reading is a few increments below the calibrated zero point
  if (pressure_adc < p_Basic_config_->zero_value_sensor - 30) {
    Pressure = 0;
    valid_reading = false;
  }

  double exponent = -10.73797 + (2617.25 / ( Temperatur + 273.15 ));
  carbondioxide = (float)(((double)Pressure + 1.013) * 10 * exp(exponent));

  if (configMode) {
    blink_yellow();
  }
  else {
    if (valid_reading && !ds_failure && fram_err == 0) {
      if(Pressure >= p_Basic_config_->value_red)
        set_red();
      else if(carbondioxide < p_Controller_->setpoint_carbondioxide * p_Basic_config_->value_blue / 100.f)
        set_blue();
      else if(carbondioxide < p_Controller_->setpoint_carbondioxide * p_Basic_config_->value_turkis / 100.f)
        set_turkis();
      else if(carbondioxide < p_Controller_->setpoint_carbondioxide * p_Basic_config_->value_green / 100.f)
        set_green();
      else
        set_violet();
    }
    else
      blink_red();
  }

  //  AddToFloatAvg(&pressure_filtered,((double)pressure_adc * p_Basic_config_->faktor_pressure));
  //  Pressure = GetOutputValue(&pressure_filtered);

    if (timer_expired(timer_display)) {
      String errStr;

      if (!valid_reading) {
        errStr += "Pressure ";
      }
      if (ds_failure) {
        errStr += "DS18B20 ";
      }
      if (fram_err == 1) {
        errStr += "FRAM I2C";
      }
      if (fram_err == 2) {
        errStr += "FRAM";
      }


      blink++;
      if (disp) {
        //disp->clear();
        //disp->setLine(0);
        //disp_print_header();

        if (errStr.length() > 0) {
          disp->setLine(7);
          disp->print(("ERROR: " + errStr).c_str());
        }
        else
        {
          if (g_flashConfig.mode == ModeSpundingValve && p_Controller_->compressed_gas_bottle){
            disp->setLine(7);
            disp->printf("th.CO2: %.2f g/l  ", p_Statistic_->theoretical_carbonisation);
          }
        }
        

        if (configMode) {
          disp->setLine(0);
          disp->printf("WiFi Config Mode");
        }
        else {
          disp->setLine(0);
          disp->printf("Druck: %.2f bar  ", Pressure);
          disp->setLine(1);
          disp->printf("Temp: %.2f 'C", Temperatur);
          disp->setLine(2);
          disp->printf("CO2: %6.2f g/l  ", carbondioxide);

          if (g_flashConfig.mode == ModeSpundingValve) {
            disp->setLine(3);
            disp->printf("Zeit: %5.2f s", p_Statistic_->opening_time/1000);
            if(g_flashConfig.display != DisplayHD44780)
            {
              disp->setLine(4);
              disp->printf("Anzahl: %6d", p_Statistic_->times_open);
              if(p_Controller_->compressed_gas_bottle)
              {
                disp->setLine(5);
                disp->print("CO2 Flasche");
                if(p_Statistic_->times_open >= p_Controller_->maximum_openings && p_Controller_->maximum_openings > 0)
                {
                  disp->setLine(6);
                  disp->print("Ventil verriegelt");
                }
                else
                {
                  disp->setLine(6);
                  disp->print("                   ");
                }
                
              }
              else
              {
                disp->setLine(5);
                disp->print("Gaerung       ");
              }
              
            }
            

            // for performance debugging, display the time in microseconds the last loop
            // took
            //disp->setLine(2);
            //disp->printf("looptime: %d us ", looptime);
          }
        }

      disp->sync();
    }

    set_timer(timer_display, 250);
  }

  if (g_flashConfig.mode == ModeSpundingValve) {
    controller();
  }

  prev = now;
}

// do 100 readings and use the value that was read most often as zero point
bool zeroPointCal()
{
  std::map<int, int> vals;

  for (int i = 0; i <= 100; ++i) {
    uint16_t val = ADC_.MCP3221_getdata();

    if (val > 0x200) {
      return false;
    }

    vals[val]++;
  }

  int max = 0;
  int zp = 0;

  for (auto &val: vals) {
    Serial.printf("Val: %d, count: %d\n", val.first, val.second);

    if (val.second > max) {
      max = val.second;
      zp = val.first;
    }
    
  }

  p_Basic_config_->zero_value_sensor = zp;

  Serial.printf("Zero point calibrated to %d (%x)\n", zp, zp);

  return true;
}