#include "ElegantOTA.h"

unsigned long ota_progress_millis = 0;

AsyncWebServer httpRestServer(80);

bool doOTAifActive() {
  ElegantOTA.loop();
  return ota_progress_millis != 0;
}

void setupOTA() {
  ElegantOTA.begin(&httpRestServer);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
}

void onOTAStart() {
  ota_progress_millis = millis();
  Serial.println("OTA update started!");
}

void onOTAProgress(size_t current, size_t final) {
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  ota_progress_millis = 0;
}

void setupWifi() {

  // track los of Wifi connection
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  portExpander.pinMode(GPIO_WIFI_LED, OUTPUT);

  // Setup REST endpoints
  httpRestServer.on("/rssi", HTTP_GET, handleRSSI);
  setupWellPumpEndpoints();
  setupIrrigationPumpEndpoints();
  httpRestServer.onNotFound(handleNotFound);
  AsyncStaticWebHandler &handler = httpRestServer
      .serveStatic("/", LittleFS, "/www/")
      .setDefaultFile("index.html")
      .setCacheControl("no-cache, no-store, max-age=0");
  if (wifiConfig.httpUsername != NULL) {
    if (wifiConfig.httpPassword == NULL) {
      setError(PSTR("Config JSON has no or empty value 'http.password'!"));
      Serial.println(error);
    } else {
      handler.setAuthentication(wifiConfig.httpUsername, wifiConfig.httpPassword);
    }
  }

  setWebAppStatusEndpoints();

  setupOTA();

  // turn on Wifi
  activateWifi();

}

void activateWifi() {

  Serial.print(F("Connecting to "));
  Serial.println(wifiConfig.ssid);

  wifiStatus = WIFI_STATUS_CONNECTING;

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false); // reconnect is done manually every minute
  WiFi.mode(WIFI_STA);
  if (wifiConfig.port != 0) {
    if (wifiConfig.mac == NULL) {
      WiFi.begin(wifiConfig.ssid, wifiConfig.password, wifiConfig.port);
    } else {
      /*
      unsigned char wifiMac[18] = WIFI_MAC;
      WiFi.begin(wifiConfig.ssid, wifiConfig.password, wifiConfig.port, wifiMac);
      */
    }
  } else {
    WiFi.begin(wifiConfig.ssid, wifiConfig.password);
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
  if (now == 0) {   // first NTP sync
    time(&now);
    localtime_r(&now, &tm_now);
    lastMinute = tm_now.tm_min;
  } else {
    time(&now);
    localtime_r(&now, &tm_now);
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

  httpRestServer.begin();

}

void handleNotFound(AsyncWebServerRequest *request) {

  Serial.println("Not found");
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

void wifiDisconnected() {
  
  Serial.println(F("WiFi disconnected!"));

  httpRestServer.end();

}
