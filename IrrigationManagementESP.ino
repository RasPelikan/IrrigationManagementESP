#include <AsyncPrinter.h>
#include <DebugPrintMacros.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncTCPbuffer.h>
#include <SyncClient.h>
#include <async_config.h>
#include <tcp_axtls.h>

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClient.h>
#include <Adafruit_MCP23X17.h>
#include <time.h>
#include "settings.h"
#include "WString.h"  // https://github.com/esp8266/Arduino/blob/60fe7b4ca8cdca25366af8a7c0a7b70d32c797f8/doc/PROGMEM.rst
#include "LittleFS.h"
#include <ArduinoJson.h>

AsyncWebServer httpRestServer(80);

#define PORT_EXPANDER_ADDR 0 // Adresse 0x20 / 0
Adafruit_MCP23X17 portExpander;

// see https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library
#define GPIO_WATERLEVEL_EMPTY 4 // GPA4
#define GPIO_WATERLEVEL_1 3 // GPA3
#define GPIO_WATERLEVEL_2 2 // GPA2
#define GPIO_WATERLEVEL_3 1 // GPA1
#define GPIO_WATERLEVEL_FULL 0 // GPA0

#define GPIO_VALVE_1 10 // GPB2
#define GPIO_VALVE_2 11 // GPB3
#define GPIO_VALVE_3 12 // GPB4
#define GPIO_VALVE_4 13 // GPB5
#define GPIO_VALVE_5 14 // GPB6
#define GPIO_VALVE_6 15 // GPB7
#define RELAIS_ON LOW
#define RELAIS_OFF HIGH

#define GPIO_WIFI_LED 7 // GPA7
#define WIFI_STATUS_DISCONNECTED 0
#define WIFI_STATUS_CONNECTING 1
#define WIFI_STATUS_WAITING_FOR_NTP 2
#define WIFI_STATUS_NTP_ACTIVE 3

#define MY_NTP_SERVER "at.pool.ntp.org"
#define MY_TZ "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"  // https://remotemonitoringsystems.ca/time-zone-abbreviations.php

#define STATUS_UPDATE_ALL 0
#define STATUS_UPDATE_WELLPUMP 1
#define STATUS_UPDATE_IRRIGATIONPUMP 2
#define STATUS_UPDATE_RSSI 3
#define STATUS_UPDATE_TIME 4
#define STATUS_UPDATE_WATERPRESSURE 5
#define STATUS_UPDATE_WATERLEVEL 6

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

void setup() {

  // Setup console output
  Serial.begin(115200);

  // Setup port expander MCP27013 for I2C address 0x20 
  if (!portExpander.begin_I2C(0x20)) {
    Serial.println(F("Could not initialize I2C port-expander on port 0x20!"));
    while (1);
  }

  //jsonBuffer.par

  // Initialize NTP for current time and Wifi
  setupNtp();
  setupWifi();

  // Initialize water pumps and valves
  setupIrrigationPump();
  setupWellPump();
  setupWaterLevel();
  setupValves();

}

// application status
unsigned long previousTime = 0;
uint8 interval = 0;
uint8 wifiStatus = WIFI_STATUS_DISCONNECTED;
uint8 lastMinute = 0;
time_t now = 0;                     // this are the seconds since Epoch (1970) - UTC
tm tm;                              // the structure tm holds time information in a more convenient way

void loop() {

  // ignore loop execution more often than LED blink interval (1/8 second)
  unsigned long currentMillis = millis();
  if (currentMillis - previousTime < 125) {
    return;
  }
  previousTime = currentMillis;

  // track seconds based on LED blink interval (8 times = 1 second)
  ++interval;
  interval = interval % 8;
  if (interval == 0) { // every second

    // 1. check current water level
    updateWaterLevel();

    // 2. check current water pressure
    updateWaterPressure();

    // 3. control irrigation pump immediately
    controlIrrigationPump();

    if (now != 0) {  // wait for first NTP update
      time(&now);    // this function calls the NTP server only every hour
      localtime_r(&now, &tm);
    }

    if (tm.tm_min != lastMinute) { // every minute
      lastMinute = tm.tm_min;
      printTime(now);

      // 4. controll well pump
      controlWellPump();

      // reactive Wifi if connection lost
      if (wifiStatus == WIFI_STATUS_DISCONNECTED) {
        activateWifi();
      }

    }

  }

  // update LEDs
  blinkLeds();

}

void blinkLeds() {

  blinkWifiLed();
  blinkIrrigationPumpLed();
  blinkWellPumpLed();

}

void blinkWifiLed() {

  if (wifiStatus == WIFI_STATUS_DISCONNECTED) {
      portExpander.digitalWrite(GPIO_WIFI_LED, LOW);
  } else if (wifiStatus == WIFI_STATUS_CONNECTING) {
    if (interval % 2 == 0) { // blinking fast
      portExpander.digitalWrite(GPIO_WIFI_LED, HIGH);
    } else {
      portExpander.digitalWrite(GPIO_WIFI_LED, LOW);
    }
  } else if (wifiStatus == WIFI_STATUS_WAITING_FOR_NTP) {
    if (interval >> 3 == 0) { // blinking slow
      portExpander.digitalWrite(GPIO_WIFI_LED, HIGH);
    } else {
      portExpander.digitalWrite(GPIO_WIFI_LED, LOW);
    }
  } else if (wifiStatus == WIFI_STATUS_NTP_ACTIVE) {
    portExpander.digitalWrite(GPIO_WIFI_LED, HIGH);
  }

}

void setupWifi() {

  // track los of Wifi connection
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  portExpander.pinMode(GPIO_WIFI_LED, OUTPUT);

  // turn on Wifi
  activateWifi();

}

void activateWifi() {

  Serial.print(F("Connecting to "));
  Serial.println(WIFI_SSID);

  wifiStatus = WIFI_STATUS_CONNECTING;

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  #ifdef WIFI_PORT
    unsigned char wifiMac[18] = WIFI_MAC;
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_PORT, wifiMac);
  #else
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  #endif

}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {

  wifiStatus = WIFI_STATUS_WAITING_FOR_NTP;
  wifiConnected();

}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {

  wifiStatus = WIFI_STATUS_DISCONNECTED;
  wifiDisconnected();

}

void ntpTimeIsSet(bool from_sntp /* <= this parameter is optional */) {

  wifiStatus = WIFI_STATUS_NTP_ACTIVE;
  if (now == 0) {   // first NTP sync
    time(&now);
    localtime_r(&now, &tm);
    lastMinute = tm.tm_min;
  } else {
    time(&now);
  }

  Serial.print(F("NTP update: "));
  printTime(now);

}

// https://www.weigu.lu/microcontroller/tips_tricks/esp_NTP_tips_tricks/index.html
void setupNtp() {

  settimeofday_cb(ntpTimeIsSet);
  configTime(MY_TZ, MY_NTP_SERVER);

}

void wifiConnected() {

  Serial.print(F("Connected to WiFi: "));
  Serial.println(WiFi.localIP().toString());

  if(!LittleFS.begin()) {
    Serial.println(F("An error has occurred on mounting LittleFS"));
    return;
  }

  // Setup REST endpoints
  httpRestServer.on("/rssi", HTTP_GET, handleRSSI);
  httpRestServer.on("/config", HTTP_GET, handleGetConfig);
  setupWellPumpEndpoints();
  httpRestServer.onNotFound(handleNotFound);
  httpRestServer
      .serveStatic("/", LittleFS, "/www/")
      .setDefaultFile("index.html")
      .setCacheControl("no-cache, no-store, max-age=0")
      .setAuthentication(WWW_USERNAME, WWW_PASSWORD);
  setWebAppStatusEndpoints();
  httpRestServer.begin();

}

void handleNotFound(AsyncWebServerRequest *request) {

  String message = F("File Not Found\n\n");
  message += F("URI: ");
  message += request->url();
  message += F("\nMethod: ");
  message += request->methodToString();
  message += "\n";
  request->send(404, F("text/plain"), message);

}

void handleRSSI(AsyncWebServerRequest *request) {

  char rssi[16];
  snprintf(rssi, sizeof rssi, "%i", WiFi.RSSI());
  String message = F("RSSI: ");
  message += rssi;
  message += " dB";
  request->send(200, F("text/plain"), message);

}

void handleGetConfig(AsyncWebServerRequest *request) {
  
  File file = LittleFS.open(F("/config.json"), "r");
  if(!file){
    Serial.println(F("Failed to open `config.json` for reading"));
    return;
  }

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  while (file.available() > 0) {
    String line = file.readString();
    response->print(line);
  }
  request->send(response);

  file.close();

}

void wifiDisconnected() {
  
  Serial.println(F("WiFi disconnected!"));

  httpRestServer.end();

}

void setupValves() {

  portExpander.pinMode(GPIO_VALVE_1, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_1, RELAIS_OFF);
  portExpander.pinMode(GPIO_VALVE_2, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_2, RELAIS_OFF);
  portExpander.pinMode(GPIO_VALVE_3, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_3, RELAIS_OFF);
  portExpander.pinMode(GPIO_VALVE_4, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_4, RELAIS_OFF);
  portExpander.pinMode(GPIO_VALVE_5, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_5, RELAIS_OFF);
  portExpander.pinMode(GPIO_VALVE_6, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_6, RELAIS_OFF);

}

void printTime(time_t time) {

  localtime_r(&time, &tm);            // converts epoch time to tm structure

  Serial.print(tm.tm_year + 1900);  // years since 1900
  Serial.print("-");
  Serial.print(tm.tm_mon + 1);      // January = 0 (!)
  Serial.print("-");
  Serial.print(tm.tm_mday);         // day of month
  Serial.print(" ");
  Serial.print(tm.tm_hour);         // hours since midnight  0-23
  Serial.print(":");
  Serial.print(tm.tm_min);          // minutes after the hour  0-59
  Serial.print(":");
  Serial.print(tm.tm_sec);          // seconds after the minute  0-61*
  Serial.print(" (day: ");
  Serial.print(tm.tm_wday);         // days since Sunday 0-6
  Serial.print("; daylight-saving: ");
  if (tm.tm_isdst == 1)             // Daylight Saving Time flag
    Serial.print("1)");
  else
    Serial.print("0)");
  Serial.println();

}
