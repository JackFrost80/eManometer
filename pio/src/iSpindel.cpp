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
#include "RunningMedian.h"
#include "webserver.h"
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <FS.h>          //this needs to be first
#include "MCP3221A5T-E.h"
#include <regex.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MR44V064B.h"
#include "OLED.h"

#include "Sender.h"
#include "display_adafruit_ssd1306.h"
#include "display_ssd1306_custom.h"
#include "timer.h"
#include "Sender.h"

#include <list>
// !DEBUG 1

timer_mgr g_timer_mgr;

// iGauge new stuff
unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 250;           // interval at which to blink (milliseconds)

DisplayInterface* disp = nullptr;

uint32_t open = 0;
uint32_t close = 0;
uint32_t open_time = 0.0;
uint32_t close_time = 0;
uint32_t next_calc = 0;
uint32_t next_vale_calc = 0;
float opening_time = 0.0;
uint16_t times_open = 0;


#define SIZE_OF_AVG  12
#define MosFET D0
#define LED_RED D4
#define LED_GREEN D5
#define LED_BLUE D6
#define Hz_1 0x04
#define Hz_0_25 0x01
#define Hz_0_5  0x02

typedef double tFloatAvgType;
typedef double tTempSumType;
uint32_t blink = 0;
double setpoint_carbondioxide = 5.0;
typedef struct FloatAvgFilter
{
	tFloatAvgType aData[SIZE_OF_AVG];
	uint8_t IndexNextValue;
} tFloatAvgFilter, *p_tFloatAvgFilter;

tFloatAvgFilter Temperature_filtered;
tFloatAvgFilter pressure_filtered;

void ICACHE_RAM_ATTR onTimerISR(){
    blink++;  //Toggle LED Pin
    timer1_write(600000);//12us
}

// definitions go here
MCP3221_Base ADC_;
OneWire *oneWire;
DallasTemperature DS18B20;
MR44V064B_Base FRAM; 
DeviceAddress tempDeviceAddress;
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

Webserver* webserver;

int detectTempSensor();

FlashConfig g_flashConfig;

uint32_t DSreqTime = 0;

float  Temperatur, Pressure, carbondioxide; 

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
      Serial.print("I2C device found at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println("  !");
 
      nDevices++;
    }
    else if (error==4)
    {
      Serial.print("Unknown error at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.println(address,HEX);
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

void AddToFloatAvg(tFloatAvgFilter * io_pFloatAvgFilter,
tFloatAvgType i_NewValue)
{
	// Neuen Wert an die dafuer vorgesehene Position im Buffer schreiben.
	io_pFloatAvgFilter->aData[io_pFloatAvgFilter->IndexNextValue] =
	i_NewValue;
	// Der naechste Wert wird dann an die Position dahinter geschrieben.
	io_pFloatAvgFilter->IndexNextValue++;
	// Wenn man hinten angekommen ist, vorne wieder anfangen.
	io_pFloatAvgFilter->IndexNextValue %= SIZE_OF_AVG;
}

tFloatAvgType GetOutputValue(tFloatAvgFilter * io_pFloatAvgFilter)
{
	tTempSumType TempSum = 0;
	// Durchschnitt berechnen
	for (uint8_t i = 0; i < SIZE_OF_AVG; ++i)
	{
		TempSum += io_pFloatAvgFilter->aData[i];
	}
	// Der cast is OK, wenn tFloatAvgType und tTempSumType korrekt gewaehlt wurden.
	tFloatAvgType o_Result = (tFloatAvgType) (TempSum / SIZE_OF_AVG);
	return o_Result;
}

void InitFloatAvg(tFloatAvgFilter * io_pFloatAvgFilter,
tFloatAvgType i_DefaultValue)
{
	// Den Buffer mit dem Initialisierungswert fuellen:
	for (uint8_t i = 0; i < SIZE_OF_AVG; ++i)
	{
		io_pFloatAvgFilter->aData[i] = i_DefaultValue;
	}
	// Der naechste Wert soll an den Anfang des Buffers geschrieben werden:
	io_pFloatAvgFilter->IndexNextValue = 0;
}


uint32_t crc32_for_byte(uint32_t r) {
  for(int j = 0; j < 8; ++j)
    r = (r & 1? 0: (uint32_t)0xEDB88320L) ^ r >> 1;
  return r ^ (uint32_t)0xFF000000L;
}

uint32_t crc32(uint32_t crc, uint8_t byte)
/*******************************************************************/
{
int8_t i;
  crc = crc ^ byte;
  for(i=7; i>=0; i--)
    crc=(crc>>1)^(0xedb88320ul & (-(crc&1)));
  return(crc);
}

void crc32_array(uint8_t *data, uint16_t n_bytes, uint32_t* crc) {
  *crc = 0xffffffff;
  for(uint16_t i=0;i<n_bytes;i++)
  {
      *crc = crc32(*crc,*data);
      data++;
  }
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



void blink_red(uint32_t Frequency)
{
  set_black();
  uint32 helper = blink & 0x01UL;
  if(helper == 0)
    digitalWrite(LED_RED, 0);
  else
    digitalWrite(LED_RED, 1);
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
            g_flashConfig.sleeptime = doc["Sleep"];
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
          

          CONSOLELN(F("parsed config:"));
#ifdef DEBUG
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

  // we make sure that configuration is properly set and we are not woken by
  // RESET button
  // ensure this was called

  rst_info *_reset_info = ESP.getResetInfoPtr();
  uint8_t _reset_reason = _reset_info->reason;

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
  doc["Sleep"] = g_flashConfig.sleeptime;
  // first reboot is for test
  g_flashConfig.sleeptime = 1;
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
      if (interval != g_flashConfig.sleeptime &&
          interval < 24 * 60 * 60 &&
          interval > 10)
      {
        g_flashConfig.sleeptime = interval;
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
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", g_flashConfig.sleeptime);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
    CONSOLELN(F("\ncalling Ubidots"));
    return sender.sendUbidots(g_flashConfig.token, g_flashConfig.name);
}

bool forwardMQTT()
{
    SenderClass sender;
    sender.add("temperature", scaleTemperature(Temperatur));
    sender.add("temp_units", tempScaleLabel());
    sender.add("pressure", Pressure);
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", g_flashConfig.sleeptime);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
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
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", g_flashConfig.sleeptime);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
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
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", g_flashConfig.sleeptime);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
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
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", g_flashConfig.sleeptime);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);

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

void initDS18B20()
{
  int owPin = detectTempSensor();
  if (owPin == -1)
  {
    CONSOLELN(F("ERROR: cannot find a OneWire Temperature Sensor!"));
    return;
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
      return;
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
}

bool isDS18B20ready()
{
  return millis() - DSreqTime > OWinterval;
}

float getTemperature(bool block = false)
{
  float t = Temperatur;

  // we need to wait for DS18b20 to finish conversion
  if (!DSreqTime ||
      (!block && !isDS18B20ready()))
    return t;

  // if we need the result we have to block
  while (!isDS18B20ready())
    delay(10);
  DSreqTime = 0;

  t = DS18B20.getTempCByIndex(0);

  if (t == DEVICE_DISCONNECTED_C || // DISCONNECTED
      t == 85.0)                    // we read 85 uninitialized
  {
    CONSOLELN(F("ERROR: OW DISCONNECTED"));
    pinMode(ONE_WIRE_BUS, OUTPUT);
    digitalWrite(ONE_WIRE_BUS, LOW);
    delay(100);
    oneWire->reset();

    CONSOLELN(F("OW Retry"));
    initDS18B20();
    delay(OWinterval);
    t = getTemperature(false);
  }

  return t;
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
  delay(250);
  digitalWrite(LED_RED ,0);
  digitalWrite(LED_GREEN,1);
  delay(250);
  digitalWrite(LED_GREEN ,0);
  digitalWrite(LED_BLUE,1);
  delay(250);
  digitalWrite(LED_BLUE,0);
}

void setup_FRAM_init()
{
  FRAM.read_controller_parameters(p_Controller_,Controller_paramter_offset);
  uint32_t calculated_crc = 0xffffffff;
  crc32_array((uint8_t *) p_Controller_,sizeof(controller_t)-4,&calculated_crc);
  uint32_t helper_crc = p_Controller_->crc32;
  CONSOLELN(helper_crc,HEX);
  CONSOLELN(calculated_crc,HEX);
  if(calculated_crc != p_Controller_->crc32)
  {
      set_red();
      set_controller_inital_values(p_Controller_);
      crc32_array((uint8_t *) p_Controller_,sizeof(controller_t)-4,&calculated_crc);
      p_Controller_->crc32 = calculated_crc;
      CONSOLELN(p_Controller_->crc32,HEX);
      CONSOLELN(calculated_crc,HEX);
      FRAM.write_controller_parameters(p_Controller_,Controller_paramter_offset);

  }
  else
  {
    crc32_array((uint8_t *) p_Controller_,sizeof(controller_t),&calculated_crc);
    CONSOLELN(p_Controller_->crc32,HEX);
    CONSOLELN(calculated_crc,HEX);
    set_green();
  }
  FRAM.read_statistics(p_Statistic_,Statistics_offset);
  crc32_array((uint8_t *) p_Statistic_,sizeof(statistics_t)-4,&calculated_crc);
  if(p_Statistic_->crc32 != calculated_crc)
  {
    p_Statistic_->opening_time = 0.0;
    p_Statistic_->times_open = 0;
    crc32_array((uint8_t *) p_Statistic_,sizeof(statistics_t)-4,&calculated_crc);
    p_Statistic_->crc32 = calculated_crc;
  }
  FRAM.read_basic_config(p_Basic_config_,basic_config_offset);
  crc32_array((uint8_t *) p_Basic_config_,sizeof(basic_config_t)-4,&calculated_crc);
  if(p_Basic_config_->crc32 != calculated_crc)
  {
    p_Basic_config_->faktor_pressure = 0.0030517578125;
    p_Basic_config_->type_of_display = 0;
    p_Basic_config_->use_regulator = 0;
    p_Basic_config_->value_blue = 0.6;
    p_Basic_config_->value_green = 1.05;
    p_Basic_config_->value_red = 1246; // Value equal to 4 bar.
    p_Basic_config_->value_turkis = 0.95;
    p_Basic_config_->zero_value_sensor = 0x163;
    crc32_array((uint8_t *) p_Basic_config_,sizeof(basic_config_t)-4,&calculated_crc);
    p_Basic_config_->crc32 = calculated_crc;
  }
}

void setup_displayInit()
{
  //disp = new Display_SSD1306_Custom(p_Basic_config_->type_of_display);
  disp = new Display_Adafruit_SSD1306();

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

  setup_ledTest();

  ADC_.MCP3221_init(); // Init I2C and Reset broken transmission.

  setup_FRAM_init();
      
  delay(500);

  setup_displayInit();

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
  initDS18B20();
  
  // decide whether we want configuration mode or normal mode
  if (shouldStartConfig(validConf)) {
    webserver->startWifiManager();
  }
  else {
    WiFi.mode(WIFI_STA);
  }

  // to make sure we wake up with STA but AP
  //WiFi.mode(WIFI_STA);
    // we try to survive
  WiFi.setOutputPower(20.5);

  uint16_t raw_data = ADC_.MCP3221_getdata();
  CONSOLE("raw data: ");
  CONSOLELN(raw_data);
  raw_data -= 0x163; // set 0,5V to zero;
  Pressure = (float((double)raw_data * 0.0030517578125));

  // call as late as possible since DS needs converge time
  Temperatur = getTemperature(true);
  CONSOLE("Temp: ");
  CONSOLELN(Temperatur);
  InitFloatAvg(&Temperature_filtered,Temperatur);
  InitFloatAvg(&pressure_filtered,Pressure);
  // calc gravity on user defined polynominal
  //Gravity = calculateGravity();
  //CONSOLE(F("Gravity: "));
  //CONSOLELN(Gravity);

  // water anomaly correction
  // float _temp = Temperatur - 4; // polynominal at 4
  // float wfact = 0.00005759 * _temp * _temp * _temp - 0.00783198 * _temp * _temp - 0.00011688 * _temp + 999.97;
  // corrGravity = Gravity - (1 - wfact / 1000);
  // CONSOLE(F("\tcorrGravity: "));
  // CONSOLELN(corrGravity);

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
    uploadData();
  }
  else
  {
    connectBackupCredentials();
    CONSOLELN(F("failed to connect"));
  }

  webserver->setConfSSID(htmlencode(g_flashConfig.ssid));
  webserver->setConfPSK(htmlencode(g_flashConfig.psk));

  webserver->startWebserver();

  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    g_timer_mgr.create_timer(250, false, [](){
      static int blink = 0;
      digitalWrite(LED_BUILTIN, blink);
      blink = !blink;
    });
  }
}

void loop()
{
  static timeout timer_apicall(g_flashConfig.sleeptime * 1000);
  static timeout timer_display;

  drd.loop();
  webserver->process();
  g_timer_mgr.check_timers();

  bool configMode = WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;

  if (timer_expired(timer_apicall)) {
    //requestTemp();
    uploadData();
    
    set_timer(timer_apicall, g_flashConfig.sleeptime * 1000);
  }

  if(isDS18B20ready())
  {
    float Temperatur_raw = getTemperature(true);
    AddToFloatAvg(&Temperature_filtered,Temperatur_raw);
    Temperatur = GetOutputValue(&Temperature_filtered);
    DS18B20.requestTemperatures();
    DSreqTime = millis();
   }
  uint16_t raw_data = ADC_.MCP3221_getdata();

  //uint16_t raw_value = raw_data;
    
    if(raw_data >= 0x100)
    {
      if(raw_data >= p_Basic_config_->zero_value_sensor)
          raw_data -=p_Basic_config_->zero_value_sensor; // Set Zero to 0,5V
        else
          raw_data = 0;
      if(raw_data >= p_Basic_config_->value_red)
        {
          set_red();
        }
        else
        {
          if(carbondioxide < p_Controller_->setpoint_carbondioxide * p_Basic_config_->value_blue)
            {
              set_blue();
            }
            else
            {
              if(carbondioxide < p_Controller_->setpoint_carbondioxide * p_Basic_config_->value_turkis)
              {
                 set_turkis();
              }
              else
              {
                if(carbondioxide < p_Controller_->setpoint_carbondioxide * p_Basic_config_->value_green)
                {
                  set_green();
                }
                else
                {
                  set_violet();
                }
              }
            }
          
        }
    }
    else
    {
      blink_red(Hz_0_25);
      
    }
    
    AddToFloatAvg(&pressure_filtered,((double)raw_data * p_Basic_config_->faktor_pressure));
    Pressure = GetOutputValue(&pressure_filtered);
    double exponent = -10.73797 + (2617.25 / ( Temperatur + 273.15 ));
    carbondioxide = (float)(((double)Pressure + 1.013) * 10 * exp(exponent));

    if (timer_expired(timer_display)) {
      blink++;
      if (disp) {
        disp->clear();
        disp->setLine(0);
        disp_print_header();

        if (configMode) {
          disp->setLine(1);
          disp->printf("WiFi Config Mode");
        }
        else {
          disp->setLine(1);
          disp->printf("Druck: %.2f bar  ", Pressure);
          disp->setLine(2);
          disp->printf("Temp: %.2f 'C", Temperatur);
          disp->setLine(3);
          disp->printf("CO2: %.2f g/l  ", carbondioxide);
          disp->setLine(4);
          disp->printf("Zeit: %.2f s", p_Statistic_->opening_time/1000);
          disp->setLine(5);
          disp->printf("Anzahl: %d", p_Statistic_->times_open);
          disp->setLine(6);
          if(p_Controller_->compressed_gas_bottle)
          {
            disp->setLine(7);
            disp->print("CO2 Flasche");
          }
          else
          {
            disp->setLine(7);
            disp->print("Gaerung       ");
          }
        }

      disp->sync();
    }

    set_timer(timer_display, 250);
  }

  unsigned long currentMillis = millis();

  if (currentMillis - next_calc >= p_Controller_->calc_time) {
    CONSOLELN("calc time");
    next_calc = currentMillis;
    calc_valve_time(p_Controller_,&open,&close,&open_time,&close_time,Pressure);
  }
  
 if (currentMillis - open >= open_time) {  // Open valve
    digitalWrite(MosFET ,0);
    
  }

  //if((currentMillis - open >= p_Controller_->open_time) && (currentMillis - close <= p_Controller_->close_time)) // Close Valve
  //{
    //digitalWrite(MosFET ,0);
  //}
  if(currentMillis - next_vale_calc >= p_Controller_->cycle_time)  // Calc new values
  {
    open_time = p_Controller_->open_time;
    open = currentMillis;
    
    //close = currentMillis + p_Controller_->close_time;
    next_vale_calc = currentMillis;
    
    
    if(p_Controller_->open_time > 0)
    {
      p_Statistic_->opening_time += open_time;
      p_Statistic_->times_open++;
      uint32_t calculated_crc = 0;
      crc32_array((uint8_t *) p_Statistic_,sizeof(statistics_t)-4,&calculated_crc);
      p_Statistic_->crc32 = calculated_crc;
      FRAM.write_statistics(p_Statistic_,Statistics_offset);
      digitalWrite(MosFET ,1);
    }
    
  }
  uint8_t input = digitalRead(D7);
  if(input == 0)
    {
      p_Statistic_->opening_time = 0.0;
      p_Statistic_->times_open = 0;
      uint32_t calculated_crc = 0;
      crc32_array((uint8_t *) p_Statistic_,sizeof(statistics_t)-4,&calculated_crc);
      p_Statistic_->crc32 = calculated_crc;
      FRAM.write_statistics(p_Statistic_,Statistics_offset);
    }
  
}
