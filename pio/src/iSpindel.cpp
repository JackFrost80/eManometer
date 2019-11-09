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
#include <DNSServer.h>
#include <ESP8266WebServer.h>
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

#include <list>
// !DEBUG 1

timer_mgr g_timer_mgr;

// iGauge new stuff
unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 250;           // interval at which to blink (milliseconds)

DisplayInterface* disp = nullptr;

double setpoint_pressure = 0;
controller_t Controller_;
p_controller_t p_Controller_ = &Controller_;
statistics_t Statistic_;
p_statistics_t p_Statistic_ = &Statistic_;
basic_config_t Basic_config_;
p_basic_config_t p_Basic_config_= &Basic_config_; 
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

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output5State = "off";
String output4State = "off";
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

// definitions go here
MCP3221_Base ADC_;
OneWire *oneWire;
DallasTemperature DS18B20;
MR44V064B_Base FRAM; 
DeviceAddress tempDeviceAddress;
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);
uint32_t diff_time = 0;
uint32_t interval_send_data = 30000;
uint32_t start_time = 0;


Webserver* webserver;

int detectTempSensor(const uint8_t pins[]);

bool shouldSaveConfig = false;

FlashConfig g_flashConfig;

uint32_t DSreqTime = 0;

float  Temperatur, Pressure, carbondioxide; 

Webserver* wifiManager;

//iGauge new Stuff 

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
  switch (g_flashConfig.my_tempscale) {
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
  return TempLabelsShort[g_flashConfig.my_tempscale];
}

// callback notifying us of the need to save config
void saveConfigCallback()
{
  shouldSaveConfig = true;
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
            strcpy(g_flashConfig.my_name, doc["Name"]);
          if (doc.containsKey("Token"))
            strcpy(g_flashConfig.my_token, doc["Token"]);
          if (doc.containsKey("Server"))
            strcpy(g_flashConfig.my_server, doc["Server"]);
          if (doc.containsKey("Sleep"))
            g_flashConfig.my_sleeptime = doc["Sleep"];
          if (doc.containsKey("API"))
            g_flashConfig.my_api = doc["API"];
          if (doc.containsKey("Port"))
            g_flashConfig.my_port = doc["Port"];
          if (doc.containsKey("URL"))
            strcpy(g_flashConfig.my_url, doc["URL"]);
          if (doc.containsKey("DB"))
            strcpy(g_flashConfig.my_db, doc["DB"]);
          if (doc.containsKey("Username"))
            strcpy(g_flashConfig.my_username, doc["Username"]);
          if (doc.containsKey("Password"))
            strcpy(g_flashConfig.my_password, doc["Password"]);
          if (doc.containsKey("Job"))
            strcpy(g_flashConfig.my_job, doc["Job"]);
          if (doc.containsKey("Instance"))
            strcpy(g_flashConfig.my_instance, doc["Instance"]);
          if (doc.containsKey("TS"))
            g_flashConfig.my_tempscale = (TempUnits) doc["TS"].as<uint8_t>();
          if (doc.containsKey("OWpin"))
            g_flashConfig.my_OWpin = doc["OWpin"];
          if (doc.containsKey("SSID"))
            g_flashConfig.my_ssid = (const char *)doc["SSID"];
          if (doc.containsKey("PSK"))
            g_flashConfig.my_psk = (const char *)doc["PSK"];
          

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
  bool _poweredOnOffOn = _reset_reason == REASON_DEFAULT_RST || _reset_reason == REASON_EXT_SYS_RST;
  if (_poweredOnOffOn)
    CONSOLELN(F("power-cycle or reset detected, config mode"));

  bool _dblreset = drd.detectDoubleReset();
  if (_dblreset)
    CONSOLELN(F("\nDouble Reset detected"));

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

  if (validConf && !_dblreset && _wifiCred && test == 1 )
  {
    CONSOLELN(F("\nwoken from deepsleep, normal mode"));
    return false;
  }
  // config mode
  else
  {

    if(test == 0)
    {
        CONSOLELN(F("\ngoing to Config Mode"));
        return true;
    }
    else
    {
      CONSOLELN(F("\ngoing to normal Mode"));
      return false;
    }
    
  }
}

void validateInput(const char *input, char *output)
{
  String tmp = input;
  tmp.trim();
  tmp.replace(' ', '_');
  tmp.toCharArray(output, tmp.length() + 1);
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

#if 0
bool startConfiguration()
{
  set_violet();
  lcd_gotoxy(0,2,p_Basic_config_->type_of_display);
  lcd_puts_invert("Konfig... 192.168.4.1");

  Webserver wifiManager;

  wifiManager.setConfigPortalTimeout(PORTALTIMEOUT);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setBreakAfterConfig(true);

  WiFiManagerParameter api_list(HTTP_API_LIST);
  WiFiManagerParameter custom_api("selAPI", "selAPI", String(my_api).c_str(),
                                  20, TYPE_HIDDEN, WFM_NO_LABEL);

  WiFiManagerParameter custom_name("name", "iSpindel Name", htmlencode(my_name).c_str(),
                                   TKIDSIZE * 2);
  WiFiManagerParameter custom_sleep("sleep", "Update Interval (s)",
                                    String(my_sleeptime).c_str(), 6, TYPE_NUMBER);
  WiFiManagerParameter custom_token("token", "Token", htmlencode(my_token).c_str(),
                                    TKIDSIZE * 2 * 2);
  WiFiManagerParameter custom_server("server", "Server Address",
                                     my_server, TKIDSIZE);
  WiFiManagerParameter custom_port("port", "Server Port",
                                   String(my_port).c_str(), TKIDSIZE,
                                   TYPE_NUMBER);
  WiFiManagerParameter custom_url("url", "Server URL", my_url, TKIDSIZE * 2);
  WiFiManagerParameter custom_db("db", "InfluxDB db", my_db, TKIDSIZE);
  WiFiManagerParameter custom_username("username", "Username", my_username, TKIDSIZE);
  WiFiManagerParameter custom_password("password", "Password", my_password, TKIDSIZE);
  WiFiManagerParameter custom_job("job", "Prometheus job", my_job, TKIDSIZE);
  WiFiManagerParameter custom_instance("instance", "Prometheus instance", my_instance, TKIDSIZE);
  
  WiFiManagerParameter tempscale_list(HTTP_TEMPSCALE_LIST);
  WiFiManagerParameter custom_tempscale("tempscale", "tempscale",
                                        String(my_tempscale).c_str(),
                                        5, TYPE_HIDDEN, WFM_NO_LABEL);

  wifiManager.addParameter(&custom_name);
  wifiManager.addParameter(&custom_sleep);
 

  WiFiManagerParameter custom_tempscale_hint("<label for=\"TS\">Unit of temperature</label>");
  wifiManager.addParameter(&custom_tempscale_hint);
  wifiManager.addParameter(&tempscale_list);
  wifiManager.addParameter(&custom_tempscale);
  WiFiManagerParameter custom_api_hint("<hr><label for=\"API\">Service Type</label>");
  wifiManager.addParameter(&custom_api_hint);

  wifiManager.addParameter(&api_list);
  wifiManager.addParameter(&custom_api);

  wifiManager.addParameter(&custom_token);
  wifiManager.addParameter(&custom_server);
  wifiManager.addParameter(&custom_port);
  wifiManager.addParameter(&custom_url);
  wifiManager.addParameter(&custom_db);
  wifiManager.addParameter(&custom_username);
  wifiManager.addParameter(&custom_password);
  wifiManager.addParameter(&custom_job);
  wifiManager.addParameter(&custom_instance);
  

  CONSOLELN(F("started Portal"));
  wifiManager.startConfigPortal("iSpindel");

  validateInput(custom_name.getValue(), g_flashConfig.my_name);
  validateInput(custom_token.getValue(), g_flashConfig.my_token);
  validateInput(custom_server.getValue(), g_flashConfig.my_server);
  validateInput(custom_db.getValue(), g_flashConfig.my_db);
  validateInput(custom_username.getValue(), g_flashConfig.my_username);
  validateInput(custom_password.getValue(), g_flashConfig.my_password);
  validateInput(custom_job.getValue(), g_flashConfig.my_job);
  validateInput(custom_instance.getValue(), g_flashConfig.my_instance);
  g_flashConfig.my_sleeptime = String(custom_sleep.getValue()).toInt();

  g_flashConfig.my_api = String(custom_api.getValue()).toInt();
  g_flashConfig.my_port = String(custom_port.getValue()).toInt();
  g_flashConfig.my_tempscale = String(custom_tempscale.getValue()).toInt();
  validateInput(custom_url.getValue(), my_url);

  
 

  // save the custom parameters to FS
  if (shouldSaveConfig)
  {
    // Wifi config
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);

    return saveConfig();
  }
  return false;
}
#endif

#if 0
bool startConfigurationNormal()
{

  wifiManager = new Webserver();
  
  wifiManager->setConfigPortalTimeout(PORTALTIMEOUT);
  wifiManager->setSaveConfigCallback(saveConfigCallback);
  wifiManager->setBreakAfterConfig(true);

  WiFiManagerParameter api_list(HTTP_API_LIST);
  WiFiManagerParameter custom_api("selAPI", "selAPI", String(my_api).c_str(),
                                  20, TYPE_HIDDEN, WFM_NO_LABEL);

  WiFiManagerParameter custom_name("name", "iSpindel Name", htmlencode(my_name).c_str(),
                                   TKIDSIZE * 2);
  WiFiManagerParameter custom_sleep("sleep", "Update Interval (s)",
                                    String(my_sleeptime).c_str(), 6, TYPE_NUMBER);
  WiFiManagerParameter custom_token("token", "Token", htmlencode(my_token).c_str(),
                                    TKIDSIZE * 2 * 2);
  WiFiManagerParameter custom_server("server", "Server Address",
                                     my_server, TKIDSIZE);
  WiFiManagerParameter custom_port("port", "Server Port",
                                   String(my_port).c_str(), TKIDSIZE,
                                   TYPE_NUMBER);
  WiFiManagerParameter custom_url("url", "Server URL", my_url, TKIDSIZE * 2);
  WiFiManagerParameter custom_db("db", "InfluxDB db", my_db, TKIDSIZE);
  WiFiManagerParameter custom_username("username", "Username", my_username, TKIDSIZE);
  WiFiManagerParameter custom_password("password", "Password", my_password, TKIDSIZE);
  WiFiManagerParameter custom_job("job", "Prometheus job", my_job, TKIDSIZE);
  WiFiManagerParameter custom_instance("instance", "Prometheus instance", my_instance, TKIDSIZE);
  
  WiFiManagerParameter tempscale_list(HTTP_TEMPSCALE_LIST);
  WiFiManagerParameter custom_tempscale("tempscale", "tempscale",
                                        String(my_tempscale).c_str(),
                                        5, TYPE_HIDDEN, WFM_NO_LABEL);

  wifiManager->addParameter(&custom_name);
  wifiManager->addParameter(&custom_sleep);
  

  WiFiManagerParameter custom_tempscale_hint("<label for=\"TS\">Unit of temperature</label>");
  wifiManager->addParameter(&custom_tempscale_hint);
  wifiManager->addParameter(&tempscale_list);
  wifiManager->addParameter(&custom_tempscale);
  WiFiManagerParameter custom_api_hint("<hr><label for=\"API\">Service Type</label>");
  wifiManager->addParameter(&custom_api_hint);

  wifiManager->addParameter(&api_list);
  wifiManager->addParameter(&custom_api);

  wifiManager->addParameter(&custom_token);
  wifiManager->addParameter(&custom_server);
  wifiManager->addParameter(&custom_port);
  wifiManager->addParameter(&custom_url);
  wifiManager->addParameter(&custom_db);
  wifiManager->addParameter(&custom_username);
  wifiManager->addParameter(&custom_password);
  wifiManager->addParameter(&custom_job);
  wifiManager->addParameter(&custom_instance);
  
  wifiManager->setConfSSID(htmlencode(my_ssid));
  wifiManager->setConfPSK(htmlencode(my_psk));

  CONSOLELN(F("started Portal"));
  wifiManager->startWebserver();

  
#if 0
  validateInput(custom_name.getValue(), my_name);
  validateInput(custom_token.getValue(), my_token);
  validateInput(custom_server.getValue(), my_server);
  validateInput(custom_db.getValue(), my_db);
  validateInput(custom_username.getValue(), my_username);
  validateInput(custom_password.getValue(), my_password);
  validateInput(custom_job.getValue(), my_job);
  validateInput(custom_instance.getValue(), my_instance);
  my_sleeptime = String(custom_sleep.getValue()).toInt();

  my_api = String(custom_api.getValue()).toInt();
  my_port = String(custom_port.getValue()).toInt();
  my_tempscale = String(custom_tempscale.getValue()).toInt();
  validateInput(custom_url.getValue(), my_url);

    
  // save the custom parameters to FS
  if (shouldSaveConfig)
  {
    // Wifi config
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);

    return saveConfig();
  }
#endif

  return false;
}
#endif


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

  doc["Name"] = g_flashConfig.my_name;
  doc["Token"] = g_flashConfig.my_token;
  doc["Sleep"] = g_flashConfig.my_sleeptime;
  // first reboot is for test
  g_flashConfig.my_sleeptime = 1;
  doc["Server"] = g_flashConfig.my_server;
  doc["API"] = g_flashConfig.my_api;
  doc["Port"] = g_flashConfig.my_port;
  doc["URL"] = g_flashConfig.my_url;
  doc["DB"] = g_flashConfig.my_db;
  doc["Username"] = g_flashConfig.my_username;
  doc["Password"] = g_flashConfig.my_password;
  doc["Job"] = g_flashConfig.my_job;
  doc["Instance"] = g_flashConfig.my_instance;
  
  doc["TS"] = (uint8_t) g_flashConfig.my_tempscale;
  doc["OWpin"] = g_flashConfig.my_OWpin;

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
      if (interval != g_flashConfig.my_sleeptime &&
          interval < 24 * 60 * 60 &&
          interval > 10)
      {
        g_flashConfig.my_sleeptime = interval;
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
    sender.add("interval", g_flashConfig.my_sleeptime);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
    CONSOLELN(F("\ncalling Ubidots"));
    return sender.sendUbidots(g_flashConfig.my_token, g_flashConfig.my_name);
}

bool forwardMQTT()
{
    SenderClass sender;
    sender.add("temperature", scaleTemperature(Temperatur));
    sender.add("temp_units", tempScaleLabel());
    sender.add("pressure", Pressure);
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", interval_send_data);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
    sender.add("RSSI", WiFi.RSSI());
    CONSOLELN(F("\ncalling MQTT"));
    return sender.sendMQTT(g_flashConfig.my_server, g_flashConfig.my_port, g_flashConfig.my_username, g_flashConfig.my_password, g_flashConfig.my_name);
}

bool forwardInfluxDB()
{
    SenderClass sender;
    sender.add("temperature", scaleTemperature(Temperatur));
    sender.add("temp_units", tempScaleLabel());
    sender.add("pressure", Pressure);
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", interval_send_data);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
    sender.add("RSSI", WiFi.RSSI());
    CONSOLELN(F("\ncalling InfluxDB"));
    CONSOLELN(String(F("Sending to db: ")) + g_flashConfig.my_db + String(F(" w/ credentials: ")) + g_flashConfig.my_username + String(F(":")) + g_flashConfig.my_password);
    return sender.sendInfluxDB(g_flashConfig.my_server, g_flashConfig.my_port, g_flashConfig.my_db, g_flashConfig.my_name, g_flashConfig.my_username, g_flashConfig.my_password);
}

bool forwardPrometheus()
{
    SenderClass sender;
    sender.add("temperature", Temperatur);
    sender.add("pressure", Pressure);
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", interval_send_data);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
    sender.add("RSSI", WiFi.RSSI());
    CONSOLELN(F("\ncalling Prometheus Pushgateway"));
    return sender.sendPrometheus(g_flashConfig.my_server, g_flashConfig.my_port, g_flashConfig.my_job, g_flashConfig.my_instance);
}

bool forwardGeneric()
{
    SenderClass sender;
    sender.add("name", g_flashConfig.my_name);
    sender.add("ID", ESP.getChipId());
    if (g_flashConfig.my_token[0] != 0)
      sender.add("token", g_flashConfig.my_token);
    sender.add("temperature", scaleTemperature(Temperatur));
    sender.add("temp_units", tempScaleLabel());
    sender.add("type","eManometer");
    sender.add("pressure", Pressure);
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", interval_send_data);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
    
    switch(g_flashConfig.my_api) {
      case API_HTTP:
        return sender.sendGenericPost(g_flashConfig.my_server, g_flashConfig.my_url, g_flashConfig.my_port);
      case API_CraftBeerPi:
        return sender.sendGenericPost(g_flashConfig.my_server, CBP_ENDPOINT, 5000);
      case API_TCP: {
        String response = sender.sendTCP(g_flashConfig.my_server, g_flashConfig.my_port);
        return processResponse(response);
      }
    }
}

bool uploadData()
{
  switch(g_flashConfig.my_api) {
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
  if (g_flashConfig.my_OWpin == -1)
  {
    g_flashConfig.my_OWpin = detectTempSensor(OW_PINS);
    if (g_flashConfig.my_OWpin == -1)
    {
      CONSOLELN(F("ERROR: cannot find a OneWire Temperature Sensor!"));
      return;
    }
  }
  pinMode(g_flashConfig.my_OWpin, OUTPUT);
  digitalWrite(g_flashConfig.my_OWpin, LOW);
  delay(100);
  oneWire = new OneWire(g_flashConfig.my_OWpin);
  DS18B20 = DallasTemperature(oneWire);
  DS18B20.begin();

  bool device = DS18B20.getAddress(tempDeviceAddress, 0);
  if (!device)
  {
    g_flashConfig.my_OWpin = detectTempSensor(OW_PINS);
    if (g_flashConfig.my_OWpin == -1)
    {
      CONSOLELN(F("ERROR: cannot find a OneWire Temperature Sensor!"));
      return;
    }
    else
    {
      delete oneWire;
      oneWire = new OneWire(g_flashConfig.my_OWpin);
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
    pinMode(g_flashConfig.my_OWpin, OUTPUT);
    digitalWrite(g_flashConfig.my_OWpin, LOW);
    delay(100);
    oneWire->reset();

    CONSOLELN(F("OW Retry"));
    initDS18B20();
    delay(OWinterval);
    t = getTemperature(false);
  }

  return t;
}

int detectTempSensor(const uint8_t pins[])
{

  for (uint8_t p = 0; p < sizeof(pins); p++)
  {
    const byte pin = pins[p];
    byte i;
    byte present = 0;
    byte type_s;
    byte data[12];
    byte addr[8];
    float celsius;

    CONSOLE(F("scanning for OW device on pin: "));
    CONSOLELN(pin);
    OneWire ds(pin);

    if (!ds.search(addr))
    {
      CONSOLELN(F("No devices found!"));
      ds.reset_search();
      delay(250);
      continue;
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
      continue;
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
      continue;
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
    return pin;
  }
  return -1;
}

void connectBackupCredentials()
{
  WiFi.disconnect();
  WiFi.begin(g_flashConfig.my_ssid.c_str(), g_flashConfig.my_psk.c_str());
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

  disp->init();
  disp->clear();
  disp_print_header();
  disp->sync();
}

void setup()
{
  Serial.begin(115200);
  i2c_reset_and_init(); // Init I2C and Reset broken transmission.

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
  start_time = millis();

  webserver->setConfSSID(htmlencode(g_flashConfig.my_ssid));
  webserver->setConfPSK(htmlencode(g_flashConfig.my_psk));

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
  webserver->process();
  g_timer_mgr.check_timers();

  // Don't do anything else than web handling in Config Mode
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    return;
  }

  #if 0
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      currentTime = millis();         
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
                       
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>eManometer Web Server</h1>");
            
            // Display current state, and ON/OFF buttons for GPIO 5  
            char helper[20];
            //client.println(header);
            String helper_get;
            String Regler_string;
            String Kp_string;
            String min_open_time_string;
            String dead_zone_string;
            String cycle_time_string;
            String compressed_gas_bottle_string;
            String Type_of_Display_string;
            sprintf((char *)&helper_get,"%.1f",(double) p_Controller_->setpoint_carbondioxide);
            sprintf((char *)&Regler_string,"%.1f",(double) p_Controller_->Setpoint);
            sprintf((char *)&Kp_string,"%.1f",(double) p_Controller_->Kp);
            sprintf((char *)&min_open_time_string,"%.1f",(double) p_Controller_->min_open_time);
            sprintf((char *)&dead_zone_string,"%.1f",(double) p_Controller_->dead_zone);
            sprintf((char *)&cycle_time_string,"%.1f",(double) p_Controller_->cycle_time);
            sprintf((char *)&compressed_gas_bottle_string,"%d",(int) p_Controller_->compressed_gas_bottle);
            client.println("<p> Test " + helper_get + " g CO2/l</p>");
            if(header.indexOf("GET /?name=") >=0)
            {
                int index = header.indexOf("&");
                helper_get = header.substring(11,index); 
                int index2 = header.indexOf("&",index+1);
                Regler_string = header.substring(index+10,index2);
                index = header.indexOf("&",index2+1);
                Kp_string = header.substring(index2+4,index);
                index2 = header.indexOf("&",index+1);
                min_open_time_string = header.substring(index+9,index2);
                index = header.indexOf("&",index2+1);
                dead_zone_string = header.substring(index2+10,index);
                index2 = header.indexOf("&",index+1);
                cycle_time_string = header.substring(index+11,index2);
                index = header.indexOf("&",index2+1);
                compressed_gas_bottle_string = header.substring(index2+5,index);
                index2 = header.indexOf("&",index+1);
                Type_of_Display_string = header.substring(index+9,index2);
                client.println("<p> Type of Display " + Type_of_Display_string + " </p>");

                p_Controller_->setpoint_carbondioxide = helper_get.toDouble();
                p_Controller_->Setpoint = Regler_string.toDouble();
                p_Controller_->Kp = Kp_string.toDouble();
                p_Controller_->min_open_time = min_open_time_string.toDouble();
                p_Controller_->dead_zone = dead_zone_string.toDouble();
                p_Controller_->cycle_time = cycle_time_string.toInt();
                p_Controller_->compressed_gas_bottle = compressed_gas_bottle_string.toInt();
                p_Basic_config_->type_of_display = Type_of_Display_string.toInt();
                uint32_t calculated_crc = 0;
                crc32_array((uint8_t *) p_Controller_,sizeof(controller_t)-4,&calculated_crc);
                p_Controller_->crc32 = calculated_crc;
                FRAM.write_controller_parameters(p_Controller_,Controller_paramter_offset);
                crc32_array((uint8_t *) p_Basic_config_,sizeof(p_basic_config_t)-4,&calculated_crc);
                p_Basic_config_->crc32 = calculated_crc;
                FRAM.write_basic_config(p_Basic_config_,basic_config_offset);
                init_LCD(p_Basic_config_->type_of_display); 
                lcd_gotoxy(0,0,p_Basic_config_->type_of_display);
                lcd_puts("eManometer ");
                lcd_puts(FIRMWAREVERSION);


            }
            sprintf(helper,"%.2f",(double)Pressure);
            output5State = helper,
            client.println("<p> Druck " + output5State + " bar</p>");
            // If the output5State is off, it displays the ON button       
            sprintf(helper,"%.2f",(double)Temperatur);
            output5State = helper,
            // Display current state, and ON/OFF buttons for GPIO 4  
            client.println("<p>Temperatur " + output5State + " °C</p>");
            // If the output4State is off, it displays the ON button       
            sprintf(helper,"%.2f",carbondioxide);
            output5State = helper,
            // Display current state, and ON/OFF buttons for GPIO 4  
            client.println("<p>Karbonisierung " + output5State + " g CO2/l</p>");
            // If the output4State is off, it displays the ON button  
            sprintf(helper,"%.2f",p_Controller_->setpoint_carbondioxide);
            output5State = helper,
            // Display current state, and ON/OFF buttons for GPIO 4  
            client.println("<p>Sollwert Karbonisierung " + output5State + " g CO2/l</p>");
            sprintf(helper,"%.2f",p_Controller_->Setpoint);
            output5State = helper,
            // Display current state, and ON/OFF buttons for GPIO 4  
            client.println("<p>Sollwert Regler " + output5State + " bar</p>");
            // If the output4State is off, it displays the ON button  
            client.println("<p><form action='/' method='get'>");
            client.println("Sollwert Carbonisierung: <input type='text' id='name' name='name' value='" + helper_get + "'> g CO2/l</br>");
            client.println("Sollwert Druck <input type='text' id='setpoint' name='setpoint' value='" + Regler_string + "'> bar </br>");
            client.println("Regler Kp <input type='text' id='Kp' name='Kp' value='" + Kp_string + "'></br>");
            client.println("Minimale öffnungszeit <input type='text' id='minopen' name='minopen' value='" + min_open_time_string + "'> ms </br>");
            client.println("Dead Zone <input type='text' id='deadzone' name='deadzone' value='" + dead_zone_string + "'> bar </br>");
            client.println("Zykluszeit <input type='text' id='cycletime' name='cycletime' value='" + cycle_time_string + "'> ms </br>");
            client.println("CO2-Flasche <input type='text' id='gas' name='gas' value='" + compressed_gas_bottle_string + "'></br>");
            String Display0 = "";
            String Display1 = "";
            String Display2 = "";
            switch(p_Basic_config_->type_of_display)
            {
              case 0:
              Display0="selected";
              break;
              case 1:
              Display1="selected";
              break;
              case 2:
              Display2="selected";
              break;
              default:
              break;
            }
            client.println("<select name='Display'> <option value=0" + Display0 +  " >Kein Display</option><option value=1 " + Display1 +  " >SSD1306</option><option value=2 " + Display2 +  " >SH1106</option></select></br>");
            client.println("<input type='hidden' name='UserBrowser' value=''>");
            client.println("<input type='submit' value='Submit'>");
            client.println("</form></p>");
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
#endif
  
  diff_time = millis() - start_time;
  if(diff_time >= interval_send_data)  // 60 sec send part
  {
    
    //requestTemp();
    uploadData();
    start_time = millis();

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

    unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED

    disp->clear();
    disp->setLine(0);
    disp_print_header();
  
    previousMillis = currentMillis;
    blink++;
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

    disp->sync();

  }

  if (currentMillis - next_calc >= p_Controller_->calc_time) {
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
