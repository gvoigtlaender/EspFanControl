/* Copyright 2019 Georg Voigtlaender gvoigtlaender@googlemail.com */
/*
 */
#include <Arduino.h>

char VERSION_STRING[] = "0.0.3.0";
char APPNAME[] = "EspFanControl";
char SHORTNAME[] = "ESPFC";

#include <string>
using std::string;
#include "config.h"
#include <ctime>
#include <iostream>
#include <list>
#include <map>
#include <vector>

string APPNAMEVER = APPNAME + string(" ") + VERSION_STRING;

#pragma region SysLog
#include <CSyslog.h>
CSyslog *m_pSyslog = NULL;
#pragma endregion

#include <CControl.h>

#include <CMqtt.h>
CMqtt *m_pMqtt = NULL;

#include <CWifi.h>
CWifi *m_pWifi = NULL;

#if USE_DISPLAY == 1
#include <CDisplay.h>
CDisplayBase *m_pDisplay = NULL;
#endif

#include <CButton.h>
CButton *m_pButton = NULL;

#include <CNtp.h>
CNtp *m_pNtp = NULL;

#include <CFanControl.h>
CFanControl *m_pFanControl = NULL;

#include <CSensor.h>
CSensorDS18B20 *m_pSensor = NULL;

#include <CBase.h>

#pragma region configuration
#include <CConfiguration.h>
#include <CConfigValue.h>
CConfiguration *m_pConfig = NULL;
CConfigKey<string> *m_pDeviceName = NULL;
#pragma endregion

#pragma region WebServer
#include <CUpdater.h>
#include <CWebserver.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

CWebServer server(80);

#include <CUpdater.h>
CUpdater *m_pUpdater = NULL;
string sStylesCss = "styles.css";
string sJavascriptJs = "javascript.js";

string sHtmlHead =
    "<link rel=\"stylesheet\" type=\"text/css\"  href=\"/" + sStylesCss +
    "\">\n"
    "<script language=\"javascript\" type=\"text/javascript\" src=\"/" +
    sJavascriptJs +
    "\"></script>\n"
    "<meta name=\"viewport\" content=\"width=device-width, "
    "initial-scale=1.0, maximum-scale=1.0, user-scalable=0\">\n"
    "<meta charset='utf-8'><meta name=\"viewport\" "
    "content=\"width=device-width,initial-scale=1,user-scalable=no\"/>\n";

void handleStatusUpdate() {
  // CControl::Log(CControl::I, "handleStatusUpdate, args=%d", server.args());
  // string sUpdate = "";
  // CheckFreeHeap();
  time_t m_RawTime;
  struct tm *m_pTimeInfo;
  time(&m_RawTime);
  m_pTimeInfo = localtime(&m_RawTime);

  // asctime(m_pTimeInfo);
  char mbstr[100];
  std::strftime(mbstr, sizeof(mbstr), "%A %c", m_pTimeInfo);
  // CheckFreeHeap();
  // std::map<string, string> oStates;
  std::vector<std::pair<string, string>> oStates;
  oStates.push_back(std::make_pair(
      "Temperature", std::to_string(m_pSensor->GetTemperatureRaw(0))));
  oStates.push_back(std::make_pair(
      "Fan Mode", m_pFanControl->m_pMqtt_ControlMode->getValue()));
  oStates.push_back(
      std::make_pair("Fan State", m_pFanControl->m_pMqtt_FanState->getValue()));
  oStates.push_back(
      std::make_pair("Scroll Mode", m_pFanControl->m_ValuesAct->m_sName));
  // oStates.push_back(std::make_pair("Heap", std::to_string(g_uiHeap)));
  // oStates.push_back(std::make_pair("Heap Min", std::to_string(g_uiHeapMin)));

  // oStates.push_back(std::make_pair(
  //    "FS Free", std::to_string(LittleFS_GetFreeSpaceKb()) + string("kB")));

  string sMqttState = "OK";
  if (!m_pMqtt->isConnected()) {
    if (m_pMqtt->isRetryConnect())
      sMqttState = "RETRY";
    else
      sMqttState = "FAIL";
  }
  oStates.push_back(std::make_pair("MQTT", sMqttState));

  // CheckFreeHeap();

  string sContent = "";
  sContent += string(mbstr) + string("\n");

  sContent += "<table>\n";
  sContent += "<tr style=\"height:2px\"><th></th><th></th></tr>\n";

  for (unsigned int n = 0; n < oStates.size(); n++) {
    sContent += "<tr><td>" + oStates[n].first + "</td><td>" +
                oStates[n].second + "</td></tr>\n";
  }

  sContent += "</td></tr>\n";
  sContent += "</table>\n";
  // CheckFreeHeap();
  server.send(200, "text/html", sContent.c_str());
  CControl::Log(CControl::D, "handleStatusUpdate done, buffersize=%u",
                sContent.length());
}

void handleTitle() {
  CControl::Log(CControl::I, "handleTitle");
  server.send(200, "text/html", APPNAMEVER.c_str());
}

void handleDeviceName() {
  CControl::Log(CControl::I, "handleDeviceName");
  server.send(200, "text/html", m_pDeviceName->m_pTValue->m_Value.c_str());
}

void handleSwitch() {
  CControl::Log(CControl::I, "handleSwitch, args=%d", server.args());
  server.send(200, "text/html", "");
  if (server.args() > 0) {
    // m_pConfig->handleArgs(&server);
    for (int n = 0; n < server.args(); n++) {
      CControl::Log(CControl::I, "Arg %d: %s = %s", n,
                    server.argName(n).c_str(), server.arg(n).c_str());
    }
    if (server.argName(0) == String("o")) {
      int n = atoi(server.arg(0).c_str());
      if (n >= CFanControl::eAutomatic && n <= CFanControl::eOff) {
        m_pFanControl->SwitchControlMode((CFanControl::E_FANCONTROLMODE)n);
      } else {
        m_pFanControl->OnButtonLongClick();
      }
    }
    if (server.argName(0) == String("rst") && server.arg(0) == String("")) {
      ESP.restart();
    }
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
#pragma endregion

void SetupServer() {
  string sStylesCssUri = "/" + sStylesCss;
  server.serveStatic(sStylesCssUri.c_str(), sStylesCss.c_str());
  string sJavascriptJsUri = "/" + sJavascriptJs;
  server.serveStatic(sJavascriptJsUri.c_str(), sJavascriptJs.c_str());
  server.serveStatic("/mainpage.js", "mainpage.js");
  server.on("/statusupdate.html", handleStatusUpdate);
  server.on("/title", handleTitle);
  server.on("/devicename", handleDeviceName);

  m_pConfig->SetupServer(&server, false);

  server.serveStatic("/", "index.html");
  server.on("/switch", handleSwitch);
  server.onNotFound(handleNotFound);
}

void wifisetupfailed() {
  // Set WiFi to station mode and disconnect from an AP if it was previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  delay(100);

  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");

      std::string ssid = WiFi.SSID(i).c_str();
      m_pWifi->m_pWifiSsid->m_pTValue->m_Choice.push_back(ssid);
      delay(10);
    }
  }

  WiFi.softAP(APPNAME);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  SetupServer();
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("");

  while (true) {
    server.handleClient();
  }

  return;
}

/*
void check_if_exist_I2C() {
  byte error, address;
  int nDevices;
  nDevices = 0;
  for (address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");

      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknow error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  } // for loop
  if (nDevices == 0)
    Serial.println("No I2C devices found");
  else
    Serial.println("**********************************\n");
  // delay(1000);           // wait 1 seconds for next scan, did not find it
  // necessary
}
*/
unsigned int nMillisLast = 0;
void setup(void) {
  nMillisLast = millis();

  Serial.begin(74880);
  CControl::Log(CControl::I, "::setup()");

  Wire.begin();
  nMillisLast = millis();
  check_if_exist_I2C();

#if defined(USE_LITTLEFS)
  // Initialize LittleFS
  if (!LittleFS.begin()) {
    CControl::Log(CControl::E, "An Error has occurred while mounting LittleFS");
    return;
  }
#else
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    CControl::Log(CControl::E, "An Error has occurred while mounting SPIFFS");
    return;
  }
#endif

  Serial.println(ESP.getFreeHeap(), DEC);
  // szhtml_content_buffer = (char *)malloc(szhtml_content_buffer_size);
  // CControl::Log(CControl::D, "szhtml_content_buffer size=%u",
  // szhtml_content_buffer_size);

  m_pConfig = new CConfiguration("/config.json");
  m_pDeviceName = CControl::CreateConfigKey<string>("Device", "Name", SHORTNAME);

#if !defined(WLAN_SSID)
#define  WLAN_SSID ""
#endif
#if !defined(WLAN_PASSWD)
#define WLAN_PASSWD ""
#endif

  m_pWifi = new CWifi(APPNAME, WLAN_SSID, WLAN_PASSWD);
  m_pMqtt = new CMqtt();
  m_pNtp = new CNtp();
  m_pSyslog = new CSyslog(APPNAME, SHORTNAME);
  m_pFanControl = new CFanControl(PIN_FAN);
  m_pSensor = new CSensorDS18B20(PIN_ONEWIRE, nullptr, 20.0, 30.0);
  m_pButton = new CButton(PIN_BTN);
  // m_pLed = new CLed(PIN_LED);

#if defined(USE_DISPLAY)
  m_pDisplay = // new
               // CDisplayU8x8<U8X8_SSD1306_128X32_UNIVISION_HW_I2C>(U8X8_PIN_NONE,
               // 4, 16);
      new CDisplayU8g2<U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C>(U8X8_PIN_NONE);
  m_pWifi->SetDisplayLine(m_pDisplay->AddLine(72, 6, 12, u8g2_font_4x6_tr));
  m_pSensor->SetDisplayLine(0, m_pDisplay->AddLine(0, 8, 8, u8g2_font_5x7_tr));
  // m_pFanControl->SetDisplayLine(m_pDisplay->AddLine(0, 16, 25,
  // u8g2_font_squeezed_r7_tr));
  m_pFanControl->SetDisplayLine(
      m_pDisplay->AddLine(30, 8, 25, u8g2_font_4x6_tr));
#endif

  m_pFanControl->m_pDS18B20 = m_pSensor;
  m_pFanControl->m_pXBM_Temp = m_pDisplay->AddXbm(0, 9, 128, 23);
  m_pConfig->load();
  CControl::Log(CControl::I, "loading config took %ldms",
                millis() - nMillisLast);

  m_pMqtt->setClientName(m_pDeviceName->m_pTValue->m_Value.c_str());
  m_pSyslog->m_pcsDeviceName = m_pDeviceName->m_pTValue->m_Value.c_str();

  nMillisLast = millis();

  if (m_pWifi->m_pWifiSsid->m_pTValue->m_Value.empty() ||
      m_pWifi->m_pWifiPassword->m_pTValue->m_Value.empty()) {
    wifisetupfailed();
  }

  CControl::Log(CControl::I, "creating hardware took %ldms",
                millis() - nMillisLast);
  nMillisLast = millis();

  CControl::Log(CControl::I, "starting server");
  WiFi.setHostname(m_pDeviceName->GetValue().c_str());
  SetupServer();
  nMillisLast = millis();
  CControl::Log(CControl::I, "server started");

  m_pUpdater = new CUpdater(&server, "/update");

  CControl::Log(CControl::I, "setup()");
  if (!CControl::Setup()) {
    // server.on("/", handleConfigure);
    m_pConfig->SetupServer(&server, true);
    server.begin();
    CControl::Log(CControl::I, "HTTP server started");
    CControl::Log(CControl::I, "");
    return;
  }

  std::string sVersion =
      APPNAMEVER + " " + string("Framework: ") + string(FWK_VERSION_STRING);
  m_pDisplay->Line(0, sVersion);

  Serial.println(ESP.getFreeHeap(), DEC);
  CControl::Log(CControl::I, "startup completed");
}

bool bStarted = false;
uint64_t nMillis = millis() + 1000;
void ServerStart() {

  if (WiFi.status() != WL_CONNECTED) {
    nMillis = millis() + 2000;
    return;
  }

  IPAddress oIP = WiFi.localIP();
  String sIP = oIP.toString();

  server.begin();

  CControl::Log(CControl::I, "Connect to http://%s", sIP.c_str());

  bStarted = true;
  nMillis = millis() + 200000;
}

void loop(void) {
  if (bStarted) {
    server.handleClient();
    /*
    if (nMillis < millis()) {
      bStarted = false;
      server.close();
    }
    */
    // httpServer.handleClient();
    // MDNS.update();
  } else {
    if (nMillis < millis()) {
      if (digitalRead(0) == LOW || true) {
        ServerStart();
      }
    }
  }

  CControl::Control();

  switch (m_pButton->getButtonState()) {
  case CButton::eNone:
    break;

  case CButton::ePressed:
    break;

  case CButton::eClick:
    m_pFanControl->OnButtonClick();
    // m_pLed->AddBlinkTask(CLed::BLINK_1);
    m_pButton->setButtonState(CButton::eNone);
    break;

  case CButton::eDoubleClick:
    m_pButton->setButtonState(CButton::eNone);
    break;

  case CButton::eLongClick:
    // m_pFanControl->SwitchControlMode(CFanControl::eAutomatic);
    m_pFanControl->OnButtonLongClick();
    m_pButton->setButtonState(CButton::eNone);
    break;

  case CButton::eVeryLongClick:
    m_pButton->setButtonState(CButton::eNone);
    break;
  }

  CheckFreeHeap();
}
