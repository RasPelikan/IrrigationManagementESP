#include "ElegantOTA.h"
#include <esp_sntp.h>

unsigned long ota_progress_millis = 0;

AsyncWebServer httpRestServer(80);

bool doOTAifActive() {
  ElegantOTA.loop();
  return ota_progress_millis != 0;
}

// Applies HTTP auth to API handlers when credentials are configured. Used by
// every endpoint registered via httpRestServer.on(...) and the SSE handler
// across the .ino files in this sketch. No-op if credentials are missing —
// the device falls back to an open API, matching the static-handler logic.
void applyApiAuth(AsyncWebHandler &handler) {
  if (wifiConfig.httpUsername != NULL && wifiConfig.httpPassword != NULL) {
    handler.setAuthentication(wifiConfig.httpUsername, wifiConfig.httpPassword);
  }
}

void setupOTA() {
  if (wifiConfig.httpUsername != NULL && wifiConfig.httpPassword != NULL) {
    ElegantOTA.setAuth(wifiConfig.httpUsername, wifiConfig.httpPassword);
  }
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

  pinMode(GPIO_WIFI_LED, OUTPUT);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false); // reconnect is done manually every minute
  WiFi.mode(WIFI_STA);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

  // initialize WiFi driver and get MAC address
  WiFi.disconnect(false, true);  // keep radio on, clear saved credentials
  delay(100);

  Serial.print("MAC-address: ");
  Serial.println(WiFi.macAddress());

  // register event handlers
  WiFi.onEvent(onWifiConnect, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(onWifiDisconnect, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  // turn on Wifi
  activateWifi();

}

void activateWifi() {

  // if already connecting, disconnect first to avoid "sta is connecting" error
  if (WiFi.status() == WL_CONNECTED || wifiStatus == WIFI_STATUS_CONNECTING) {
    WiFi.disconnect(false);
    delay(100);
  }

  Serial.print(F("Connecting to "));
  Serial.println(wifiConfig.ssid);

  wifiStatus = WIFI_STATUS_CONNECTING;

  if (wifiConfig.channel != 0) {
    if (wifiConfig.mac == NULL) {
      WiFi.begin(wifiConfig.ssid, wifiConfig.password, wifiConfig.channel);
    } else {
      /*
      unsigned char wifiMac[18] = WIFI_MAC;
      WiFi.begin(wifiConfig.ssid, wifiConfig.password, wifiConfig.channel, wifiMac);
      */
    }
  } else {
    WiFi.begin(wifiConfig.ssid, wifiConfig.password);
  }

}

void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info) {

  wifiStatus = WIFI_STATUS_WAITING_FOR_NTP;

  Serial.print(F("Connected to WiFi: "));
  Serial.println(WiFi.localIP().toString());

  // Setup REST endpoints
  applyApiAuth(httpRestServer.on("/rssi", HTTP_GET, handleRSSI));
  setupWellPumpEndpoints();
  setupIrrigationPumpEndpoints();
  setupIrrigationEndpoints();
  httpRestServer.onNotFound(handleNotFound);
  // The static tree is split so the PWA can install on Android:
  //   - /assets/index-*  (compiled webapp bundle): auth-protected.
  //   - everything else  (HTML shell, sw.js, manifest, icons): public.
  // Chrome on Android can't show a Basic Auth dialog inside a PWA standalone
  // window, so manifest, icons and sw.js need to be reachable without auth or
  // the install icon stays blank and the launched window stays empty. The
  // bundle URL prefix `/assets/index` matches Vite's hashed output filenames
  // (assets/index-XXXXXXXX.{js,css}) — handler registration order matters
  // because AsyncWebServer dispatches in registration order.
  AsyncStaticWebHandler &codeHandler = httpRestServer
      .serveStatic("/assets/index", LittleFS, "/www/assets/index")
      .setCacheControl("no-cache, no-store, max-age=0");
  AsyncStaticWebHandler &publicHandler = httpRestServer
      .serveStatic("/", LittleFS, "/www/")
      .setDefaultFile("index.html")
      .setCacheControl("no-cache, no-store, max-age=0");
  if (wifiConfig.httpUsername != NULL) {
    if (wifiConfig.httpPassword == NULL) {
      setError("Config JSON has no or empty value 'http.password'!");
      Serial.println(error);
    } else {
      codeHandler.setAuthentication(wifiConfig.httpUsername, wifiConfig.httpPassword);
    }
  }

  setupNtp();
  setWebAppStatusEndpoints();

  setupOTA();

  // start HTTP server (works even before WiFi connects, just won't receive requests)
  httpRestServer.begin();

}

void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info) {

  wifiStatus = WIFI_STATUS_DISCONNECTED;
  Serial.println(F("WiFi disconnected!"));
  httpRestServer.end();

}

void ntpTimeIsSet(struct timeval *tv) {

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

  esp_sntp_set_time_sync_notification_cb(ntpTimeIsSet);
  configTzTime(MY_TZ, MY_NTP_SERVER);

}

void handleNotFound(AsyncWebServerRequest *request) {

  File file = LittleFS.open("/www/index.html", "r");
  if (!file || !file.available()){
    Serial.println("Not found");
    String message = F("File Not Found\n\n");
    message += F("URI: ");
    message += request->url();
    message += F("\nMethod: ");
    message += request->methodToString();
    message += "\n";
    request->send(404, F("text/plain"), message);
    return;
  }

  AsyncResponseStream *response = request->beginResponseStream("text/html");
  while (file.available() > 0) {
    String line = file.readString();
    response->print(line);
  }
  request->send(response);

  file.close();

}

void handleRSSI(AsyncWebServerRequest *request) {

  char rssi[16];
  snprintf(rssi, sizeof rssi, "%i", WiFi.RSSI());
  String message = F("RSSI: ");
  message += rssi;
  message += " dB";
  request->send(200, F("text/plain"), message);

}
