/**************************************************************

"iSpindel"
All rights reserverd by S.Lang <universam@web.de>

**************************************************************/

// includes go here
#include <PubSubClient.h>
#include "Globals.h"
#include "MPUOffset.h"
// #endif
#include "OneWire.h"
#include "Wire.h"
// #include <Ticker.h>
#include "DallasTemperature.h"
#include "DoubleResetDetector.h" // https://github.com/datacute/DoubleResetDetector
#include "RunningMedian.h"
#include "WiFiManagerKT.h"
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <FS.h>          //this needs to be first
#include "tinyexpr.h"
#include "MCP3221A5T-E.h"
#include <Ticker.h>
#include <regex.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MR44V064B.h"
#include "OLED.h"

#include "Sender.h"
// !DEBUG 1

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

// iGauge new stuff
unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 250;           // interval at which to blink (milliseconds)

double setpoint_pressure = 0;
controller_t Controller_;
p_controller_t p_Controller_ = &Controller_;
statistics_t Statistic_;
p_statistics_t p_Statistic_ = &Statistic_;
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
WiFiServer server(80);
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
MPU6050_Base accelgyro;
MCP3221_Base ADC_;
OneWire *oneWire;
DallasTemperature DS18B20;
MR44V064B_Base FRAM; 
DeviceAddress tempDeviceAddress;
Ticker flasher;
RunningMedian samples = RunningMedian(MEDIANROUNDSMAX);
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);
uint32_t diff_time = 0;
uint32_t interval_send_data = 30000;
uint32_t start_time = 0;
#define TEMP_CELSIUS 0
#define TEMP_FAHRENHEIT 1
#define TEMP_KELVIN 2

int detectTempSensor(const uint8_t pins[]);
bool testAccel();

#ifdef USE_DMP
#include "MPU6050.h"

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q;        // [w, x, y, z]         quaternion container
VectorInt16 aa;      // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;  // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld; // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity; // [x, y, z]            gravity vector
float euler[3];      // [psi, theta, phi]    Euler angle container
float ypr[3];        // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
#endif

bool shouldSaveConfig = false;

char my_token[TKIDSIZE * 2];
char my_name[TKIDSIZE] = "iGauge000";
char my_server[TKIDSIZE];
char my_url[TKIDSIZE * 2];
char my_db[TKIDSIZE] = "iGauge";
char my_username[TKIDSIZE];
char my_password[TKIDSIZE];
char my_job[TKIDSIZE] = "iGauge";
char my_instance[TKIDSIZE] = "000";
//char my_polynominal[100] = "-0.00031*tilt^2+0.557*tilt-14.054";

String my_ssid;
String my_psk;
uint8_t my_api;
uint32_t my_sleeptime = 15 * 60;
uint16_t my_port = 80;
float my_vfact = ADCDIVISOR;
int16_t my_aX = UNINIT, my_aY = UNINIT, my_aZ = UNINIT;
uint8_t my_tempscale = TEMP_CELSIUS;
int8_t my_OWpin = -1;

uint32_t DSreqTime = 0;
float pitch, roll;

int16_t ax, ay, az;
float Volt, Temperatur, Pressure, carbondioxide; // , corrGravity;



//iGauge new Stuff 

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
  if (my_tempscale == TEMP_CELSIUS)
    return t;
  else if (my_tempscale == TEMP_FAHRENHEIT)
    return (1.8f * t + 32);
  else if (my_tempscale == TEMP_KELVIN)
    return t + 273.15f;
  else
    return t; // Invalid value for my_tempscale => default to celsius
}

String tempScaleLabel(void)
{
  if (my_tempscale == TEMP_CELSIUS)
    return "C";
  else if (my_tempscale == TEMP_FAHRENHEIT)
    return "F";
  else if (my_tempscale == TEMP_KELVIN)
    return "K";
  else
    return "C"; // Invalid value for my_tempscale => default to celsius
}

// callback notifying us of the need to save config
void saveConfigCallback()
{
  shouldSaveConfig = true;
}

void applyOffset()
{
  if (my_aX != UNINIT && my_aY != UNINIT && my_aZ != UNINIT)
  {
    CONSOLELN(F("applying offsets"));
    accelgyro.setXAccelOffset(my_aX);
    accelgyro.setYAccelOffset(my_aY);
    accelgyro.setZAccelOffset(my_aZ);
  }
  else
    CONSOLELN(F("offsets not available"));
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
            strcpy(my_name, doc["Name"]);
          if (doc.containsKey("Token"))
            strcpy(my_token, doc["Token"]);
          if (doc.containsKey("Server"))
            strcpy(my_server, doc["Server"]);
          if (doc.containsKey("Sleep"))
            my_sleeptime = doc["Sleep"];
          if (doc.containsKey("API"))
            my_api = doc["API"];
          if (doc.containsKey("Port"))
            my_port = doc["Port"];
          if (doc.containsKey("URL"))
            strcpy(my_url, doc["URL"]);
          if (doc.containsKey("DB"))
            strcpy(my_db, doc["DB"]);
          if (doc.containsKey("Username"))
            strcpy(my_username, doc["Username"]);
          if (doc.containsKey("Password"))
            strcpy(my_password, doc["Password"]);
          if (doc.containsKey("Job"))
            strcpy(my_job, doc["Job"]);
          if (doc.containsKey("Instance"))
            strcpy(my_instance, doc["Instance"]);
          if (doc.containsKey("Vfact"))
            my_vfact = doc["Vfact"];
          if (doc.containsKey("TS"))
            my_tempscale = doc["TS"];
          if (doc.containsKey("OWpin"))
            my_OWpin = doc["OWpin"];
          if (doc.containsKey("SSID"))
            my_ssid = (const char *)doc["SSID"];
          if (doc.containsKey("PSK"))
            my_psk = (const char *)doc["PSK"];
          

          my_aX = UNINIT;
          my_aY = UNINIT;
          my_aZ = UNINIT;

          

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

bool startConfiguration()
{
  set_violet();
  lcd_gotoxy(0,2);
  lcd_puts_invert("Konfig... 192.168.4.1");

  WiFiManager wifiManager;

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
  WiFiManagerParameter custom_vfact("vfact", "Battery conversion factor",
                                    String(my_vfact).c_str(), 7, TYPE_NUMBER);
  WiFiManagerParameter tempscale_list(HTTP_TEMPSCALE_LIST);
  WiFiManagerParameter custom_tempscale("tempscale", "tempscale",
                                        String(my_tempscale).c_str(),
                                        5, TYPE_HIDDEN, WFM_NO_LABEL);

  wifiManager.addParameter(&custom_name);
  wifiManager.addParameter(&custom_sleep);
  wifiManager.addParameter(&custom_vfact);

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
  
  wifiManager.setConfSSID(htmlencode(my_ssid));
  wifiManager.setConfPSK(htmlencode(my_psk));

  CONSOLELN(F("started Portal"));
  wifiManager.startConfigPortal("iSpindel");

  

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

  String tmp = custom_vfact.getValue();
  tmp.trim();
  tmp.replace(',', '.');
  my_vfact = tmp.toFloat();
  if (my_vfact < ADCDIVISOR * 0.8 || my_vfact > ADCDIVISOR * 1.2)
    my_vfact = ADCDIVISOR;

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

bool startConfigurationNormal()
{

  WiFiManager wifiManager;

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
  WiFiManagerParameter custom_vfact("vfact", "Battery conversion factor",
                                    String(my_vfact).c_str(), 7, TYPE_NUMBER);
  WiFiManagerParameter tempscale_list(HTTP_TEMPSCALE_LIST);
  WiFiManagerParameter custom_tempscale("tempscale", "tempscale",
                                        String(my_tempscale).c_str(),
                                        5, TYPE_HIDDEN, WFM_NO_LABEL);

  wifiManager.addParameter(&custom_name);
  wifiManager.addParameter(&custom_sleep);
  wifiManager.addParameter(&custom_vfact);

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
  
  wifiManager.setConfSSID(htmlencode(my_ssid));
  wifiManager.setConfPSK(htmlencode(my_psk));

  CONSOLELN(F("started Portal"));
  wifiManager.startConfigPortalNormal("iSpindel");

  

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

  String tmp = custom_vfact.getValue();
  tmp.trim();
  tmp.replace(',', '.');
  my_vfact = tmp.toFloat();
  if (my_vfact < ADCDIVISOR * 0.8 || my_vfact > ADCDIVISOR * 1.2)
    my_vfact = ADCDIVISOR;

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

  doc["Name"] = my_name;
  doc["Token"] = my_token;
  doc["Sleep"] = my_sleeptime;
  // first reboot is for test
  my_sleeptime = 1;
  doc["Server"] = my_server;
  doc["API"] = my_api;
  doc["Port"] = my_port;
  doc["URL"] = my_url;
  doc["DB"] = my_db;
  doc["Username"] = my_username;
  doc["Password"] = my_password;
  doc["Job"] = my_job;
  doc["Instance"] = my_instance;
  doc["Vfact"] = my_vfact;
  doc["TS"] = my_tempscale;
  doc["OWpin"] = my_OWpin;

  // Store current Wifi credentials
  doc["SSID"] = WiFi.SSID();
  doc["PSK"] = WiFi.psk();

 
  doc["aX"] = my_aX;
  doc["aY"] = my_aY;
  doc["aZ"] = my_aZ;

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
      if (interval != my_sleeptime &&
          interval < 24 * 60 * 60 &&
          interval > 10)
      {
        my_sleeptime = interval;
        CONSOLE(F("Received new Interval config: "));
        CONSOLELN(interval);
        return saveConfig();
      }
    }
  return false;
}

bool uploadData(uint8_t service)
{
  SenderClass sender;

#ifdef API_UBIDOTS
  if (service == DTUbiDots)
  {
    sender.add("temperature", scaleTemperature(Temperatur));
    sender.add("pressure", Pressure);
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", my_sleeptime);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
    CONSOLELN(F("\ncalling Ubidots"));
    return sender.sendUbidots(my_token, my_name);
  }
#endif

#ifdef API_MQTT
  if (service == DTMQTT)
  {
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
    return sender.sendMQTT(my_server, my_port, my_username, my_password, my_name);
  }
#endif

#ifdef API_INFLUXDB
  if (service == DTInfluxDB)
  {
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
    CONSOLELN(String(F("Sending to db: ")) + my_db + String(F(" w/ credentials: ")) + my_username + String(F(":")) + my_password);
    return sender.sendInfluxDB(my_server, my_port, my_db, my_name, my_username, my_password);
  }
#endif

#ifdef API_PROMETHEUS
  if (service == DTPrometheus)
  {
    
    sender.add("temperature", Temperatur);
    sender.add("pressure", Pressure);
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", interval_send_data);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
    sender.add("RSSI", WiFi.RSSI());
    CONSOLELN(F("\ncalling Prometheus Pushgateway"));
    return sender.sendPrometheus(my_server, my_port, my_job, my_instance);
  }
#endif

#ifdef API_GENERIC
  if ((service == DTHTTP) || (service == DTCraftBeerPi) || (service == DTiSPINDELde) || (service == DTTCP))
  {

    sender.add("name", my_name);
    sender.add("ID", ESP.getChipId());
    if (my_token[0] != 0)
      sender.add("token", my_token);
    sender.add("temperature", scaleTemperature(Temperatur));
    sender.add("temp_units", tempScaleLabel());
    sender.add("battery", Volt);
    sender.add("type","eManometer");
    sender.add("pressure", Pressure);
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", interval_send_data);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);
    

    if (service == DTHTTP)
    {
      CONSOLELN(F("\ncalling HTTP"));
      return sender.sendGenericPost(my_server, my_url, my_port);
    }
    else if (service == DTCraftBeerPi)
    {
      CONSOLELN(F("\ncalling CraftbeerPi"));
      return sender.sendGenericPost(my_server, CBP_ENDPOINT, 5000);
    }
    else if (service == DTiSPINDELde)
    {
      CONSOLELN(F("\ncalling iSPINDELde"));
      return sender.sendTCP("ispindle.de", 9501);
    }
    else if (service == DTTCP)
    {
      CONSOLELN(F("\ncalling TCP"));
      String response = sender.sendTCP(my_server, my_port);
      return processResponse(response);
    }
  }
#endif // DATABASESYSTEM

#ifdef API_FHEM
  if (service == DTFHEM)
  {
    
    
    sender.add("temperature", scaleTemperature(Temperatur));
    sender.add("temp_units", tempScaleLabel());
    sender.add("pressure", Pressure);
    sender.add("carbondioxid", carbondioxide);
    sender.add("interval", interval_send_data);
    sender.add("RSSI", WiFi.RSSI());
    sender.add("Opening_times",p_Statistic_->times_open);
    sender.add("Open_time",p_Statistic_->opening_time/1000);    
    sender.add("ID", ESP.getChipId());
    CONSOLELN(F("\ncalling FHEM"));
    return sender.sendFHEM(my_server, my_port, my_name);
  }
#endif // DATABASESYSTEM ==
#ifdef API_TCONTROL
  if (service == DTTcontrol)
  {
    sender.add("T", scaleTemperature(Temperatur));
    //sender.add("D", Tilt);
    sender.add("U", Volt);
    //sender.add("G", Gravity);
    CONSOLELN(F("\ncalling TCONTROL"));
    return sender.sendTCONTROL(my_server, 4968);
  }
#endif // DATABASESYSTEM ==
  return false;
}

void goodNight(uint32_t seconds)
{

  uint32_t _seconds = seconds;
  uint32_t left2sleep = 0;
  uint32_t validflag = RTCVALIDFLAG;

  drd.stop();

  // workaround for DS not floating
  pinMode(my_OWpin, OUTPUT);
  digitalWrite(my_OWpin, LOW);

  // we need another incarnation before work run
  if (_seconds > MAXSLEEPTIME)
  {
    left2sleep = _seconds - MAXSLEEPTIME;
    ESP.rtcUserMemoryWrite(RTCSLEEPADDR, &left2sleep, sizeof(left2sleep));
    ESP.rtcUserMemoryWrite(RTCSLEEPADDR + 1, &validflag, sizeof(validflag));
    CONSOLELN(String(F("\nStep-sleep: ")) + MAXSLEEPTIME + "s; left: " + left2sleep + "s; RT: " + millis());
    ESP.deepSleep(MAXSLEEPTIME * 1e6, WAKE_RF_DISABLED);
  }
  // regular sleep with RF enabled after wakeup
  else
  {
    // clearing RTC to mark next wake
    left2sleep = 0;
    ESP.rtcUserMemoryWrite(RTCSLEEPADDR, &left2sleep, sizeof(left2sleep));
    ESP.rtcUserMemoryWrite(RTCSLEEPADDR + 1, &validflag, sizeof(validflag));
    CONSOLELN(String(F("\nFinal-sleep: ")) + _seconds + "s; RT: " + millis());
    // WAKE_RF_DEFAULT --> auto reconnect after wakeup
    ESP.deepSleep(_seconds * 1e6, WAKE_RF_DEFAULT);
  }
  // workaround proper power state init
  delay(500);
}
void sleepManager()
{
  uint32_t left2sleep, validflag;
  ESP.rtcUserMemoryRead(RTCSLEEPADDR, &left2sleep, sizeof(left2sleep));
  ESP.rtcUserMemoryRead(RTCSLEEPADDR + 1, &validflag, sizeof(validflag));

  // check if we have to incarnate again
  if (left2sleep != 0 && !drd.detectDoubleReset() && validflag == RTCVALIDFLAG)
  {
    goodNight(left2sleep);
  }
  else
  {
    CONSOLELN(F("Worker run!"));
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
  if (my_OWpin == -1)
  {
    my_OWpin = detectTempSensor(OW_PINS);
    if (my_OWpin == -1)
    {
      CONSOLELN(F("ERROR: cannot find a OneWire Temperature Sensor!"));
      return;
    }
  }
  pinMode(my_OWpin, OUTPUT);
  digitalWrite(my_OWpin, LOW);
  delay(100);
  oneWire = new OneWire(my_OWpin);
  DS18B20 = DallasTemperature(oneWire);
  DS18B20.begin();

  bool device = DS18B20.getAddress(tempDeviceAddress, 0);
  if (!device)
  {
    my_OWpin = detectTempSensor(OW_PINS);
    if (my_OWpin == -1)
    {
      CONSOLELN(F("ERROR: cannot find a OneWire Temperature Sensor!"));
      return;
    }
    else
    {
      delete oneWire;
      oneWire = new OneWire(my_OWpin);
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

void initAccel()
{
  // join I2C bus (I2Cdev library doesn't do this automatically)
  Wire.begin(D2, D1);
  Wire.setClock(100000);
  Wire.setClockStretchLimit(2 * 230);

  // init the Accel
  //accelgyro.initialize();
  //accelgyro.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
  //accelgyro.setFullScaleGyroRange(MPU6050_GYRO_FS_250);
  //accelgyro.setDLPFMode(MPU6050_DLPF_BW_5);
  //accelgyro.setTempSensorEnabled(true);
#ifdef USE_DMP
  //accelgyro.setDMPEnabled(true);
  //packetSize = accelgyro.dmpGetFIFOPacketSize();
#endif
  //accelgyro.setInterruptLatch(0); // pulse
  //accelgyro.setInterruptMode(1);  // Active Low
  //accelgyro.setInterruptDrive(1); // Open drain
  //accelgyro.setRate(17);
  //accelgyro.setIntDataReadyEnabled(true);
  //testAccel();
}

float calculateTilt()
{
  //float _ax = ax;
  //float _ay = ay;
  //float _az = az;
  //float pitch = (atan2(_ay, sqrt(_ax * _ax + _az * _az))) * 180.0 / M_PI;
  //float roll = (atan2(_ax, sqrt(_ay * _ay + _az * _az))) * 180.0 / M_PI;
  //return sqrt(pitch * pitch + roll * roll);
  return 1;
}

bool testAccel()
{
  uint8_t res = Wire.status();
  if (res != I2C_OK)
    CONSOLELN(String(F("I2C ERROR: ")) + res);

  bool con = accelgyro.testConnection();
  if (!con)
    CONSOLELN(F("Acc Test Connection ERROR!"));

  return res == I2C_OK && con == true;
}

void getAccSample()
{
  accelgyro.getAcceleration(&ax, &az, &ay);
}

float getTilt()
{
  uint32_t start = millis();
  uint8_t i = 0;

  for (; i < MEDIANROUNDSMAX; i++)
  {
    while (!accelgyro.getIntDataReadyStatus())
      delay(2);
    getAccSample();
    float _tilt = calculateTilt();
    samples.add(_tilt);

    if (i >= MEDIANROUNDSMIN && isDS18B20ready())
      break;
  }
  CONSOLE("Samples:");
  CONSOLE(++i);
  CONSOLE(" min:");
  CONSOLE(samples.getLowest());
  CONSOLE(" max:");
  CONSOLE(samples.getHighest());
  CONSOLE(" time:");
  CONSOLELN(millis() - start);
  return samples.getAverage(MEDIANAVRG);
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
    pinMode(my_OWpin, OUTPUT);
    digitalWrite(my_OWpin, LOW);
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

float getBattery()
{
  analogRead(A0); // drop first read
  return analogRead(A0) / my_vfact;
}

float calculateGravity()
{
  //double _tilt = Tilt;
  //double _temp = Temperatur;
  //float _gravity = 0;
  //int err;
  //te_variable vars[] = {{"tilt", &_tilt}, {"temp", &_temp}};
  
}

void flash()
{
  // triggers the LED
  //Volt = getBattery();
  //if (testAccel())
   // getAccSample();
  //Tilt = calculateTilt();
  //Temperatur = getTemperature(false);
  //Gravity = calculateGravity();
  //requestTemp();
}

bool isSafeMode(float _volt)
{
  if (_volt < LOWBATT)
  {
    CONSOLELN(F("\nWARNING: low Battery"));
    return true;
  }
  else
    return false;
}

void connectBackupCredentials()
{
  WiFi.disconnect();
  WiFi.begin(my_ssid.c_str(), my_psk.c_str());
  //wifiManager.startConfigPortal("iSpindel",NULL,true);
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

void setup()
{
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
  delay(1000);
  digitalWrite(LED_RED ,0);
  digitalWrite(LED_GREEN,1);
  delay(1000);
  digitalWrite(LED_GREEN ,0);
  digitalWrite(LED_BLUE,1);
  delay(1000);
  digitalWrite(LED_BLUE,0);
  ADC_.MCP3221_init(); // Init I2C and Reset broken transmission.
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
  }
      
  delay(500);
  init_LCD();
  lcd_gotoxy(0,0);
  lcd_puts("eManometer ");
  lcd_puts(FIRMWAREVERSION);
  
  
  //Initialize Ticker every 0.5s
   // timer1_attachInterrupt(onTimerISR);
   // timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
   // timer1_write(600000); //120000 us
  CONSOLELN(F("\nFW " FIRMWAREVERSION));
  CONSOLELN(ESP.getSdkVersion());

  sleepManager();

  bool validConf = readConfig();
  if (!validConf)
    CONSOLELN(F("\nERROR config corrupted"));
  initDS18B20();
  
  //initAccel();

  // decide whether we want configuration mode or normal mode
  if (shouldStartConfig(validConf))
  {
    uint32_t tmp;
    ESP.rtcUserMemoryRead(WIFIENADDR, &tmp, sizeof(tmp));

    // DIRTY hack to keep track of WAKE_RF_DEFAULT --> find a way to read WAKE_RF_*
    if (tmp != RTCVALIDFLAG)
    {
      drd.setRecentlyResetFlag();
      tmp = RTCVALIDFLAG;
      ESP.rtcUserMemoryWrite(WIFIENADDR, &tmp, sizeof(tmp));
      CONSOLELN(F("reboot RFCAL"));
      ESP.deepSleep(100000, WAKE_RFCAL);
      delay(500);
    }
    else
    {
      tmp = 0;
      ESP.rtcUserMemoryWrite(WIFIENADDR, &tmp, sizeof(tmp));
    }

    //flasher.attach(1, flash);

    // rescue if wifi credentials lost because of power loss
    if (!startConfiguration())
    {
      // test if ssid exists
      if (WiFi.SSID() == "" &&
          my_ssid != "" && my_psk != "")
      {
        connectBackupCredentials();
      }
    }
    uint32_t left2sleep = 0;
    ESP.rtcUserMemoryWrite(RTCSLEEPADDR, &left2sleep, sizeof(left2sleep));

    //flasher.detach();
  }
  // to make sure we wake up with STA but AP
  WiFi.mode(WIFI_STA);
  Volt = getBattery();
  // we try to survive
  if (isSafeMode(Volt))
    WiFi.setOutputPower(0);
  else
    WiFi.setOutputPower(20.5);

#ifndef USE_DMP
  //Tilt = getTilt();
#else
  while (fifoCount < packetSize)
  {
    //do stuff
    CONSOLELN(F("wait DMP"));

    fifoCount = accelgyro.getFIFOCount();
  }
  if (fifoCount == 1024)
  {
    CONSOLELN(F("FIFO overflow"));
    accelgyro.resetFIFO();
  }
  else
  {
    fifoCount = accelgyro.getFIFOCount();

    accelgyro.getFIFOBytes(fifoBuffer, packetSize);

    accelgyro.resetFIFO();

    fifoCount -= packetSize;

    accelgyro.dmpGetQuaternion(&q, fifoBuffer);
    accelgyro.dmpGetEuler(euler, &q);

    /*
    for (int i = 1; i < 64; i++) {
    CONSOLE(fifoBuffer[i]);
    CONSOLE(" ");
    }
    */

    CONSOLE(F("euler\t"));
    CONSOLE((euler[0] * 180 / M_PI));
    CONSOLE("\t");
    CONSOLE(euler[1] * 180 / M_PI);
    CONSOLE("\t");
    CONSOLELN(euler[2] * 180 / M_PI);

    ax = euler[0];
    ay = euler[2];
    az = euler[1];

    float _ax = ax;
    float _ay = ay;
    float _az = az;
    float pitch = (atan2(_ay, sqrt(_ax * _ax + _az * _az))) * 180.0 / M_PI;
    float roll = (atan2(_ax, sqrt(_ay * _ay + _az * _az))) * 180.0 / M_PI;
    //Tilt = sqrt(pitch * pitch + roll * roll);
  }
#endif

  //float accTemp = accelgyro.getTemperature() / 340.00 + 36.53;
  //accelgyro.setSleepEnabled(true);
  uint16_t raw_data = ADC_.MCP3221_getdata();
  CONSOLE("raw data: ");
  CONSOLELN(raw_data);
  raw_data -= 0x163; // set 0,5V to zero;
  Pressure = (float((double)raw_data * 0.0030517578125));
  //CONSOLE(F("Tilt: "));
  //CONSOLELN(Tilt);
  //CONSOLE("Tacc: ");
  //CONSOLELN(accTemp);
  CONSOLE("Volt: ");
  CONSOLELN(Volt);

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
    uploadData(my_api);
  }
  else
  {
    connectBackupCredentials();
    CONSOLELN(F("failed to connect"));
  }
  start_time = millis();
  // survive - 60min sleep time
  //if (isSafeMode(Volt))
  //  my_sleeptime = EMERGENCYSLEEP;
  //goodNight(my_sleeptime);
        // Draw characters of the default font

  server.begin();
  pinMode(MosFET, OUTPUT);
  
  
}

void loop()
{
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
                client.println("<p> Test2 " + Regler_string + " </p>");
                p_Controller_->setpoint_carbondioxide = helper_get.toDouble();
                p_Controller_->Setpoint = Regler_string.toDouble();
                p_Controller_->Kp = Kp_string.toDouble();
                p_Controller_->min_open_time = min_open_time_string.toDouble();
                p_Controller_->dead_zone = dead_zone_string.toDouble();
                p_Controller_->cycle_time = cycle_time_string.toInt();
                p_Controller_->compressed_gas_bottle = compressed_gas_bottle_string.toInt();
                uint32_t calculated_crc = 0;
                crc32_array((uint8_t *) p_Controller_,sizeof(controller_t)-4,&calculated_crc);
                p_Controller_->crc32 = calculated_crc;
                FRAM.write_controller_parameters(p_Controller_,Controller_paramter_offset);

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

  
  diff_time = millis() - start_time;
  if(diff_time >= interval_send_data)  // 60 sec send part
  {
    
    //requestTemp();
    uploadData(my_api);
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
      if(raw_data >= 0x163)
          raw_data -= 0x163; // Set Zero to 0,5V
        else
          raw_data = 0;
      if(raw_data >= 1246)
        {
          set_red();
        }
        else
        {
          if(carbondioxide < p_Controller_->setpoint_carbondioxide * 0.6)
            {
              set_blue();
            }
            else
            {
              if(carbondioxide < p_Controller_->setpoint_carbondioxide * 0.95)
              {
                 set_turkis();
              }
              else
              {
                if(carbondioxide < p_Controller_->setpoint_carbondioxide * 1.05)
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
    
    AddToFloatAvg(&pressure_filtered,((double)raw_data * 0.0030517578125));
    Pressure = GetOutputValue(&pressure_filtered);
    double exponent = -10.73797 + (2617.25 / ( Temperatur + 273.15 ));
    carbondioxide = (float)(((double)Pressure + 1.013) * 10 * exp(exponent));

    unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    blink++;
    char buffer_[20];
    sprintf(buffer_,"Druck: %.2f bar  ",Pressure);
    lcd_gotoxy(0,1);
    lcd_puts(buffer_);
    sprintf(buffer_,"Temp: %.2f 'C",Temperatur);
    lcd_gotoxy(0,2);
    lcd_puts(buffer_);
    sprintf(buffer_,"CO2: %.2f g/l  ",carbondioxide);
    lcd_gotoxy(0,3);
    lcd_puts(buffer_);
    sprintf(buffer_,"Zeit: %.2f s",p_Statistic_->opening_time/1000);
    lcd_gotoxy(0,4);
    lcd_puts(buffer_);
    sprintf(buffer_,"Anzahl: %d",p_Statistic_->times_open);
    lcd_gotoxy(0,5);
    lcd_puts(buffer_);
    if(p_Controller_->compressed_gas_bottle)
    {
      lcd_gotoxy(0,6);
      lcd_puts("CO2-Flasche");


    }
    else
    {
      lcd_gotoxy(0,6);
      lcd_puts("Gärung     ");
    }
    

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
