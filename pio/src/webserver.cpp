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

Webserver::Webserver()
{
  //Do a network scan before setting up an access point so as not to close WiFiNetwork while scanning.
  numberOfNetworks = scanWifiNetworks(networkIndicesptr);
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
  server->on("/close", std::bind(&Webserver::handleServerClose, this));
  server->on("/i", std::bind(&Webserver::handleInfo, this));
  server->on("/info", std::bind(&Webserver::handleiSpindel, this));
  server->on("/ajax", std::bind(&Webserver::handleAjaxRefresh, this));
  server->on("/state", std::bind(&Webserver::handleState, this));
  server->on("/scan", std::bind(&Webserver::handleScan, this));
  server->on("/mnt", std::bind(&Webserver::handleMnt, this));
  //server->on("/offset", std::bind(&WiFiManager::handleOffset, this));
  server->on("/reset", std::bind(&Webserver::handleReset, this));
  server->on("/update", HTTP_POST, std::bind(&Webserver::handleUpdateDone, this), std::bind(&Webserver::handleUpdating, this));
  server->onNotFound(std::bind(&Webserver::handleNotFound, this));
  server->begin(); // Web server start
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
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Options");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
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

void Webserver::setBreakAfterConfig(boolean shouldBreak)
{
  _shouldBreakAfterConfig = shouldBreak;
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
  String page = FPSTR(HTTP_HEAD);
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
  String page = FPSTR(HTTP_HEAD);
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
#if 0
  char parLength[2];
  // add the extra parameters to the form
  for (int i = 0; i < _paramsCount; i++)
  {
    if (_params[i] == NULL)
    {
      break;
    }

    String pitem;
    switch (_params[i]->getLabelPlacement())
    {
    case WFM_LABEL_BEFORE:
      pitem = FPSTR(HTTP_FORM_LABEL);
      pitem += FPSTR(HTTP_FORM_PARAM);
      break;
    case WFM_LABEL_AFTER:
      pitem = FPSTR(HTTP_FORM_PARAM);
      pitem += FPSTR(HTTP_FORM_LABEL);
      break;
    default:
      // WFM_NO_LABEL
      pitem = FPSTR(HTTP_FORM_PARAM);
      break;
    }

    String customHTML = reinterpret_cast<const __FlashStringHelper *>(_params[i]->getCustomHTML());
    if (_params[i]->getID() != NULL)
    {
      pitem.replace("{i}", _params[i]->getID());
      pitem.replace("{n}", _params[i]->getID());
      pitem.replace("{p}", _params[i]->getPlaceholder());
      snprintf(parLength, 2, "%d", _params[i]->getValueLength());
      pitem.replace("{l}", parLength);
      pitem.replace("{v}", _params[i]->getValue());
      pitem.replace("{c}", customHTML);
    }
    else
    {
      pitem = customHTML;
    }

    page += pitem;
  }
  if (_params[0] != NULL)
  {
    page += "<br/>";
  }
#endif
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

  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent config page"));
}

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
    pitem.replace("{v}", String(value));

    for (auto& item: items) {
      pitem += "<option value=" + String(item.val) + ">" + item.name + "</option>";
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
      pitem += "<option value=" + String(i) + ">" + items[i] + "</option>";
    }

    pitem += "</select>";

    page += pitem;
}

void Webserver::handleHWConfig()
{
  header();
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Hardware Config");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h2>Hardware Config</h2>");

  // page += FPSTR(HTTP_FORM_START);
  page += F("<form method=\"get\" action=\"cs\">");

  addList(page, "display", "Display Type", p_Basic_config_->type_of_display, {
    {0, "SH1106"},
    {1, "SSD1306"}
  });

  page += FPSTR(HTTP_FORM_END);
  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent config page"));
}


void Webserver::handleAPIConfig()
{
  header();
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Remote Sender Config");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(remoteApiJs);
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h2>Remote Sender Config</h2>");

  // page += FPSTR(HTTP_FORM_START);
  page += F("<form method=\"get\" action=\"cs\">");

  addParam(page, "name", "Name", g_flashConfig.my_name, sizeof(g_flashConfig.my_name) -1);
  addParam(page, "interval", "Update Interval", String(g_flashConfig.my_sleeptime), 12);
  addList(page, "api", "API", g_flashConfig.my_api, RemoteAPILabels);
  addParam(page, "token", "Token", g_flashConfig.my_token, sizeof(g_flashConfig.my_token)- 1);
  addParam(page, "address", "Server Address", g_flashConfig.my_server, sizeof(g_flashConfig.my_server) -1);
  addParam(page, "port", "Port Number", String(g_flashConfig.my_port), 128);
  addParam(page, "url", "URL", g_flashConfig.my_url, sizeof(g_flashConfig.my_url)-1);
  addParam(page, "db", "Influx DB", g_flashConfig.my_db, sizeof(g_flashConfig.my_db) -1);
  addParam(page, "username", "Username", g_flashConfig.my_username, sizeof(g_flashConfig.my_username) -1);
  addParam(page, "password", "Password", g_flashConfig.my_password, sizeof(g_flashConfig.my_password -1));
  addParam(page, "job", "Prometheus Job", g_flashConfig.my_job, sizeof(g_flashConfig.my_job) -1);
  addParam(page, "instance", "Prometheus Instance", g_flashConfig.my_instance, sizeof(g_flashConfig.my_instance)-1);

  page += FPSTR(HTTP_FORM_END);
  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent config page"));
}

/** Wifi config page handler */
void Webserver::handleConfig()
{
  header();
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "eManometer Config");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h2>eManometer Config</h2>");

  // page += FPSTR(HTTP_FORM_START);
  page += F("<form method=\"get\" action=\"cs\">");

  addList(page, "tempscale", "Temperature Unit", g_flashConfig.my_tempscale, TempLabels);
  addParam(page, "setpoint_carb", "Target Carbonation [g/l]", String(p_Controller_->setpoint_carbondioxide), 12);
  addParam(page, "setpoint", "Target Pressure [bar]", String(p_Controller_->Setpoint), 12);
  addParam(page, "controller_p_value", "Controller P Value", String(p_Controller_->Kp), 12);
  addParam(page, "opening_time", "Mininum valve open time [ms]", String(p_Controller_->min_open_time), 12);
  addParam(page, "dead_zone", "Dead Zone [bar]", String(p_Controller_->dead_zone), 12);
  addParam(page, "cycle_time", "Cycle Time [ms]", String(p_Controller_->cycle_time), 12);
  addList(page, "pressure_source", "Pressure Source", p_Controller_->compressed_gas_bottle, {
    {0, "Fermentation"},
    {1, "CO2 Gas Bottle"}
  });

  page += FPSTR(HTTP_FORM_END);
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

#if 0
  //parameters
  for (int i = 0; i < _paramsCount; i++)
  {
    if (_params[i] == NULL)
    {
      break;
    }
    //read parameter
    String value = server->arg(_params[i]->getID()).c_str();
    //store it in array
    value.toCharArray(_params[i]->_value, _params[i]->_length);
    DEBUG_WM(F("Parameter"));
    DEBUG_WM(_params[i]->getID());
    DEBUG_WM(value);
  }
#endif

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

  String page = FPSTR(HTTP_HEAD);
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

  DEBUG_WM(F("Sent wifi save page"));

  connect = true; //signal ready to connect/reset
}

void Webserver::handleConfigSave()
{

}

/** Handle shut down the server page */
void Webserver::handleServerClose()
{
  DEBUG_WM(F("Server Close"));
  header();
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Close Server");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<div class=\"msg\">");
  page += F("My network is <strong>");
  page += WiFi.SSID();
  page += F("</strong><br>");
  page += F("My IP address is <strong>");
  page += WiFi.localIP().toString();
  page += F("</strong><br><br>");
  page += F("Configuration server closed...<br><br>");
  //page += F("Push button on device to restart configuration server!");
  page += FPSTR(HTTP_END);
  server->send(200, "text/html", page);
  stopConfigPortal = true; //signal ready to shutdown config portal
  DEBUG_WM(F("Sent server close page"));
}
/** Handle the info page */
void Webserver::handleInfo()
{
  DEBUG_WM(F("Info"));
  header();
  String page = FPSTR(HTTP_HEAD);
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

  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Info");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(ajaxRefresh);
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h1>Info</h1><hr>");
  page += F("<h3><table>");
  page += F("<tr><td>Pressure:</td><td>");
  page += F("<div class=\"info-pressure\" />");;
  page += F("<tr><td>Temperature:</td><td>");
  page += F("<div class=\"info-temperature\" />");
  page += F("<tr><td>CO2:</td><td>");
  page += F("<div class=\"info-co2\" />");
  page += F("<tr><td>Valve open time:</td><td>");
  page += F("<div class=\"info-opening-time\" />");
  page += F("<tr><td>Num of openings:</td><td>");
  page += F("<div class=\"info-num-openings\" />");
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
  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent iSpindel info page"));
}

void Webserver::handleAjaxRefresh()
{
  DynamicJsonDocument doc(1024);

  char buf[64];

  snprintf(buf, sizeof(buf), "%.2f °%s", scaleTemperature(Temperatur), tempScaleLabel().c_str());
  doc["temperature"] = buf;

  snprintf(buf, sizeof(buf), "%.2f bar", Pressure);
  doc["pressure"] = buf;

  snprintf(buf, sizeof(buf), "%.2f g/l", carbondioxide);
  doc["co2"] = buf;

  snprintf(buf, sizeof(buf), "%.2f s", p_Statistic_->opening_time / 1000);
  doc["opening-time"] = buf;

  doc["num-openings"] = p_Statistic_->times_open;

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
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Maintenance");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h2>Offset Calibration</h2><br>Before proceeding with calibration make sure the iSpindel is leveled flat, exactly at 0&deg; horizontally and vertically, according to this picture:<br>");
  page += FPSTR(HTTP_ISPINDEL_IMG);
  page += F("<br><form action=\"/offset\" method=\"get\"><button class=\"btn\">calibrate</button></form><br/>");
  page += F("<hr><h2>Firmware Update</h2><br>Firmware updates:<br><a href=\"https://github.com/universam1\">github.com/universam1</a>");
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
  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Sent iSpindel info page"));
}

#if 0
/** Handle the info page */
void WiFiManager::handleOffset()
{
  DEBUG_WM(F("offset"));



  // we reset the timeout
  _configPortalStart = millis();

  header();

  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "calibrate Offset");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;

  
  page += F("<META HTTP-EQUIV=\"refresh\" CONTENT=\"120;url=http://192.168.4.1/\">");
  page += FPSTR(HTTP_HEAD_END);
  page += F("<h1>calibrate Offset</h1><hr>");
  page += F("<table>");
  page += F("<tr><td>");
  page += F("...calibration in progress...<br><h2>DO NOT MOVE OR SHAKE!</h2><br>It takes ~2min to complete. Once complete the iSpindel will restart and the blue LED will switch from continous to blinking");
  // page += offset.getStatus();
  page += F("</td></tr>");
  page += F("</table>");

  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);
  ESP.reset();
}
#endif

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
  String page = FPSTR(HTTP_HEAD);
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

//start up config portal callback
void Webserver::setAPCallback(void (*func)(Webserver *myWiFiManager))
{
  _apcallback = func;
}

//start up save config callback
void Webserver::setSaveConfigCallback(void (*func)(void))
{
  _savecallback = func;
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
