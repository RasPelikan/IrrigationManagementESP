
#include <SyncClient.h>
#include <async_config.h>
#include <tcp_axtls.h>

#include <Arduino.h>
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
  #include <ESPAsyncTCP.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif
#include <ESPAsyncTCPbuffer.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClient.h>
#include <Adafruit_MCP23X17.h>
#include <time.h>
#include "types.h"
#include "WString.h"  // https://github.com/esp8266/Arduino/blob/60fe7b4ca8cdca25366af8a7c0a7b70d32c797f8/doc/PROGMEM.rst
#include "LittleFS.h"
#include <ArduinoJson.h>

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

#define STATUS_UPDATE_ALL 255
#define STATUS_UPDATE_WELLPUMP 1
#define STATUS_UPDATE_IRRIGATIONPUMP 2
#define STATUS_UPDATE_RSSI 4
#define STATUS_UPDATE_TIME 8
#define STATUS_UPDATE_WATERPRESSURE 16
#define STATUS_UPDATE_WATERLEVEL 32
#define STATUS_UPDATE_CYCLE 64
#define STATUS_UPDATE_ERROR 128

#define MAX_ERROR_LENGTH 300

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
char *error = NULL;
uint32_t heapAfterSetup = 0;
bool wrongConfig = true;

void setup() {

  // Setup console output
  Serial.begin(115200);
  Serial.println("\n\n\n\n");

  if(!LittleFS.begin()) {
    Serial.println(F("An error has occurred on mounting LittleFS"));
    return;
  }

  // Setup port expander MCP27013 for I2C address 0x20 
  if (!portExpander.begin_I2C(0x20)) {
    Serial.println(F("Could not initialize I2C port-expander on port 0x20!"));
    return;
  }

  // load parameters
  if (!readConfiguration()) {
    return;
  }
  wrongConfig = false;

  // Initialize NTP for current time and Wifi
  setupNtp();
  setupWifi();

  // Initialize water pumps and valves
  setupIrrigationPump();
  setupWellPump();
  setupWaterLevel();
  setupValves();

  // Initialize cycle processing
  setupCycles();

  heapAfterSetup = ESP.getFreeHeap();

}

// application status
unsigned long previousTime = 0;
uint8_t interval = 0;
uint8_t wifiStatus = WIFI_STATUS_DISCONNECTED;
uint8_t lastMinute = 0;
uint8_t lastSecond = 0;
time_t now = 0;                     // this are the seconds since Epoch (1970) - UTC
tm tm_now;                          // the structure tm holds time information in a more convenient way

void loop() {

  // process data during OTA
  doOTAifActive();

  if (wrongConfig) {
    return;
  }

  // ignore loop execution more often than LED blink interval (1/8 second)
  unsigned long currentMillis = millis();
  if (currentMillis < previousTime) { // handle overflow
    previousTime = currentMillis;
    return;
  }
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
      localtime_r(&now, &tm_now);
    } else if (wifiStatus == WIFI_STATUS_DISCONNECTED) { // no Wifi connected on setup
      activateWifi();
    }

    if (tm_now.tm_min != lastMinute) { // every minute
      lastMinute = tm_now.tm_min;
      printTime(now);

      // 4. controll well pump
      controlWellPump();

      // 5. do irrigation cycles
      checkForActiveCycles();

      // 6. switch valves according to current cycle
      switchValves();

      // reactive Wifi if connection lost
      if (wifiStatus == WIFI_STATUS_DISCONNECTED) {
        activateWifi();
      }

    }

    // every 30 seconds send update to clients to keep SSE connection alive
    if ((tm_now.tm_sec != lastSecond)
        && ((tm_now.tm_sec == 15) || (tm_now.tm_sec == 45))) {
      lastSecond = tm_now.tm_sec;
      updateStatusClients(STATUS_UPDATE_ERROR);
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

void printTime(time_t time) {

  tm tmp_tm;
  localtime_r(&time, &tmp_tm);          // converts epoch time to tm structure

  Serial.printf_P(PSTR("%04u-%02u-%02u %02u:%02u:%02u (day: %u, daylight-saving: %u)\n"),
      tmp_tm.tm_year + 1900,            // years since 1900
      tmp_tm.tm_mon + 1,                // January = 0 (!)
      tmp_tm.tm_mday,                   // day of month
      tmp_tm.tm_hour,                   // hours since midnight 0-23
      tmp_tm.tm_min,                    // minutes after the hour 0-59
      tmp_tm.tm_sec,                    // seconds after the minute 0-59
      tmp_tm.tm_wday,                   // days since Sunday 0-6
      tmp_tm.tm_isdst);                 // Daylight Saving Time flag
  
}

void setError(PGM_P format, ...) {
  
  if (error != NULL) {
    return;
  }

  error = new char[MAX_ERROR_LENGTH];
  va_list arg;
  va_start(arg, format);
  vsnprintf_P(error, MAX_ERROR_LENGTH, format, arg);
  va_end(arg);

  Serial.println(error);

  updateStatusClients(STATUS_UPDATE_ERROR);

}
