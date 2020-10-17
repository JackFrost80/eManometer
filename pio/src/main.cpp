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
#include "display_adafruit_ssd1306.h"
#include "display_ssd1306_custom.h"
#include "timer.h"
#include "Sender.h"
#include "median.h"
#include "mean.h"
#include "crc.h"

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
char serial_data[1024];
char Live_data_direct[256];
String Live_data;
uint32_t serial_length = 0;
bool data_ready  = false;
bool data_begin = false;
bool type_done = false;
uint8_t type = 0;
Recipe_t recipe;
p_Recipe_t p_recipe = &recipe;
brew_documentation_t brewdocu;
p_brew_documentation_t p_brewdocu = &brewdocu;
WiFiServer Software_connection(Software_port);
uint8_t stream_data[128];
uint8_t stream_length = 0;
String Data_recieved;
IPAddress IP_adresse;





static bool ds_failure = false;
static timeout timer_init_ds;

#define start_tx  0x02
#define end_tx  0x03
#define mask  0x10

#define send_recipe 0x10

#define MosFET D0
#define LED_RED D4
#define LED_GREEN D5
#define LED_BLUE D2
#define Hz_1 0x04
#define Hz_0_25 0x01
#define Hz_0_5  0x02

uint32_t blink = 0;

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

float  Temperatur, Setpoint, gradient;
uint32_t output,time_left; 

void blink_yellow()
{
  uint32 helper = blink & 0x01UL;
  if(helper == 1) {
    
    digitalWrite(LED_BLUE,1);
    
  }
  else {
  
    digitalWrite(LED_BLUE,0);
    
  }
}

bool readConfig()
{
  CONSOLE(F("mounting FS..."));

  if (SPIFFS.begin())
  {
    //CONSOLELN(F(" mounted!"));
    if (SPIFFS.exists(CFGFILE))
    {
      // file exists, reading and loading
      //CONSOLELN(F("reading config file"));
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
          if (doc.containsKey("Default_IP"))
            g_flashConfig.default_ip = doc["Default_IP"].as<String>();
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
          //CONSOLELN(F("parsed config:"));
          //serializeJson(doc, Serial);
          //CONSOLELN();
#endif
          return true;
        }
        else
        {
          //CONSOLELN(F("ERROR: failed to load json config"));
          return false;
        }
      }
      //CONSOLELN(F("ERROR: unable to open config file"));
    }
  }
  else
  {
    //CONSOLELN(F(" ERROR: failed to mount FS!"));
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
  //CONSOLELN(ESP.getResetReason());

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
    //CONSOLELN(F("\nERROR no Wifi credentials"));

  int test = digitalRead(D7);

  bool config = false;

  if (!validConf) {
    //CONSOLELN(F("going to config mode. Reaseon: no valid config"));
    config = true;
  }
  if (_dblreset) {
    //CONSOLELN(F("going to config mode. Reaseon: double reset detected"));
    config = true;
  }
  if (!_wifiCred) {
    //CONSOLELN(F("going to config mode. Reaseon: no WIFI credentials"));
    config = true;
  }
  //if (test == 0) {
    //CONSOLELN(F("going to config mode. Reaseon: button held down"));
    //config = true;
  //}

  if (config) 
    return true;
  else {
    //CONSOLELN(F("normal mode"));
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
  //CONSOLELN(SPIFFS.format());
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
  doc["Default_IP"] = g_flashConfig.default_ip;
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
    //CONSOLELN(F("failed to open config file for writing"));
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
    //CONSOLELN(F("saved successfully"));
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
        //CONSOLELN(interval);
        return saveConfig();
      }
    }
  return false;
}

bool forwardUbidots()
{
    SenderClass sender;
    sender.add("temperature",Temperatur);
    sender.add("setpoint", Setpoint);
    sender.add("gradient", gradient);
    sender.add("output",output);
    sender.add("time_left",time_left);
    sender.add("interval", g_flashConfig.interval);
    return sender.sendUbidots(g_flashConfig.token, g_flashConfig.name);
}


bool forwardGeneric()
{
    SenderClass sender;
    sender.add("name", g_flashConfig.name);
    sender.add("ID", ESP.getChipId());
    sender.add("type", "IDS2");
    if (g_flashConfig.token[0] != 0)
      sender.add("token", g_flashConfig.token);
    sender.add("temperature", Temperatur);
    sender.add("setpoint", Setpoint);
    sender.add("gradient", gradient);
    sender.add("cv",output);
    sender.add("time_left",time_left);
    sender.add("interval", g_flashConfig.interval);
    sender.add("RSSI", WiFi.RSSI());
    //sender.add("Data", Data_recieved);

    
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



String split(String s, char parser, int index) {
  String rs="";
  int parserIndex = index;
  int parserCnt=0;
  int rFromIndex=0, rToIndex=-1;
  while (index >= parserCnt) {
    rFromIndex = rToIndex+1;
    rToIndex = s.indexOf(parser,rFromIndex);
    if (index == parserCnt) {
      if (rToIndex == 0 || rToIndex == -1) return "";
      return s.substring(rFromIndex,rToIndex);
    } else parserCnt++;
  }
  return rs;
}



void connectBackupCredentials()
{
  WiFi.disconnect();
  WiFi.begin(g_flashConfig.ssid.c_str(), g_flashConfig.psk.c_str());
  //wifiManager->startConfigPortal("iSpindel",NULL,true);
  //CONSOLELN(F("Rescue Wifi credentials"));
  delay(100);
}

void reset__brew_documentation(p_brew_documentation_t value)
{
	value->iodine_time = 0;
	value->mash_time = 0;
	value->sparge_time = 0;
	for(uint8_t i=0;i<number_of_rasts;i++)
	{
		value->rast_time[i] = 0;
		value->rast_temp[i] = 0;
		value->rast_max[i] = 0;
		value->rast_min[i] = 0;
	}
	for(uint8_t i=0;i<Error_ammunt;i++)
	{
		value->Error_data[i].Timestamp = 0;
		value->Error_data[i].Type = 0;
		
	}
	value->Error_index = 0;
	for(uint8_t i=0;i<number_of_hop_timers;i++)
	{
		value->Hop_data[i].Timestamp = 0;
		value->Hop_data[i].Type = 0;
		
	}
	value->hop_index = 0;
	for(uint8_t i=0;i<Hope_flame_out_data;i++)
	{
		value->hop_flameout[i].Timestamp = 0;
		value->hop_flameout[i].Type = 0;
		value->hop_flameout[i].Temperature = 0;
		
	}
	value->hop_flameout_index = 0;
	value->boil_temp = 0;
	value->cooling_start = 0;
	value->cooling_isomer = 0;
	value->cooling_end = 0;
	value->irish_moos = 0;
	value->pH_value = 7;
	value->pH_timestamp = 0;
		
	uint32_t calculated_crc = 0xFFFFFFFF;
	crc32_array((uint8_t *)value, sizeof(p_brew_documentation_t) - sizeof(uint32_t), &calculated_crc);
	value->crc = calculated_crc;

}






void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200);
  //pinMode(LED_BLUE, OUTPUT);
  //pinMode(2, OUTPUT);
  //digitalWrite(2,LOW);
  //Serial1.write(0x55);
  for(uint8_t i = 0;i<100;i++)
  {
    stream_data[i] = 0;
  }
  stream_length = 0;
  reset__brew_documentation(p_brewdocu);
  uint32_t calculated_crc = 0xffffffff;
	        crc32_array((uint8_t *)p_recipe, sizeof(Recipe_t) - sizeof(uint32_t), &calculated_crc);
          if(calculated_crc != p_recipe->crc)
          {
          // Init recipe
            p_recipe->ID = 27;
            p_recipe->temperatur_mash = 47.0;
            p_recipe->rast_temp[0] = 45.0;
            p_recipe->rast_time[0] = 600;
            p_recipe->dekoktion[0] = 0;
            p_recipe->rast_temp[1] = 63.0;
            p_recipe->rast_time[1] = 1800;
            p_recipe->dekoktion[1] = 0;
            p_recipe->rast_temp[2] = 72.0;
            p_recipe->rast_time[2] = 1800;
            p_recipe->dekoktion[2] = 0;
            p_recipe->rast_temp[3] = 20.0;
            p_recipe->rast_time[3] = 10;
            p_recipe->dekoktion[3] = 0;
            p_recipe->rast_temp[4] = 20.0;
            p_recipe->rast_time[4] = 10;
            p_recipe->dekoktion[4] = 0;
            p_recipe->rast_temp[5] = 20.0;
            p_recipe->rast_time[5] = 10;
            p_recipe->dekoktion[5] = 0;
            p_recipe->rast_temp[6] = 20.0;
            p_recipe->rast_time[6] = 10;
            p_recipe->dekoktion[6] = 0;
            p_recipe->boil_time = 5400;
            p_recipe->hop_time[0] = 3600; 
            p_recipe->hop_time[1] = 600; 
            p_recipe->hop_time[2] = 0;
            p_recipe->hop_time[3] = 0; 
            p_recipe->lauter_temp = 77;
            p_recipe->cooling_temp = 20.0;
            p_recipe->hop_temp_flame_out[0] = 49.0;
            p_recipe->hop_temp_flame_out[1] = 49.0;
            p_recipe->hop_time_flame_out[0] = 0;
            p_recipe->hop_time_flame_out[1] = 0;
            p_recipe->lauter_temp = 77;
            p_recipe->cooler = 1;
            p_recipe->Irish_moos = 1;
            p_recipe->yeast = 1;
            calculated_crc = 0xffffffff;
            crc32_array((uint8_t *)p_recipe, sizeof(Recipe_t) - sizeof(uint32_t), &calculated_crc);
            p_recipe->crc = calculated_crc;
          }

  //Initialize Ticker every 0.5s
   // timer1_attachInterrupt(onTimerISR);
   // timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
   // timer1_write(600000); //120000 us
  

  webserver = new Webserver();
  
  
  bool validConf = readConfig();
  if (!validConf)
    //CONSOLELN(F("\nERROR config corrupted"));
  
  
  // decide whether we want configuration mode or normal mode
  if (shouldStartConfig(validConf)) {
    webserver->startWifiManager();
  }
  else {
    WiFi.mode(WIFI_STA);
  }

  WiFi.setOutputPower(20.5);

  
  
  
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
    
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    
  }
  else
  {
    connectBackupCredentials();
    
  }

  webserver->setConfSSID(htmlencode(g_flashConfig.ssid));
  webserver->setConfPSK(htmlencode(g_flashConfig.psk));

  webserver->startWebserver();
  Software_connection.begin(); // Start Server for PC-Software
  IP_adresse.fromString(g_flashConfig.default_ip);
    
}



void loop()
{
  static unsigned long prev = micros();
  unsigned long now = micros();
  unsigned long looptime = now - prev;

  static timeout timer_apicall(g_flashConfig.interval * 1000);
  static timeout timer_display;
  static int temp_raw = 0;
  static int char_count = 0;
  static int char_to_recive = 0;
  static bool count_type = false;
  static bool count_recieved = false;
  static bool check_command = false;
  static bool data_stream_recieved = false;
  static bool data_to_return = false;
  static bool data_to_send = false;
  static bool data_to_read = false;
  static uint8_t type_of_read = 0;
  static bool return_data_clear = false;
  static bool return_data_once = false;
  static uint8_t type_to_return = 0;
  static uint8_t type_to_return_once = 0;
  
  char command = 0x00;
  char incomming = 0x00;
  drd.loop();
  webserver->process();
  g_timer_mgr.check_timers();


  WiFiClient clien_re = Software_connection.available();
 
  if (clien_re) {
    //CONSOLELN(F("\nconnected"));
    while (clien_re.connected()) {
      
      
      while (clien_re.available()) {
        IP_adresse =  clien_re.remoteIP();
        uint8_t data = clien_re.read();
        stream_data[stream_length++] = data;
        //Serial.println(data);
        
        
        
      }
 
      delay(10);
      clien_re.write("a");
    }
    
    data_stream_recieved = true;
    //CONSOLELN(F("\nStreamrecivedd"));
    //Serial.print("length: ");
    //Serial.println(stream_length);
    //Serial.println(stream_data[0]);

    //Serial.println("Streamrecived");
    clien_re.stop();
  }
  if(data_stream_recieved)
  {
    data_stream_recieved = false;
    stream_length = 0;
    if(stream_data[0] == 'a') // read recipe
    {
      
      //Serial.print('1');
      //Serial.print('5');
      //CONSOLELN(F("\nAsk for recip"));
      //Serial.println("Ask for recipe");
      data_to_return = true;
      data_to_read = true;
      type_of_read = '5';
      //return_data_clear = true;
      type_to_return = 0x10;
    }
    if(stream_data[0] == 'b') // read recipe
    {
      
      //Serial.print('1');
      //Serial.print('5');
      //CONSOLELN(F("\nRecieved recipe WLAN"));
      memcpy((uint8_t *)p_recipe,(uint8_t *)&stream_data[1],sizeof(Recipe_t));
      //memcpy(p_recipe,&stream_data[1],sizeof(Recipe_t));
      //CONSOLELN(F("\nData copied"));
      ////Serial.printf("MAsh temp: %f \r\n",p_recipe->temperatur_mash);
      data_to_send = true;
      //Serial.printl(n("Ask for recipe");
      //data_to_return = true;
      //return_data_clear = true;
      //type_to_return = 0x10;
    }
    if(stream_data[0] == 'c') // read recipe
    {
      
      //Serial.print('1');
      //Serial.print('5');
      //CONSOLELN(F("\nAsk for recip"));
      //Serial.println("Ask for recipe");
      data_to_return = true;
      data_to_read = true;
      type_of_read = '4';  // Read Brewdocumentation
      //return_data_clear = true;
      type_to_return = 0x10;
    }
  }

  if(data_to_read)
  {
    data_to_read = false;
    Serial.write(mask); //delimeter
    Serial.write(start_tx); //start_frame
    Serial.write(type_of_read); //type od return
    Serial.write(mask); //delimeter
    Serial.write(end_tx); //start_frame

  }

  if(data_to_send)
  {
    //Serial.println("Send Recipe to Controller");
    data_to_send = false;
    Serial.write(mask); //delimeter
    Serial.write(start_tx); //start_frame
    //Serial1.write(mask); //delimeter
    Serial.write('6');
    uint8_t *pointer_to_send = (uint8_t *)p_recipe;
    for(uint16_t i = 0;i<sizeof(Recipe_t);i++)
    {
      if(*pointer_to_send == 0x10)
        Serial.write(mask);
      Serial.write(*pointer_to_send);
      pointer_to_send++;
    }
    Serial.write(mask); //delimeter
    Serial.write(end_tx); //start_frame

  }

  if(return_data_once)
  {
    WiFiClient return_;
    if(return_.connect(IP_adresse,13003))
      {
        return_data_once = false;
        switch(type_to_return_once)
        {
          case 0x13:
          {
            //uint8_t date_to_return[256];
            return_.write((uint8_t *)p_recipe,sizeof(Recipe_t));
            ////Serial.printf("Data lengt, : %d \n" , sizeof(Recipe_t));
            ////Serial.printf("ID, : %d \n" , p_recipe->ID);
            ////Serial.printf("Mash temp: %f",p_recipe->temperatur_mash);
            return_.flush();
            delay(20);
                 
            
            return_.stop();
          }
          case 0x0E:  // Brewdocumentation
          {
            //uint8_t date_to_return[256];
            return_.write((uint8_t *)p_brewdocu,sizeof(brew_documentation_t));
            ////Serial.printf("Data lengt, : %d \n" , sizeof(Recipe_t));
            ////Serial.printf("ID, : %d \n" , p_recipe->ID);
            ////Serial.printf("Mash temp: %f",p_recipe->temperatur_mash);
            return_.flush();
            delay(20);
                 
            
            return_.stop();
          }
          break;
          case 0x0F:  // Live Data
          {
            //uint8_t date_to_return[256];
            uint8_t date_to_return[256];
            Live_data.getBytes(date_to_return,Live_data.length());
            return_.write((uint8_t *)p_brewdocu,sizeof(brew_documentation_t));
            ////Serial.printf("Data lengt, : %d \n" , sizeof(Recipe_t));
            ////Serial.printf("ID, : %d \n" , p_recipe->ID);
            ////Serial.printf("Mash temp: %f",p_recipe->temperatur_mash);
            return_.flush();
            delay(20);
                 
            
            return_.stop();
          }
          break;
          default:
          //Serial.println("Wrong type");
          break;
        
        }
      }
       else
      {
        //Serial.println("connection failed!]");
        return_.stop();
      }
  }

  if(return_data_clear)
  {
    ////Serial.printf("Datenreadyt\n");
    WiFiClient return_;
    

      if(return_.connect(IP_adresse,13002))
      {
        return_data_clear = false;
        //CONSOLELN(F("\nconnected to server"));
        switch(type_to_return)
        {
          case 0x10:
          {
            type_to_return = 0;
            uint8_t date_to_return[256];
            //char *pointer_real_data = &serial_data[1];
            //Live_data.getBytes(date_to_return,Live_data.length());
            return_.write(Live_data_direct,128);
            //////Serial.printf("String: %s \n" , Live_data);
            ////Serial.printf("Daten gesendet\n");
            return_.flush();
            delay(20);
                 
            
            return_.stop();
          }
          break;

          
          default:
          //Serial.println("Wrong type");
          break;
        }
      }
      else
      {
        //Serial.println("connection failed!]");
        return_.stop();
      }
    

  }

  bool configMode = WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
  if (Serial.available() > 0) {
    char helper = Serial.read();
    
    if(!check_command)
    {
      if(helper == mask)
      {
        check_command = true;
      }
      else
      {
        if(data_begin && !data_ready)
        {
            serial_data[serial_length] = helper;
            
            if(serial_length<1024)
              serial_length++;
        }
      }  
    }
    else
    { 
      check_command = false;
      if(helper == end_tx || helper == start_tx)
      {
        if(helper == start_tx)
        {
          data_begin = true;
          serial_length = 0;
        }
        else
        {
          data_ready = true;
          //String str((char*)serial_data);
          // can we see it now?
          //Serial.println(str);
          //Serial.write(serial_data,serial_length);
        }
        
      }
      else
      {
        if(data_begin && !data_ready)
        {
            serial_data[serial_length] = helper;
            if(serial_length<1024)
              serial_length++;
        } 
      }
    }
    
  }
  //Serial1.write(0x55);
    if(data_ready)
    {
      char *pointer_real_data = &serial_data[1];
      String str(pointer_real_data);
      Data_recieved = serial_data;
      //String str((char*)serial_data);
          // can we see it now?
      //Serial.println(str);
      //Live_data = serial_data;
      type = serial_data[0];
      data_ready = false;
      data_begin = false;
      //Serial.println(type);
      switch(type)
      {
        
        case 0x10:  // Live data
        {
          strcpy(Live_data_direct,&serial_data[1]);
          String Sub_data = split(str,';',0);
          Temperatur = Sub_data.toFloat();
          //Temperatur /= 100;
          Sub_data = split(str,';',1);
          Setpoint =  Sub_data.toFloat();
          //Setpoint /= 100;
          Sub_data = split(str,';',2);
          gradient =  Sub_data.toFloat();
          //gradient /= 100;
          Sub_data = split(str,';',3);
          output = Sub_data.toInt();
          Sub_data = split(str,';',4);
          time_left = Sub_data.toInt();
          return_data_clear = true;
          data_to_return = false;
          data_ready = false;
          data_begin = false;
          type_to_return = 0x10;
          uploadData();


        }
        break;
        case 13:  // Live data
        {
          //Serial.println("Recipe recieved");
          char *pointer_real_data = &serial_data[1];
          memcpy((uint8_t *)p_recipe,(uint8_t *)pointer_real_data,sizeof(Recipe_t));
          ////Serial.printf("Rast temp: %f \n\r",p_recipe->temperatur_mash);
          uint32_t calculated_crc = 0xffffffff;
	        crc32_array((uint8_t *)p_recipe, sizeof(Recipe_t) - sizeof(uint32_t), &calculated_crc);
          if(calculated_crc != p_recipe->crc)
          {
            //Serial.println("crc wrong");

          // Init recipe
            p_recipe->temperatur_mash = 47.0;
            p_recipe->rast_temp[0] = 45.0;
            p_recipe->rast_time[0] = 600;
            p_recipe->dekoktion[0] = 0;
            p_recipe->rast_temp[1] = 63.0;
            p_recipe->rast_time[1] = 1800;
            p_recipe->dekoktion[1] = 0;
            p_recipe->rast_temp[2] = 72.0;
            p_recipe->rast_time[2] = 1800;
            p_recipe->dekoktion[2] = 0;
            p_recipe->rast_temp[3] = 20.0;
            p_recipe->rast_time[3] = 10;
            p_recipe->dekoktion[3] = 0;
            p_recipe->rast_temp[4] = 20.0;
            p_recipe->rast_time[4] = 10;
            p_recipe->dekoktion[4] = 0;
            p_recipe->rast_temp[5] = 20.0;
            p_recipe->rast_time[5] = 10;
            p_recipe->dekoktion[5] = 0;
            p_recipe->rast_temp[6] = 20.0;
            p_recipe->rast_time[6] = 10;
            p_recipe->dekoktion[6] = 0;
            p_recipe->boil_time = 5400;
            p_recipe->hop_time[0] = 3600; 
            p_recipe->hop_time[1] = 600; 
            p_recipe->hop_time[2] = 0;
            p_recipe->hop_time[3] = 0; 
            p_recipe->lauter_temp = 77;
            p_recipe->cooling_temp = 20.0;
            p_recipe->hop_temp_flame_out[0] = 49.0;
            p_recipe->hop_temp_flame_out[1] = 49.0;
            p_recipe->hop_time_flame_out[0] = 0;
            p_recipe->hop_time_flame_out[1] = 0;
            p_recipe->lauter_temp = 77;
            p_recipe->cooler = 1;
            p_recipe->Irish_moos = 1;
            p_recipe->yeast = 1;
            calculated_crc = 0xffffffff;
            crc32_array((uint8_t *)p_recipe, sizeof(Recipe_t) - sizeof(uint32_t), &calculated_crc);
            p_recipe->crc = calculated_crc;
            data_ready = false;
            if(data_to_return)
            {
                return_data_once = true;
                data_to_return = false;
                type_to_return_once = 0x13;
            }
          }
          else
          {
            data_ready = false;
            //Serial.println("crc ok");
            if(data_to_return)
            {
                return_data_once = true;
                data_to_return = false;
                type_to_return_once = 0x13;
            }
              
          }
          

        }
        break;
        case 0x0E:  // Brewdocu
        {
          //Serial.println("Recipe recieved");
          char *pointer_real_data = &serial_data[1];
          memcpy((uint8_t *)p_brewdocu,(uint8_t *)pointer_real_data,sizeof(brew_documentation));
          ////Serial.printf("Rast temp: %f \n\r",p_recipe->temperatur_mash);
          uint32_t calculated_crc = 0xffffffff;
	        crc32_array((uint8_t *)p_brewdocu, sizeof(brew_documentation) - sizeof(uint32_t), &calculated_crc);
          data_ready = false;
          if(calculated_crc != p_brewdocu->crc)
          {
            reset__brew_documentation(p_brewdocu);
            if(data_to_return)
            {
                return_data_once = true;
                data_to_return = false;
                type_to_return_once = 0x0E;
            }
          }
          else
          {
            data_ready = false;
            //Serial.println("crc ok");
            if(data_to_return)
            {
                return_data_once = true;
                data_to_return = false;
                type_to_return_once = 0x0E;
            }
              
          }
          

        }
        break;
        default:
        {
            data_ready = false;
            data_begin = false;
        }
          
        break;
      }

    }
    

    

  

  if (timer_expired(timer_apicall)) {
    //requestTemp();
    //uploadData();
    
    set_timer(timer_apicall, g_flashConfig.interval * 1000);
  }

  

  if (configMode) {
    blink_yellow();
  }
  

  
}

