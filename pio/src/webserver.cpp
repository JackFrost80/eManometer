/**************************************************************

    "iSpindel"
    changes by S.Lang <universam@web.de>

 **************************************************************/

/**************************************************************
   WiFiManager is a library for the ESP8266/Arduino platform
   (https://github.com/esp8266/Arduino) to enable easy
   configuration and reconfiguration of WiFi credentials using a Captive Portal
   inspired by:
   http://www.esp8266.com/viewtopic.php?f=29&t=2520
   https://github.com/chriscook8/esp-arduino-apboot
   https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/CaptivePortalAdvanced/
   Forked from Tzapu https://github.com/tzapu/WiFiManager
   Built by Ken Taylor https://github.com/kentaylor
   Licensed under MIT license
 **************************************************************/

#include "webserver.h"
#include <ArduinoJson.h>
#include <vector>
#include <array>

#include "Globals.h"
#include <map>

struct ListItem {
  int val;
  String name;
};

static void addParam(String& page, String id, String label, String value, int len)
{
    String pitem;
    pitem = FPSTR(HTTP_FORM_LABEL);
    pitem += FPSTR(HTTP_FORM_PARAM);
    
    char parLength[2];

    pitem.replace("{i}", id);
    pitem.replace("{n}", id);
    pitem.replace("{p}", label);
    snprintf(parLength, 2, "%d", len);
    pitem.replace("{l}", parLength);
    pitem.replace("{v}", value);

    page += pitem;
}

static void addList(String& page, String id, String label, int value, std::vector<ListItem> items)
{
    String pitem;
    pitem = FPSTR(HTTP_FORM_LABEL);
    pitem += FPSTR(HTTP_FORM_LIST);
  
    pitem.replace("{i}", id);
    pitem.replace("{n}", id);
    pitem.replace("{p}", label);

    for (int i = 0; i < items.size(); ++i) {
      auto& item = items[i];

      pitem += "<option value=" + String(item.val);
      
      if (i == value)
        pitem += " selected";
      
      pitem += ">" + item.name + "</option>";
    }

    pitem += "</select>";

    page += pitem;
}

static void addList(String& page, String id, String label, int value, const std::vector<String>& items)
{
    String pitem;
    pitem = FPSTR(HTTP_FORM_LABEL);
    pitem += FPSTR(HTTP_FORM_LIST);
  
    pitem.replace("{i}", id);
    pitem.replace("{n}", id);
    pitem.replace("{p}", label);
    pitem.replace("{v}", String(value));

    for (int i = 0; i < items.size(); ++i) {
      pitem += "<option value=" + String(i);
      if (i == value)
        pitem += " selected";
        
      pitem += ">" + items[i] + "</option>";
    }

    pitem += "</select>";

    page += pitem;
}

void validateInput(String input, String& output)
{
  input.trim();
  input.replace(' ', '_');
  output = input;
}

bool g_flashChanged = false;

// Map Holding all possible Configuration Parameters
// First item is the internal name of the Parameter
// Second a function to be called to generete html for the parameter
// Third a function to be called when the parameter is changed to save the new values

struct paramFunctions {
  std::function<void(String& page)> genHTML;
  std::function<void(const String& arg)> saveValue;
};

typedef std::map<
  String,
  paramFunctions
> paramMap;

paramMap parameters = {
  {
    "name",
    {
      [] (String& page) {
        addParam(page, "name", "Name", String(g_flashConfig.name), 128);
      },
      [] (const String& arg) {
        validateInput(arg, g_flashConfig.name);
        g_flashChanged = true;
      }
    }
  },
  {
    "interval",
    {
      [] (String& page) {
        addParam(page, "interval", "Update Interval", String(g_flashConfig.interval), 128);
      },
      [] (const String& arg) {
        g_flashConfig.interval = arg.toInt();
        g_flashChanged = true;
      }
    }
  },
  {
    "api",
    {
      [] (String& page) {
        addList(page, "api", "API", g_flashConfig.api, RemoteAPILabels);
      },
      [] (const String& arg) {
        g_flashConfig.api = arg.toInt();
        g_flashChanged = true;
      }
    }
  },
  {
    "token",
    {
      [] (String& page) {
        addParam(page, "token", "Token", g_flashConfig.token, 128);
      },
      [] (const String& arg) {
        validateInput(arg, g_flashConfig.token);
        g_flashChanged = true;
      }
    }
  },
  {
    "address",
    {
      [] (String& page) {
        addParam(page, "address", "Server Address", g_flashConfig.server, 128);
      },
      [] (const String& arg) {
        validateInput(arg, g_flashConfig.server);
        g_flashChanged = true;
      }
    }
  },
   {
    "address_default",
    {
      [] (String& page) {
        addParam(page, "address_default", "Default Address", g_flashConfig.default_ip, 128);
      },
      [] (const String& arg) {
        validateInput(arg, g_flashConfig.default_ip);
        g_flashChanged = true;
      }
    }
  },
  {
    "port",
    {
      [] (String& page) {
        addParam(page, "port", "Port Number", String(g_flashConfig.port), 128);
      },
      [] (const String& arg) {
        g_flashConfig.port = arg.toInt();
        g_flashChanged = true;
      }
    }
  },
  {
    "url",
    {
      [] (String& page) {
        addParam(page, "url", "URL", g_flashConfig.url, 128);
      },
      [] (const String& arg) {
        validateInput(arg, g_flashConfig.url);
        g_flashChanged = true;
      }
    }
  },
  {
    "db",
    {
      [] (String& page) {
        addParam(page, "db", "Influx DB", g_flashConfig.db, 128);
      },
      [] (const String& arg) {
        validateInput(arg, g_flashConfig.db);
        g_flashChanged = true;
      }
    }
  },
  {
    "username",
    {
      [] (String& page) {
        addParam(page, "username", "Username", g_flashConfig.username, 128);
      },
      [] (const String& arg) {
        validateInput(arg, g_flashConfig.username);
        g_flashChanged = true;
      }
    }
  },
  {
    "password",
    {
      [] (String& page) {
        addParam(page, "password", "Password", g_flashConfig.password, 128);
      },
      [] (const String& arg) {
        validateInput(arg, g_flashConfig.password);
        g_flashChanged = true;
      }
    }
  },
  {
    "job",
    {
      [] (String& page) {
        addParam(page, "job", "Prometheus Job", g_flashConfig.job, 128);
      },
      [] (const String& arg) {
        validateInput(arg, g_flashConfig.job);
        g_flashChanged = true;
      }
    }
  },
  {
    "instance",
    {
      [] (String& page) {
        addParam(page, "instance", "Prometheus Instance", g_flashConfig.instance, 128);
      },
      [] (const String& arg) {
        validateInput(arg, g_flashConfig.instance);
        g_flashChanged = true;
      }
    }
  },
  {
    "tempscale",
    {
      [] (String& page) {
        addList(page, "tempscale", "Temperature Unit", g_flashConfig.tempscale, TempLabels);
      },
      [] (const String& arg) {
        g_flashConfig.tempscale = (TempUnits) arg.toInt();
        g_flashChanged = true;
      }
    }
  },
  {
    "setpoint_carb",
    {
      [] (String& page) {
        addParam(page, "setpoint_carb", "Target Carbonation [g/l]", String(p_Controller_->setpoint_carbondioxide), 12);
      },
      [] (const String& arg) {
        p_Controller_->setpoint_carbondioxide = arg.toDouble();
      }
    }
  },
  {
    "setpoint",
    {
      [] (String& page) {
        addParam(page, "setpoint", "Target Pressure [bar]", String(p_Controller_->Setpoint), 12);
      },
      [] (const String& arg) {
        p_Controller_->Setpoint = arg.toFloat();
      }
    }
  },
  {
    "controller_p_value",
    {
      [] (String& page) {
        addParam(page, "controller_p_value", "Controller P Value", String(p_Controller_->Kp), 12);
      },
      [] (const String& arg) {
        p_Controller_->Kp = arg.toFloat();
      }
    }
  },
  {
    "opening_time",
    {
      [] (String& page) {
        addParam(page, "opening_time", "Mininum valve open time [ms]", String(p_Controller_->min_open_time), 12);
      },
      [] (const String& arg) {
        p_Controller_->min_open_time = arg.toFloat();
      }
    }
  },
  {
    "dead_zone",
    {
      [] (String& page) {
        addParam(page, "dead_zone", "Dead Zone [bar]", String(p_Controller_->dead_zone), 12);
      },
      [] (const String& arg) {
        p_Controller_->dead_zone = arg.toFloat();
      }
    }
  },
  {
    "cycle_time",
    {
      [] (String& page) {
        addParam(page, "cycle_time", "Cycle Time [ms]", String(p_Controller_->cycle_time), 12);
      },
      [] (const String& arg) {
        p_Controller_->cycle_time = arg.toInt();
      }
    }
  },
  {
    "pressure_source",
    {
      [] (String& page) {
          addList(page, "pressure_source", "Pressure Source", p_Controller_->compressed_gas_bottle, {
            {0, "Fermentation"},
            {1, "CO2 Gas Bottle"}
          });
      },
      [] (const String& arg) {
        p_Controller_->compressed_gas_bottle = arg.toInt();
      }
    }
  },
  {
    "emanometer_mode",
    {
      [] (String& page) {
        addList(page, "emanometer_mode", "eManometer Mode", (int) g_flashConfig.mode, {
          {0, "Bottle Gauge"},
          {1, "Spunding Valve"}
        });
      },
      [] (const String& arg) {
        g_flashConfig.mode = (eManometerMode) arg.toInt();
        g_flashChanged = true;
      }
    }
  },
  {
    "display",
    {
      [] (String& page) {
        addList(page, "display", "Display Type", (int) g_flashConfig.display, {
          {0, "None"},
          {1, "SSD1306"},
          {2, "SH1106"}
        });
      },
      [] (const String& arg) {
        g_flashConfig.display = (DisplayType) arg.toInt();
        g_flashChanged = true;
      }
    }
  },
  {
    "led_value_red",
    {
      [] (String& page) {
        addParam(page, "led_value_red", "Red LED Threshold [> bar]", String(p_Basic_config_->value_red), 12);
      },
      [] (const String& arg) {
        p_Basic_config_->value_red = arg.toFloat();
      }
    }
  },
  {
    "led_value_blue",
    {
      [] (String& page) {
        addParam(page, "led_value_blue", "Blue LED Threshold [< % of carbonation]", String(p_Basic_config_->value_blue), 12);
      },
      [] (const String& arg) {
        p_Basic_config_->value_blue = arg.toFloat();
      }
    }
  },
  {
    "led_value_turkis",
    {
      [] (String& page) {
        addParam(page, "led_value_turkis", "Turkis LED Threshold [< % of carbonation]", String(p_Basic_config_->value_turkis), 12);
      },
      [] (const String& arg) {
        p_Basic_config_->value_turkis = arg.toFloat();
      }
    }
  },
  {
    "led_value_green",
    {
      [] (String& page) {
        addParam(page, "led_value_green", "Green LED Threshold [< % of carbonation]", String(p_Basic_config_->value_green), 12);
      },
      [] (const String& arg) {
        p_Basic_config_->value_green = arg.toFloat();
      }
    }
  },
  {
    "einmaischen",
    {
      [] (String& page) {
        addParam(page, "einmaischen", "Temperatur einmaischen [°C]", String(p_recipe->temperatur_mash), 12);
      },
      [] (const String& arg) {
        p_recipe->temperatur_mash = arg.toFloat();
      }
    }
  },
  {
    "Rast_1",
    {
      [] (String& page) {
        addParam(page, "Rast_1", "1. Rast [°C]", String(p_recipe->rast_temp[0]), 12);
      },
      [] (const String& arg) {
        p_recipe->rast_temp[0] = arg.toFloat();
      }
    }
  },
  {
    "Rast_2",
    {
      [] (String& page) {
        addParam(page, "Rast_2", "2. Rast [°C]", String(p_recipe->rast_temp[1]), 12);
      },
      [] (const String& arg) {
        p_recipe->rast_temp[1] = arg.toFloat();
      }
    }
  },
  {
    "Rast_3",
    {
      [] (String& page) {
        addParam(page, "Rast_3", "3. Rast [°C]", String(p_recipe->rast_temp[2]), 12);
      },
      [] (const String& arg) {
        p_recipe->rast_temp[2] = arg.toFloat();
      }
    }
  },
  {
    "Rast_4",
    {
      [] (String& page) {
        addParam(page, "Rast_4", "4. Rast [°C]", String(p_recipe->rast_temp[3]), 12);
      },
      [] (const String& arg) {
        p_recipe->rast_temp[2] = arg.toFloat();
      }
    }
  },
  {
    "Rast_5",
    {
      [] (String& page) {
        addParam(page, "Rast_54", "5. Rast [°C]", String(p_recipe->rast_temp[4]), 12);
      },
      [] (const String& arg) {
        p_recipe->rast_temp[2] = arg.toFloat();
      }
    }
  },
  {
    "Rast_6",
    {
      [] (String& page) {
        addParam(page, "Rast_6", "6. Rast [°C]", String(p_recipe->rast_temp[5]), 12);
      },
      [] (const String& arg) {
        p_recipe->rast_temp[2] = arg.toFloat();
      }
    }
  },
  {
    "Rast_7",
    {
      [] (String& page) {
        addParam(page, "Rast_7", "7. Rast [°C]", String(p_recipe->rast_temp[6]), 12);
      },
      [] (const String& arg) {
        p_recipe->rast_temp[2] = arg.toFloat();
      }
    }
  },
};

Webserver::Webserver()
{
  //Do a network scan before setting up an access point so as not to close WiFiNetwork while scanning.
  numberOfNetworks = scanWifiNetworks(networkIndicesptr);

  if (WiFi.getAutoConnect() == 0)
    WiFi.setAutoConnect(1);
}
Webserver::~Webserver()
{
  free(networkIndices); //indices array no longer required so free memory
}

void Webserver::header()
{
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");
}

boolean Webserver::startWebserver()
{
  server.reset(new ESP8266WebServer(80));

  /* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
  server->on("/", std::bind(&Webserver::handleRoot, this));
  server->on("/wifi", std::bind(&Webserver::handleWifi, this));
  server->on("/hw", std::bind(&Webserver::handleHWConfig, this));
  server->on("/api", std::bind(&Webserver::handleAPIConfig, this));
  server->on("/config", std::bind(&Webserver::handleConfig, this));
  server->on("/cs", std::bind(&Webserver::handleConfigSave, this));
  server->on("/wifisave", std::bind(&Webserver::handleWifiSave, this));
  server->on("/i", std::bind(&Webserver::handleInfo, this));
  server->on("/info", std::bind(&Webserver::handleiSpindel, this));
  server->on("/ajax", std::bind(&Webserver::handleAjaxRefresh, this));
  server->on("/state", std::bind(&Webserver::handleState, this));
  server->on("/scan", std::bind(&Webserver::handleScan, this));
  server->on("/mnt", std::bind(&Webserver::handleMnt, this));
  server->on("/zerocal", std::bind(&Webserver::handleZeroCal, this));
  server->on("/reset", std::bind(&Webserver::handleReset, this));
  server->on("/update", HTTP_POST, std::bind(&Webserver::handleUpdateDone, this), std::bind(&Webserver::handleUpdating, this));
  server->onNotFound(std::bind(&Webserver::handleNotFound, this));
  DEBUG_WM(F("HTTP server started"));

  server->begin();
}

void Webserver::startWifiManager()
{
  int connRes = WiFi.waitForConnectResult();
  
  if (connRes == WL_CONNECTED)
  {
    WiFi.mode(WIFI_AP_STA); //Dual mode works fine if it is connected to WiFi
    DEBUG_WM("SET AP STA");
  }
  else
  {
    WiFi.mode(WIFI_AP); // Dual mode becomes flaky if not connected to a WiFi network.
    // When ESP8266 station is trying to find a target AP, it will scan on every channel,
    // that means ESP8266 station is changing its channel to scan. This makes the channel of ESP8266 softAP keep changing too..
    // So the connection may break. From http://bbs.espressif.com/viewtopic.php?t=671#p2531
    DEBUG_WM("SET AP");
  }

  dnsServer.reset(new DNSServer());
  WiFi.softAP(_apName);

  delay(500);
  dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
}

void Webserver::process()
{
  if (dnsServer) { dnsServer->processNextRequest(); }
  if (server) { server->handleClient(); }
}

void Webserver::handleUpdating()
{
  // handler for the file upload, get's the sketch bytes, and writes
  // them through the Update object


  HTTPUpload &upload = server->upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    Serial.setDebugOutput(true);

    WiFiUDP::stopAll();
    Serial.printf("Update: %s\n", upload.filename.c_str());
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace))
    { //start with max available size
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    Serial.printf(".");
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
    {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (Update.end(true))
    { //true to set the size to the current progress
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
    }
    else
    {
      Update.printError(Serial);
    }
    Serial.setDebugOutput(false);
  }
  else if (upload.status == UPLOAD_FILE_ABORTED)
  {
    Update.end();
    CONSOLELN("Update was aborted");
  }
  delay(0);
}

void Webserver::handleUpdateDone()
{
  DEBUG_WM(F("Handle update done"));
  if (captivePortal())
  { // If caprive portal redirect instead of displaying the page.
    return;
  }
  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "Options");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += F("<META HTTP-EQUIV=\"refresh\" CONTENT=\"15;url=/\">");
  page += FPSTR(HTTP_HEAD_END);
  page += "<h1>";
  page += _apName;
  page += "</h1>";
  if (Update.hasError())
  {
    page += FPSTR(HTTP_UPDATE_FAI);
    CONSOLELN("update failed");
  }
  else
  {
    page += FPSTR(HTTP_UPDATE_SUC);
    CONSOLELN("update done");
  }
  page += F("<br>Will be redirected to portal page in 15 seconds");
  page += FPSTR(HTTP_END);
  server->send(200, "text/html", page);
  delay(1000); // send page
  ESP.restart();
}


String Webserver::getConfigPortalSSID()
{
  return _apName;
}

void Webserver::resetSettings()
{
  DEBUG_WM(F("previous settings invalidated"));
  WiFi.disconnect(true);
  //trying to fix connection in progress hanging
  ETS_UART_INTR_DISABLE();
  wifi_station_disconnect();
  ETS_UART_INTR_ENABLE();
  delay(200);
  return;
}

void Webserver::setTimeout(unsigned long seconds)
{
  setConfigPortalTimeout(seconds);
}

void Webserver::setConfigPortalTimeout(unsigned long seconds)
{
  _configPortalTimeout = seconds * 1000;
}

void Webserver::setConnectTimeout(unsigned long seconds)
{
  _connectTimeout = seconds * 1000;
}

void Webserver::setDebugOutput(boolean debug)
{
  _debug = debug;
}

void Webserver::setAPStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn)
{
  _ap_static_ip = ip;
  _ap_static_gw = gw;
  _ap_static_sn = sn;
}

void Webserver::setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn)
{
  _sta_static_ip = ip;
  _sta_static_gw = gw;
  _sta_static_sn = sn;
}

void Webserver::setMinimumSignalQuality(int quality)
{
  _minimumQuality = quality;
}

void Webserver::reportStatus(String &page)
{
  if (WiFi.SSID() != "")
  {
    page += F("Configured to connect to access point ");
    page += WiFi.SSID();
    if (WiFi.status() == WL_CONNECTED)
    {
      page += F(" and <strong>currently connected</strong> on IP <a href=\"http://");
      page += WiFi.localIP().toString();
      page += F("/\">");
      page += WiFi.localIP().toString();
      page += F("</a>");
    }
    else
    {
      page += F(" but <strong>not currently connected</strong> to network.");
    }
  }
  else
  {
    page += F("No network currently configured.");
  }
}

/** Handle root or redirect to captive portal */
void Webserver::handleRoot()
{
  DEBUG_WM(F("Handle root"));
  if (captivePortal())
  { // If caprive portal redirect instead of displaying the error page.
    return;
  }
  header();
  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "Options");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += "<h2>";
  page += _apName;
  if (WiFi.SSID() != "")
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      page += " on ";
      page += WiFi.SSID();
    }
    else
    {
      page += " <s>on ";
      page += WiFi.SSID();
      page += "</s>";
    }
  }
  page += "</h2>";
  page += FPSTR(HTTP_PORTAL_OPTIONS);
  page += F("<div class=\"msg\">");
  reportStatus(page);
  page += F("</div>");
  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);
}

/** Wifi config page handler */
void Webserver::handleWifi()
{
  header();
  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "Config ESP");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h2>WiFi Config</h2>");
  //Print list of WiFi networks that were found in earlier scan
  if (numberOfNetworks == 0)
  {
    page += F("WiFi scan found no networks. Restart configuration portal to scan again.");
  }
  else
  {
    //display networks in page
    for (int i = 0; i < numberOfNetworks; i++)
    {
      if (networkIndices[i] == -1)
        continue; // skip dups and those that are below the required quality

      DEBUG_WM(WiFi.SSID(networkIndices[i]));
      DEBUG_WM(WiFi.RSSI(networkIndices[i]));
      int quality = getRSSIasQuality(WiFi.RSSI(networkIndices[i]));

      String item = FPSTR(HTTP_ITEM);
      String rssiQ;
      rssiQ += quality;
      item.replace("{v}", WiFi.SSID(networkIndices[i]));
      item.replace("{r}", rssiQ);
      if (WiFi.encryptionType(networkIndices[i]) != ENC_TYPE_NONE)
      {
        item.replace("{i}", "l");
      }
      else
      {
        item.replace("{i}", "");
      }
      //DEBUG_WM(item);
      page += item;
      delay(0);
    }
    page += "<br/>";
  }

  // page += FPSTR(HTTP_FORM_START);
  page += FPSTR(HTTP_FORM_START1);
  page += _ssid;
  page += FPSTR(HTTP_FORM_START2);
  page += _pass;
  page += FPSTR(HTTP_FORM_START3);

  if (_sta_static_ip)
  {

    String item = FPSTR(HTTP_FORM_PARAM);
    item.replace("{i}", "ip");
    item.replace("{n}", "ip");
    item.replace("{p}", "Static IP");
    item.replace("{l}", "15");
    item.replace("{v}", _sta_static_ip.toString());

    page += item;

    item = FPSTR(HTTP_FORM_PARAM);
    item.replace("{i}", "gw");
    item.replace("{n}", "gw");
    item.replace("{p}", "Static Gateway");
    item.replace("{l}", "15");
    item.replace("{v}", _sta_static_gw.toString());

    page += item;

    item = FPSTR(HTTP_FORM_PARAM);
    item.replace("{i}", "sn");
    item.replace("{n}", "sn");
    item.replace("{p}", "Subnet");
    item.replace("{l}", "15");
    item.replace("{v}", _sta_static_sn.toString());

    page += item;

    page += "<br/>";
  }

  page += FPSTR(HTTP_FORM_END);
  page += FPSTR(HTTP_BACK_BTN);

  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent config page"));
}

void Webserver::handleHWConfig()
{
  header();
  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "Hardware Config");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h2>Hardware Config</h2>");

  //genConfigPage(page, {"emanometer_mode", "display"});
  genConfigPage(page, { 
    "Rast_1", 
    "Rast_2", 
    "Rast_3", 
    "Rast_4", 
    "Rast_5", 
    "port", 
    "url", 
    "db", 
    "username", 
    "password", 
    "job", 
    "instance" 
  });

  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent config page"));
}

void Webserver::genConfigPage(String& page, const std::list<String>& params)
{
  // page += FPSTR(HTTP_FORM_START);
  page += F("<form method=\"get\" action=\"cs\">");

  for (const String& param: params) {
    parameters[param].genHTML(page);
  }

  page += FPSTR(HTTP_FORM_END);
  page += FPSTR(HTTP_BACK_BTN);

}


void Webserver::handleAPIConfig()
{
  header();
  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "Remote Sender Config");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(remoteApiJs);
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h2>Remote Sender Config</h2>");

  genConfigPage(page, { 
    "name", 
    "interval", 
    "api", 
    "token", 
    "address", 
    "address_default",
    "port", 
    "url", 
    "db", 
    "username", 
    "password", 
    "job", 
    "instance" 
  });

  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent config page"));
}

/** Wifi config page handler */
void Webserver::handleConfig()
{
  header();
  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "WLAN-Gateway Config");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h2>WLAN-Gateway Config</h2>");

  std::list<String> configItems = {
    "tempscale", 
    "setpoint_carb", 
    "setpoint",
    "led_value_red",
    "led_value_blue",
    "led_value_turkis",
    "led_value_green"
  };

  if (g_flashConfig.mode == ModeSpundingValve) {
    configItems.insert(configItems.end(), {
      "controller_p_value", 
      "opening_time", 
      "dead_zone", 
      "cycle_time", 
      "pressure_source"
    });
  }

  genConfigPage(page, configItems);

  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent config page"));
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void Webserver::handleWifiSave()
{
  DEBUG_WM(F("WiFi save"));

  //SAVE/connect here
  _ssid = server->arg("s").c_str();
  _pass = server->arg("p").c_str();

  if (server->arg("ip") != "")
  {
    DEBUG_WM(F("static ip"));
    DEBUG_WM(server->arg("ip"));
    //_sta_static_ip.fromString(server->arg("ip"));
    String ip = server->arg("ip");
    optionalIPFromString(&_sta_static_ip, ip.c_str());
  }
  if (server->arg("gw") != "")
  {
    DEBUG_WM(F("static gateway"));
    DEBUG_WM(server->arg("gw"));
    String gw = server->arg("gw");
    optionalIPFromString(&_sta_static_gw, gw.c_str());
  }
  if (server->arg("sn") != "")
  {
    DEBUG_WM(F("static netmask"));
    DEBUG_WM(server->arg("sn"));
    String sn = server->arg("sn");
    optionalIPFromString(&_sta_static_sn, sn.c_str());
  }

  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "Credentials Saved");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(HTTP_SAVED);
  page.replace("{v}", _apName);
  page.replace("{x}", _ssid);
  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  if (connectWifi(_ssid, _pass) != WL_CONNECTED) {
    DEBUG_WM(F("Failed to connect."));
    WiFi.mode(WIFI_AP); // Dual mode becomes flaky if not connected to a WiFi network.
    // I think this might be because too much of the processor is being utilised
    //trying to connect to the network.
  }
  else {
    saveConfig();
  }
}

//Convenient for debugging but wasteful of program space.
//Remove if short of space
char *Webserver::getStatus(int status)
{
  switch (status)
  {
  case WL_IDLE_STATUS:
    return "WL_IDLE_STATUS";
  case WL_NO_SSID_AVAIL:
    return "WL_NO_SSID_AVAIL";
  case WL_CONNECTED:
    return "WL_CONNECTED";
  case WL_CONNECT_FAILED:
    return "WL_CONNECT_FAILED";
  case WL_DISCONNECTED:
    return "WL_DISCONNECTED";
  default:
    return "UNKNOWN";
  }
}

int Webserver::connectWifi(String ssid, String pass)
{
  DEBUG_WM(F("Connecting wifi with new parameters..."));
  if (ssid != "")
  {
    resetSettings(); /*Disconnect from the network and wipe out old values
	if no values were entered into form. If you don't do this
    esp8266 will sometimes lock up when SSID or password is different to
	the already stored values and device is in the process of trying to connect
	to the network. Mostly it doesn't but occasionally it does.
	*/
    // check if we've got static_ip settings, if we do, use those.
    if (_sta_static_ip)
    {
      DEBUG_WM(F("Custom STA IP/GW/Subnet"));
      WiFi.config(_sta_static_ip, _sta_static_gw, _sta_static_sn);
      DEBUG_WM(WiFi.localIP());
    }

    WiFi.mode(WIFI_STA);                    //It will start in station mode if it was previously in AP mode.
    WiFi.begin(ssid.c_str(), pass.c_str()); // Start Wifi with new values.
  }
  else if (!WiFi.SSID())
  {
    DEBUG_WM(F("No saved credentials"));
  }

  int connRes = waitForConnectResult();
  DEBUG_WM("Connection result: ");
  DEBUG_WM(getStatus(connRes));
  //not connected, WPS enabled, no pass - first attempt
  return connRes;
}

uint8_t Webserver::waitForConnectResult()
{
  if (_connectTimeout == 0)
  {
    unsigned long startedAt = millis();
    DEBUG_WM(F("After waiting..."));
    int connRes = WiFi.waitForConnectResult();
    float waited = (millis() - startedAt);
    DEBUG_WM(waited / 1000);
    DEBUG_WM(F("seconds"));
    return connRes;
  }
  else
  {
    DEBUG_WM(F("Waiting for connection result with time out"));
    unsigned long start = millis();
    boolean keepConnecting = true;
    uint8_t status;
    while (keepConnecting)
    {
      status = WiFi.status();
      if (millis() > start + _connectTimeout)
      {
        keepConnecting = false;
        DEBUG_WM(F("Connection timed out"));
      }
      if (status == WL_CONNECTED || status == WL_CONNECT_FAILED)
      {
        keepConnecting = false;
      }
      delay(100);
    }
    return status;
  }
}

void Webserver::handleConfigSave()
{
  for (uint8_t i = 0; i < server->args(); i++) {
    auto it = parameters.find(server->argName(i));
    if (it != parameters.end()) {
      it->second.saveValue(server->arg(i));
    }
  }

  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "Config Saved");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += F("<META HTTP-EQUIV=\"refresh\" CONTENT=\"10;url=/\">");
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(HTTP_SAVED2);
  page += FPSTR(HTTP_END);

  if (g_flashChanged) {
    saveConfig();
    g_flashChanged = false;
  }

  FRAM.write_basic_config(p_Basic_config_, basic_config_offset);
  FRAM.write_controller_parameters(p_Controller_, Controller_paramter_offset);

  server->send(200, "text/html", page);

  delay(1000);
  ESP.restart();

}

/** Handle the info page */
void Webserver::handleInfo()
{
  DEBUG_WM(F("Info"));
  header();
  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "Info");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h2>WiFi Information</h2>");
  // page += F("Android app from <a href=\"https://play.google.com/store/apps/details?id=au.com.umranium.espconnect\">https://play.google.com/store/apps/details?id=au.com.umranium.espconnect</a> provides easier ESP WiFi configuration.<p/>");
  reportStatus(page);
  page += F("<h3>Device Data</h3>");
  page += F("<table class=\"table\">");
  page += F("<thead><tr><th>Name</th><th>Value</th></tr></thead><tbody><tr><td>Chip ID</td><td>");
  page += ESP.getChipId();
  page += F("</td></tr>");
  page += F("<tr><td>Flash Chip ID</td><td>");
  page += ESP.getFlashChipId();
  page += F("</td></tr>");
  page += F("<tr><td>SDK Version</td><td>");
  page += ESP.getSdkVersion();
  page += F("</td></tr>");
  page += F("<tr><td>Free HEAP</td><td>");
  page += ESP.getFreeHeap();
  page += F("</td></tr>");
  page += F("<tr><td>IDE Flash Size</td><td>");
  page += ESP.getFlashChipSize();
  page += F(" bytes</td></tr>");
  page += F("<tr><td>Real Flash Size</td><td>");
  page += ESP.getFlashChipRealSize();
  page += F(" bytes</td></tr>");
  page += F("<tr><td>Access Point IP</td><td>");
  page += WiFi.softAPIP().toString();
  page += F("</td></tr>");
  page += F("<tr><td>Access Point MAC</td><td>");
  page += WiFi.softAPmacAddress();
  page += F("</td></tr>");

  page += F("<tr><td>SSID</td><td>");
  page += WiFi.SSID();
  page += F("</td></tr>");

  page += F("<tr><td>Station IP</td><td>");
  page += WiFi.localIP().toString();
  page += F("</td></tr>");

  page += F("<tr><td>Station MAC</td><td>");
  page += WiFi.macAddress();
  page += F("</td></tr>");
  page += F("</tbody></table>");

  page += F("<h3>Available Pages</h3>");
  page += F("<table class=\"table\">");
  page += F("<thead><tr><th>Page</th><th>Function</th></tr></thead><tbody>");
  page += F("<tr><td><a href=\"/\">/</a></td>");
  page += F("<td>Menu page.</td></tr>");
  page += F("<tr><td><a href=\"/wifi\">/wifi</a></td>");
  page += F("<td>Show WiFi scan results and enter WiFi configuration.</td></tr>");
  page += F("<tr><td><a href=\"/wifisave\">/wifisave</a></td>");
  page += F("<td>Save WiFi configuration information and configure device. Needs variables supplied.</td></tr>");
  page += F("<tr><td><a href=\"/close\">/close</a></td>");
  page += F("<td>Close the configuration server and configuration WiFi network.</td></tr>");
  page += F("<tr><td><a href=\"/i\">/i</a></td>");
  page += F("<td>This page.</td></tr>");
  page += F("<tr><td><a href=\"/r\">/r</a></td>");
  page += F("<td>Delete WiFi configuration and reboot. ESP device will not reconnect to a network until new WiFi configuration data is entered.</td></tr>");
  page += F("<tr><td><a href=\"/state\">/state</a></td>");
  page += F("<td>Current device state in JSON format. Interface for programmatic WiFi configuration.</td></tr>");
  page += F("<tr><td><a href=\"/scan\">/scan</a></td>");
  page += F("<td>Run a WiFi scan and return results in JSON format. Interface for programmatic WiFi configuration.</td></tr>");
  page += F("</table>");
  page += F("<p/>");
  page += FPSTR(HTTP_BACK_BTN);
  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent info page"));
}

/** Handle the info page */
void Webserver::handleiSpindel()
{
  DEBUG_WM(F("iSpindel"));

  // we reset the timeout
  _configPortalStart = millis();

  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "Info");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(ajaxRefresh);
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h1>Info</h1><hr>");
  page += F("<h3><table>");
  page += F("<tr><td>Setpoint:</td><td>");
  page += F("<div class=\"info-pressure\" />");;
  page += F("<tr><td>Temperature:</td><td>");
  page += F("<div class=\"info-temperature\" />");
  page += F("<tr><td>gradient:</td><td>");
  page += F("<div class=\"info-co2\" />");

  if (g_flashConfig.mode == ModeSpundingValve) {
    page += F("<tr><td>Time left:</td><td>");
    page += F("<div class=\"info-opening-time\" />");
    page += F("<tr><td>CV:</td><td>");
    page += F("<div class=\"info-num-openings\" />");
  }
  page += F("</td></tr>");
  page += F("</table></h3>");
  page += F("<hr><dl>");
  page += F("<dt><h3>Firmware</h3></dt>");
  page += F("<dd>Version: ");
  page += FIRMWAREVERSION;
  page += F("</dd>");
  page += F("<dd>Date: ");
  page += __DATE__ " " __TIME__;
  page += F("</dl>");
  page += FPSTR(HTTP_BACK_BTN);
  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent iSpindel info page"));
}

void Webserver::handleAjaxRefresh()
{
  DynamicJsonDocument doc(4096);

  char buf[64];

  snprintf(buf, sizeof(buf), "%.2f °C", Temperatur);
  doc["temperature"] = buf;

  snprintf(buf, sizeof(buf), "%.2f °C", Setpoint);
  doc["pressure"] = buf;

  snprintf(buf, sizeof(buf), "%.2f °C/min", gradient);
  doc["co2"] = buf;

  snprintf(buf, sizeof(buf), "%.2f s", p_Statistic_->opening_time / 1000);
  doc["opening-time"] = output;

  doc["num-openings"] = time_left;

  String page;
  serializeJson(doc, page);

  server->send(200, "application/json", page);
}

/** Handle the info page */
void Webserver::handleMnt()
{
  DEBUG_WM(F("Maintenance page"));

  // we reset the timeout
  _configPortalStart = millis();
  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "Maintenance");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h2>Zero Point Calibration</h2><br>Current Zero Point AD Offset: ");
  page += String(p_Basic_config_->zero_value_sensor);
  page += F("<br>Pressure calibration should be done with the sensor under ambient pressure.");
  page += F("<br><form action=\"/zerocal\" method=\"get\"><button class=\"btn\">calibrate</button></form><br/>");
  page += F("<hr><h2>Firmware Update</h2><br>Firmware updates:<br><a href=\"https://github.com/irrwisch1\">github.com/irrwisch1</a><br>");
  page += F("Current Firmware installed:<br><dl>");
  page += F("<dd>Version: ");
  page += FIRMWAREVERSION;
  page += F("</dd>");
  page += F("<dd>Date: ");
  page += __DATE__ " " __TIME__;
  page += F("</dd></dl><br>");
  page += F("<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><br><input type='submit' class=\"btn\" value='update'></form>");
  page += F("<hr><h2>Factory Reset</h2><br>All settings will be removed");
  page += F("<br><form action=\"/reset\" method=\"get\"><button class=\"btn\">factory reset</button></form><br/>");
  page += FPSTR(HTTP_BACK_BTN);
  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent iSpindel info page"));
}

/** Handle the info page */
void Webserver::handleZeroCal()
{
  
}

/** Handle the state page */
void Webserver::handleState()
{
  DEBUG_WM(F("State - json"));
  header();
  String page = F("{\"Soft_AP_IP\":\"");
  page += WiFi.softAPIP().toString();
  page += F("\",\"Soft_AP_MAC\":\"");
  page += WiFi.softAPmacAddress();
  page += F("\",\"Station_IP\":\"");
  page += WiFi.localIP().toString();
  page += F("\",\"Station_MAC\":\"");
  page += WiFi.macAddress();
  page += F("\",");
  if (WiFi.psk() != "")
  {
    page += F("\"Password\":true,");
  }
  else
  {
    page += F("\"Password\":false,");
  }
  page += F("\"SSID\":\"");
  page += WiFi.SSID();
  page += F("\"}");
  server->send(200, "application/json", page);
  DEBUG_WM(F("Sent state page in json format"));
}

/** Handle the scan page */
void Webserver::handleScan()
{
  DEBUG_WM(F("State - json"));
  header();

  int n;
  int *indices;
  int **indicesptr = &indices;
  //Space for indices array allocated on heap in scanWifiNetworks
  //and should be freed when indices no longer required.
  n = scanWifiNetworks(indicesptr);
  DEBUG_WM(F("In handleScan, scanWifiNetworks done"));
  String page = F("{\"Access_Points\":[");
  //display networks in page
  for (int i = 0; i < n; i++)
  {
    if (indices[i] == -1)
      continue; // skip duplicates and those that are below the required quality
    if (i != 0)
      page += F(", ");
    DEBUG_WM(WiFi.SSID(indices[i]));
    DEBUG_WM(WiFi.RSSI(indices[i]));
    int quality = getRSSIasQuality(WiFi.RSSI(indices[i]));
    String item = FPSTR(JSON_ITEM);
    String rssiQ;
    rssiQ += quality;
    item.replace("{v}", WiFi.SSID(indices[i]));
    item.replace("{r}", rssiQ);
    if (WiFi.encryptionType(indices[i]) != ENC_TYPE_NONE)
    {
      item.replace("{i}", "true");
    }
    else
    {
      item.replace("{i}", "false");
    }
    //DEBUG_WM(item);
    page += item;
    delay(0);
  }
  free(indices); //indices array no longer required so free memory
  page += F("]}");
  server->send(200, "application/json", page);
  DEBUG_WM(F("Sent WiFi scan data ordered by signal strength in json format"));
}

/** Handle the reset page */
void Webserver::handleReset()
{
  DEBUG_WM(F("Reset"));
  header();
  String page = FPSTR(HTTP_HEAD_HTML);
  page.replace("{v}", "WiFi Information");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("Module will reset in a few seconds.");
  page += FPSTR(HTTP_END);
  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent reset page"));
  delay(1000);
  WiFi.disconnect(true); // Wipe out WiFi credentials.
  resetSettings();
  formatSpiffs();
  FRAM.reset_settings();
  ESP.reset();
  delay(2000);
}

void Webserver::handleNotFound()
{
  if (captivePortal())
  { // If caprive portal redirect instead of displaying the error page.
    return;
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";

  for (uint8_t i = 0; i < server->args(); i++)
  {
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  header();
  server->send(404, "text/plain", message);
}

/** Redirect to captive portal if we got a request for another domain. Return true in
that case so the page handler do not try to handle the request again. */
boolean Webserver::captivePortal()
{
  if (!isIp(server->hostHeader()) && server->hostHeader() != (String(myHostname)))
  {
    DEBUG_WM(F("Request redirected to captive portal"));
    server->sendHeader("Location", ("http://") + String(myHostname), true);
    server->setContentLength(0);
    server->send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
                                         //    server->client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

//sets a custom element to add to head, like a new style tag
void Webserver::setCustomHeadElement(const char *element)
{
  _customHeadElement = element;
}

//if this is true, remove duplicated Access Points - defaut true
void Webserver::setRemoveDuplicateAPs(boolean removeDuplicates)
{
  _removeDuplicateAPs = removeDuplicates;
}

//Scan for WiFiNetworks in range and sort by signal strength
//space for indices array allocated on the heap and should be freed when no longer required
int Webserver::scanWifiNetworks(int **indicesptr)
{
  int n = WiFi.scanNetworks();
  DEBUG_WM(F("Scan done"));
  if (n == 0)
  {
    DEBUG_WM(F("No networks found"));
    return (0);
  }
  else
  {
    // Allocate space off the heap for indices array.
    // This space should be freed when no longer required.
    int *indices = (int *)malloc(n * sizeof(int));
    if (indices == NULL)
    {
      DEBUG_WM(F("ERROR: Out of memory"));
      return (0);
    }
    *indicesptr = indices;
    //sort networks
    for (int i = 0; i < n; i++)
    {
      indices[i] = i;
    }

    std::sort(indices, indices + n, [](const int &a, const int &b) -> bool {
      return WiFi.RSSI(a) > WiFi.RSSI(b);
    });
    // remove duplicates ( must be RSSI sorted )
    if (_removeDuplicateAPs)
    {
      String cssid;
      for (int i = 0; i < n; i++)
      {
        if (indices[i] == -1)
          continue;
        cssid = WiFi.SSID(indices[i]);
        for (int j = i + 1; j < n; j++)
        {
          if (cssid == WiFi.SSID(indices[j]))
          {
            DEBUG_WM("DUP AP: " + WiFi.SSID(indices[j]));
            indices[j] = -1; // set dup aps to index -1
          }
        }
      }
    }

    for (int i = 0; i < n; i++)
    {
      if (indices[i] == -1)
        continue; // skip dups

      int quality = getRSSIasQuality(WiFi.RSSI(indices[i]));
      if (!(_minimumQuality == -1 || _minimumQuality < quality))
      {
        indices[i] = -1;
        DEBUG_WM(F("Skipping due to quality"));
      }
    }

    return (n);
  }
}

template <typename Generic>
void Webserver::DEBUG_WM(Generic text)
{
  if (_debug)
  {
    CONSOLE("*WM: ");
    CONSOLELN(text);
  }
}

int Webserver::getRSSIasQuality(int RSSI)
{
  int quality = 0;

  if (RSSI <= -100)
  {
    quality = 0;
  }
  else if (RSSI >= -50)
  {
    quality = 100;
  }
  else
  {
    quality = 2 * (RSSI + 100);
  }
  return quality;
}

/** Is this an IP? */
boolean Webserver::isIp(String str)
{
  for (unsigned int i = 0; i < str.length(); i++)
  {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9'))
    {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String Webserver::toStringIp(IPAddress ip)
{
  String res = "";
  for (int i = 0; i < 3; i++)
  {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}
