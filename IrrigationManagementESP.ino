#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <Adafruit_MCP23X17.h>
#include <time.h>

const char* wifiSsid = "XXXXXXXXX";
const char* wifiPassword = "YYYYYYYYY";
// set MAC and Port of AccessPoint if you'r connecting to a mesh Wifi to stick to one certain AP.
const unsigned char wifiMac[18] = { 0x1C, 0x7F, 0x2C, 0x63, 0xA3, 0x58 };
const uint8 wifiPort = 11;

#define HTTP_REST_PORT 8080
ESP8266WebServer httpRestServer(HTTP_REST_PORT);

#define PORT_EXPANDER_ADDR 0 // Adresse 0x20 / 0
Adafruit_MCP23X17 portExpander;

// see https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library
#define GPIO_WATERLEVEL_EMPTY 4 // GPA4
#define WATERLEVEL_EMPTY 0 // GPA4
#define GPIO_WATERLEVEL_1 3 // GPA3
#define WATERLEVEL_1 2 // GPA3
#define GPIO_WATERLEVEL_2 2 // GPA2
#define WATERLEVEL_2 35 // GPA2
#define GPIO_WATERLEVEL_3 1 // GPA1
#define WATERLEVEL_3 67 // GPA1
#define GPIO_WATERLEVEL_FULL 0 // GPA0
#define WATERLEVEL_FULL 100 // GPA0
#define WATERLEVEL_HYSTERESIS 120 // 120 seconds = 2 minutes

#define GPIO_PUMP_IRRIGATION 8 // GPB0
#define GPIO_PUMP_WELL 9 // GPB1
#define PUMP_WELL_SLEEP 15 // 15/45 minute cycle
#define PUMP_WELL_RUN 45 // 45/15 minute cycle
#define GPIO_VALVE_1 10 // GPB2
#define GPIO_VALVE_2 11 // GPB3
#define GPIO_VALVE_3 12 // GPB4
#define GPIO_VALVE_4 13 // GPB5
#define GPIO_VALVE_5 14 // GPB6
#define GPIO_VALVE_6 15 // GPB7
#define RELAIS_ON LOW
#define RELAIS_OFF HIGH

#define WIFI_LED 7 // GPA7
#define WIFI_STATUS_DISCONNECTED 0
#define WIFI_STATUS_CONNECTING 1
#define WIFI_STATUS_WAITING_FOR_NTP 2
#define WIFI_STATUS_NTP_ACTIVE 3

#define LED BUILTIN_LED

const long blinkInterval = 125;

#define MY_NTP_SERVER "at.pool.ntp.org"           
#define MY_TZ "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"  // https://remotemonitoringsystems.ca/time-zone-abbreviations.php
time_t now = 0;                     // this are the seconds since Epoch (1970) - UTC
tm tm;                              // the structure tm holds time information in a more convenient way

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

void setup() {

  // Setup console output
  Serial.begin(115200);

  // Setup port expander MCP27013 for I2C address 0x20 
  if (!portExpander.begin_I2C(0x20)) {
    Serial.println("Could not initialize I2C port-expander on port 0x20!");
    while (1);
  }

  setupNtp();

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
  setupWifi();

  setupWaterPump();
  setupValves();

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

}

unsigned long previousTime = 0;
uint8 interval = 0;
uint8 wifiStatus = WIFI_STATUS_DISCONNECTED;
uint8 lastMinute = 61;
uint8 waterLevel = 0;
uint8 waterStatusHysteresis = 0;
uint8 wellPumpEnabled = 0;
uint8 wellPumpInterval = PUMP_WELL_SLEEP;

void loop() {

  // process REST requests
  httpRestServer.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - previousTime < blinkInterval) {
    return;
  }

  previousTime = currentMillis;
  ++interval;

  interval = interval % 8;
  if (interval == 0) { // every second

    testForWater();

    time(&now);
    localtime_r(&now, &tm);
    if (tm.tm_min != lastMinute) { // every minute
      lastMinute = tm.tm_min;
      printTime(now);

      // control well pump: if container not full, then pump at
      // a cycle of 45/15 minutes (according to pump specification)
      if (wellPumpInterval > 0) {
        if (waterLevel != WATERLEVEL_FULL) {
          --wellPumpInterval;
        } else {
          wellPumpInterval = 0;
          wellPumpEnabled = false;
          portExpander.digitalWrite(GPIO_PUMP_WELL, RELAIS_OFF);
        }
      } else if (waterLevel != WATERLEVEL_FULL) {
        if (wellPumpEnabled) {
          wellPumpEnabled = false;
          portExpander.digitalWrite(GPIO_PUMP_WELL, RELAIS_OFF);
          wellPumpInterval = PUMP_WELL_SLEEP - 1;
          if (Serial.availableForWrite()) {
            Serial.print("Switch off well pump for ");
            Serial.print(PUMP_WELL_SLEEP);
            Serial.println(" minutes");
          }
        } else {
          wellPumpEnabled = true;
          portExpander.digitalWrite(GPIO_PUMP_WELL, RELAIS_ON);
          wellPumpInterval = PUMP_WELL_RUN - 1;
          if (Serial.availableForWrite()) {
            Serial.print("Switch on well pump for ");
            Serial.print(PUMP_WELL_RUN);
            Serial.println(" minutes");
          }
        }
      }

      if (wifiStatus == WIFI_STATUS_DISCONNECTED) {
        setupWifi();
      }

    }

  }

  blinkLeds();

}

void testForWater() {

  if (waterStatusHysteresis > 0) {
    --waterStatusHysteresis;
    return;
  }
  
  if (portExpander.digitalRead(GPIO_WATERLEVEL_EMPTY)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_EMPTY) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_EMPTY;
      digitalWrite(LED, HIGH);
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_OFF); // disable irrigation pump
      if (Serial.availableForWrite()) {
        Serial.print("Waterlevel ");
        Serial.print(WATERLEVEL_EMPTY);
        Serial.println("%");
      }
    }
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_1)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_1) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_1;
      digitalWrite(LED, LOW);
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_ON); // enable irrigation pump
      if (Serial.availableForWrite()) {
        Serial.print("Waterlevel ");
        Serial.print(WATERLEVEL_EMPTY);
        Serial.print("-");
        Serial.print(WATERLEVEL_1);
        Serial.println("%");
      }
    }
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_2)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_2) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_2;
      digitalWrite(LED, LOW);
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_ON); // enable irrigation pump
      if (Serial.availableForWrite()) {
        Serial.print("Waterlevel ");
        Serial.print(WATERLEVEL_1);
        Serial.print("-");
        Serial.print(WATERLEVEL_2);
        Serial.println("%");
      }
    }
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_3)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_3) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_3;
      digitalWrite(LED, LOW);
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_ON); // enable irrigation pump
      if (Serial.availableForWrite()) {
        Serial.print("Waterlevel ");
        Serial.print(WATERLEVEL_2);
        Serial.print("-");
        Serial.print(WATERLEVEL_3);
        Serial.println("%");
      }
    }
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_FULL)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_FULL) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_FULL;
      digitalWrite(LED, LOW);
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_ON); // enable irrigation pump
      if (Serial.availableForWrite()) {
        Serial.print("Waterlevel ");
        Serial.print(WATERLEVEL_3);
        Serial.print("-");
        Serial.print(WATERLEVEL_FULL);
        Serial.println("%");
      }
    }
  } else {
    if (waterLevel != WATERLEVEL_FULL) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_FULL;
      digitalWrite(LED, LOW);
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_ON); // enable irrigation pump
      if (Serial.availableForWrite()) {
        Serial.print("Waterlevel ");
        Serial.print(WATERLEVEL_FULL);
        Serial.println("%");
      }
    }
  }

}

void blinkLeds() {

  if (wifiStatus == WIFI_STATUS_DISCONNECTED) {
      portExpander.digitalWrite(WIFI_LED, LOW);
  } else if (wifiStatus == WIFI_STATUS_CONNECTING) {
    if (interval % 2 == 0) { // blinking fast
      portExpander.digitalWrite(WIFI_LED, HIGH);
    } else {
      portExpander.digitalWrite(WIFI_LED, LOW);
    }
  } else if (wifiStatus == WIFI_STATUS_WAITING_FOR_NTP) {
    if (interval >> 2 % 2 == 0) { // blinking slow
      portExpander.digitalWrite(WIFI_LED, HIGH);
    } else {
      portExpander.digitalWrite(WIFI_LED, LOW);
    }
  } else if (wifiStatus == WIFI_STATUS_NTP_ACTIVE) {
    portExpander.digitalWrite(WIFI_LED, HIGH);
  }

}

void setupWifi() {

  portExpander.pinMode(WIFI_LED, OUTPUT);

  Serial.print("Connecting to ");
  Serial.println(ssid);

  wifiStatus = WIFI_STATUS_CONNECTING;

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  // see https://olimex.wordpress.com/2021/12/10/avoid-wifi-channel-12-13-14-when-working-with-esp-devices/
  if (wifiPort < 15) {
    WiFi.begin(wifiSsid, wifiPassword, wifiPort, wifiMac);
  } else {
    WiFi.begin(wifiSsid, wifiPassword);
  }

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
  time(&now);                       // this function calls the NTP server only every hour

  Serial.print("NTP update: ");
  printTime(now);

}

// https://www.weigu.lu/microcontroller/tips_tricks/esp_NTP_tips_tricks/index.html
void setupNtp() {

  settimeofday_cb(ntpTimeIsSet);
  configTime(MY_TZ, MY_NTP_SERVER);

}

void wifiConnected() {

  Serial.print("Connected to WiFi: ");
  Serial.println(WiFi.localIP().toString());

  // Setup REST endpoints
  httpRestServer.on("/rssi", HTTP_GET, handleRSSI);
  httpRestServer.on("/", HTTP_GET, []() {
      httpRestServer.send(200, F("text/html"), F("Welcome to the REST Web Server"));
    });
  httpRestServer.onNotFound(handleNotFound);
  
  httpRestServer.begin();

}

void handleNotFound() {

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpRestServer.uri();
  message += "\nMethod: ";
  message += (httpRestServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpRestServer.args();
  message += "\n";
  for (uint8_t i = 0; i < httpRestServer.args(); i++) {
    message += " " + httpRestServer.argName(i) + ": " + httpRestServer.arg(i) + "\n";
  }
  httpRestServer.send(404, "text/plain", message);

}

void handleRSSI() {

  char rssi[16];
  snprintf(rssi, sizeof rssi, "%i", WiFi.RSSI());
  String message = "RSSI: ";
  message += rssi;
  message += " dB";
  httpRestServer.send(200, "text/plain", message);

}

void wifiDisconnected() {
  
  Serial.println("WiFi disconnected!");

  httpRestServer.stop();

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

void setupWaterPump() {

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
