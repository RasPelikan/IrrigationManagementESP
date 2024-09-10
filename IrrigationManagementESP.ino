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
AsyncEventSource statusEvents("/api/status-events");
uint8 numberOfStatusEventsClients = 0;

#define STATUS_UPDATE_ALL 0
#define STATUS_UPDATE_WELLPUMP 1
#define STATUS_UPDATE_IRRIGATIONPUMP 2
#define STATUS_UPDATE_RSSI 3
#define STATUS_UPDATE_TIME 4
#define STATUS_UPDATE_WATERPRESSURE 5
#define STATUS_UPDATE_WATERLEVEL 6

#define PORT_EXPANDER_ADDR 0 // Adresse 0x20 / 0
Adafruit_MCP23X17 portExpander;

// see https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library
#define GPIO_WATERLEVEL_EMPTY 4 // GPA4
#define WATERLEVEL_EMPTY 0
#define GPIO_WATERLEVEL_1 3 // GPA3
#define WATERLEVEL_1 2
#define GPIO_WATERLEVEL_2 2 // GPA2
#define WATERLEVEL_2 35
#define GPIO_WATERLEVEL_3 1 // GPA1
#define WATERLEVEL_3 67
#define GPIO_WATERLEVEL_FULL 0 // GPA0
#define WATERLEVEL_4 68
#define WATERLEVEL_FULL 100

#define GPIO_PUMP_IRRIGATION 8 // GPB0
#define GPIO_PUMP_IRRIGATION_LED 5 // GPA5
#define GPIO_PUMP_WELL 9 // GPB1
#define GPIO_PUMP_WELL_LED 6 // GPA6
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

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
//StaticJsonBuffer<756> jsonBuffer;   // see https://arduinojson.org/v5/assistant/

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
  setupWaterPumps();
  setupValves();

}

// application status
unsigned long previousTime = 0;
uint8 interval = 0;
uint8 wifiStatus = WIFI_STATUS_DISCONNECTED;
uint8 lastMinute = 0;
uint8 waterLevel = 101; // means print current level on startup
uint8 waterStatusHysteresis = 0;
bool wellPumpActive = false;
//uint8 irrigationPumpHysteresis = 0;
uint8 wellPumpInterval = PUMP_WELL_SLEEP;  // means wait for sleep interval on startup
int waterPressure = 0;
bool irrigationPumpEnabled = false;
bool irrigationPumpActive = false;
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

    // 2. control irrigation pump immediately
    controlIrrigationPump();

    if (now != 0) {  // wait for first NTP update
      time(&now);    // this function calls the NTP server only every hour
      localtime_r(&now, &tm);
    }

    if (tm.tm_min != lastMinute) { // every minute
      lastMinute = tm.tm_min;
      printTime(now);

      // 3. controll well pump
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

void updateWaterLevel() {

  // ignore changes in a row cause by small waves in the container
  if (waterStatusHysteresis > 0) {
    --waterStatusHysteresis;
    return;
  }
  
  if (portExpander.digitalRead(GPIO_WATERLEVEL_EMPTY)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_EMPTY) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_EMPTY;
      irrigationPumpEnabled = false;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel "));
      Serial.print(WATERLEVEL_EMPTY);
      Serial.println(F("%"));
    }
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_1)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_1) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_1;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel "));
      Serial.print(WATERLEVEL_EMPTY);
      Serial.print(F("-"));
      Serial.print(WATERLEVEL_1);
      Serial.println(F("%"));
    }
    /*
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_2)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_2) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_2;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print("Waterlevel ");
      Serial.print(WATERLEVEL_1);
      Serial.print("-");
      Serial.print(WATERLEVEL_2);
      Serial.println("%");
    }
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_3)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_3) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_3;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print("Waterlevel ");
      Serial.print(WATERLEVEL_2);
      Serial.print("-");
      Serial.print(WATERLEVEL_3);
      Serial.println("%");
    }
    */
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_FULL)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_4) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_4;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel "));
      Serial.print(WATERLEVEL_3);
      Serial.print(F("-"));
      Serial.print(WATERLEVEL_FULL);
      Serial.println(F("%"));
    }
  } else { // all waterlevel sensors are low, means container is full
    if (waterLevel != WATERLEVEL_FULL) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_FULL;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel "));
      Serial.print(WATERLEVEL_FULL);
      Serial.println(F("%"));
    }
  }

}

// control well pump: if container not full, then pump at
// a cycle of 45/15 minutes (according to pump specification)
void controlWellPump() {

  switchOfWellPumpIfContainerIsFull();

  if (wellPumpInterval > 0) {  // avoid overflow if cycle is set to 0
    --wellPumpInterval;
    updateStatusClients(STATUS_UPDATE_WELLPUMP);
  }
  if (wellPumpInterval > 0) {
    return;
  }

  // after interval completed switch well pump on or off
  activateOrDeactivateWellPumpIfContainerIsNotFull();

}

void activateOrDeactivateWellPumpIfContainerIsNotFull() {

  if (waterLevel == WATERLEVEL_FULL) {
    return;
  }

  if (wellPumpActive) {
    wellPumpActive = false;
    portExpander.digitalWrite(GPIO_PUMP_WELL, RELAIS_OFF);
    wellPumpInterval = PUMP_WELL_SLEEP;

    updateStatusClients(STATUS_UPDATE_WELLPUMP);
    Serial.print(F("Switched off well pump for "));
    Serial.print(PUMP_WELL_SLEEP);
    Serial.println(F(" minutes"));
  } else {
    wellPumpActive = true;
    portExpander.digitalWrite(GPIO_PUMP_WELL, RELAIS_ON);
    wellPumpInterval = PUMP_WELL_RUN;

    updateStatusClients(STATUS_UPDATE_WELLPUMP);
    Serial.print(F("Switched on well pump for "));
    Serial.print(PUMP_WELL_RUN);
    Serial.println(F(" minutes"));
  }

}

void switchOfWellPumpIfContainerIsFull() {

    // if container is full, then switch off well pump
    if ((waterLevel == WATERLEVEL_FULL) && wellPumpActive) {

      wellPumpInterval = PUMP_WELL_SLEEP;
      wellPumpActive = false;
      portExpander.digitalWrite(GPIO_PUMP_WELL, RELAIS_OFF);

      updateStatusClients(STATUS_UPDATE_WELLPUMP);
      Serial.println(F("Switched off well pump because container is full"));
      return;

    }

}

void controlIrrigationPump() {

  int previousPressure = waterPressure;
  waterPressure = analogRead(A0);
  if (abs(previousPressure - waterPressure) > 1) {
    updateStatusClients(STATUS_UPDATE_WATERPRESSURE);
  }

  if (irrigationPumpEnabled) {
    /* if (irrigationPumpHysteresis > 0) {
      --irrigationPumpHysteresis;
    } else*/ if (irrigationPumpActive
        && (waterPressure > WATERPRESSURE_HIGH_END)) {
      irrigationPumpActive = false;
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_OFF); // turn off irrigation pump
      updateStatusClients(STATUS_UPDATE_IRRIGATIONPUMP);
      Serial.println(F("Switched off irrigation pump because water-pressure beyond upper boundary"));
    } else if (!irrigationPumpActive
        && (waterPressure < WATERPRESSURE_LOW_END)) {
      irrigationPumpActive = true;
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_ON); // turn on irrigation pump
      updateStatusClients(STATUS_UPDATE_IRRIGATIONPUMP);
      Serial.println(F("Switched on irrigation pump because water-pressure below lower boundary"));
    }
  } else if (irrigationPumpActive) {
    irrigationPumpActive = false;
    //irrigationPumpHysteresis = PUMP_IRRIGATION_HYSTERESIS;
    portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_OFF); // turn off irrigation pump
    updateStatusClients(STATUS_UPDATE_IRRIGATIONPUMP);
    Serial.println(F("Switched off irrigation pump because no water left in container"));
  }

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

void blinkIrrigationPumpLed() {

  if (irrigationPumpActive) {
    if (interval >> 2 == 0) { // blinking slow
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION_LED, HIGH);
    } else {
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION_LED, LOW);
    }
  } else if (!irrigationPumpEnabled) {
    if (interval % 2 == 0) { // blinking fast
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION_LED, HIGH);
    } else {
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION_LED, LOW);
    }
  } else {
    portExpander.digitalWrite(GPIO_PUMP_IRRIGATION_LED, LOW);
  }

}

void blinkWellPumpLed() {

  if (wellPumpActive) {
    if (interval >> 2 == 0) { // blinking slow
      portExpander.digitalWrite(GPIO_PUMP_WELL_LED, HIGH);
    } else {
      portExpander.digitalWrite(GPIO_PUMP_WELL_LED, LOW);
    }
  } else if (wellPumpInterval > 0) {
    if (interval == 0) { // flash
      portExpander.digitalWrite(GPIO_PUMP_WELL_LED, HIGH);
    } else {
      portExpander.digitalWrite(GPIO_PUMP_WELL_LED, LOW);
    }
  } else {
    portExpander.digitalWrite(GPIO_PUMP_WELL_LED, LOW);
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
  httpRestServer.onNotFound(handleNotFound);
  httpRestServer
      .serveStatic("/", LittleFS, "/www/")
      .setDefaultFile("index.html")
      .setCacheControl("max-age=0")
      .setAuthentication(WWW_USERNAME, WWW_PASSWORD);
  statusEvents.onConnect(webappInitStatus);
  httpRestServer.addHandler(&statusEvents);
  httpRestServer.begin();

}

void webappInitStatus(AsyncEventSourceClient *client) {

  if(client->lastId()){
    Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
  }
  if (client->connected()) {
    numberOfStatusEventsClients += 1;
    Serial.printf("Connect status client: %u -> %u\n", client->client()->remotePort(), numberOfStatusEventsClients);
    client->client()->onDisconnect(disconnectStatusClient, NULL);
    updateStatusClients(STATUS_UPDATE_ALL);
  }

}

void updateStatusClients(uint8 what) {

    if (numberOfStatusEventsClients == 0) {
      return;
    }

    JsonDocument doc;
    if ((what == STATUS_UPDATE_ALL) || (what == STATUS_UPDATE_RSSI)) {
      doc["rssi"] = WiFi.RSSI();
    }
    if ((what == STATUS_UPDATE_ALL) || (what == STATUS_UPDATE_TIME)) {
      if (now != 0) {  // wait for first NTP update
        time(&now);
        char isoTimestamp[sizeof "2011-10-08T07:07:09.000Z"];
        strftime(isoTimestamp, sizeof isoTimestamp, "%FT%T.000Z", gmtime(&now));
        doc["currentDate"] = isoTimestamp;
      }
    }
    if ((what == STATUS_UPDATE_ALL) || (what == STATUS_UPDATE_WATERLEVEL)) {
      doc["waterLevel"] = waterLevel;
    }
    if ((what == STATUS_UPDATE_ALL) || (what == STATUS_UPDATE_WATERPRESSURE)) {
      doc["waterPressure"] = waterPressure;
    }
    if ((what == STATUS_UPDATE_ALL) || (what == STATUS_UPDATE_WELLPUMP)) {
      doc["wellPump"] = wellPumpActive ? "active-cycle" : wellPumpInterval > 0 ? "inactive-cycle" : "inactive";
      doc["wellPumpCycle"] = wellPumpInterval;
      doc["wellPumpMode"] = "auto";
    }
    if ((what == STATUS_UPDATE_ALL) || (what == STATUS_UPDATE_IRRIGATIONPUMP)) {
      doc["irrigationPump"] = irrigationPumpActive ? "active" : !irrigationPumpEnabled ? "out-of-water" : "inactive";
      doc["irrigationPumpMode"] = "auto";
    }
    char initialStatusEvent[300];
    serializeJson(doc, initialStatusEvent);
    statusEvents.send(initialStatusEvent, what == 0 ? "INIT" : "UPDATE", millis(), 1000);

}

void disconnectStatusClient(void *arg, AsyncClient *client) {

  numberOfStatusEventsClients -= 1;
  Serial.printf("Disconnect status client: %u -> %u\n", client->remotePort(), numberOfStatusEventsClients);

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

void setupWaterPumps() {

  portExpander.pinMode(GPIO_PUMP_IRRIGATION_LED, OUTPUT);
  portExpander.pinMode(GPIO_PUMP_WELL_LED, OUTPUT);

  portExpander.pinMode(GPIO_PUMP_IRRIGATION, OUTPUT);
  portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_OFF);
  portExpander.pinMode(GPIO_PUMP_WELL, OUTPUT);
  portExpander.digitalWrite(GPIO_PUMP_WELL, RELAIS_OFF);
  portExpander.pinMode(GPIO_WATERLEVEL_EMPTY, INPUT_PULLUP); // configure button pin for input with pull up
  portExpander.pinMode(GPIO_WATERLEVEL_1, INPUT_PULLUP); // configure button pin for input with pull up
  portExpander.pinMode(GPIO_WATERLEVEL_2, INPUT_PULLUP); // configure button pin for input with pull up
  portExpander.pinMode(GPIO_WATERLEVEL_3, INPUT_PULLUP); // configure button pin for input with pull up
  portExpander.pinMode(GPIO_WATERLEVEL_FULL, INPUT_PULLUP); // configure button pin for input with pull up

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
