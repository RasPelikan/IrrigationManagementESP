AsyncEventSource statusEvents("/api/status-events");
uint8_t numberOfStatusEventsClients = 0;

void setWebAppStatusEndpoints() {

  statusEvents.onConnect(statusClientConnected);
  statusEvents.onDisconnect(statusClientDisconnected);
  httpRestServer.addHandler(&statusEvents);
  httpRestServer.on("/api/config", HTTP_GET, handleGetConfig);
  httpRestServer.on("/api/config", HTTP_POST, handleSetConfig, NULL, handleConfigUpload);
  
}

void handleGetConfig(AsyncWebServerRequest *request) {
  
  File file = LittleFS.open(F(CONFIG_PATH), "r");
  if (!file || !file.available() || !file.isFile()){
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

void handleConfigUpload(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {

  if (!index) {
    Serial.println("Config upload started");
    char tmpFilename[32];
    snprintf(tmpFilename, sizeof tmpFilename, "/tmp_%u.json", millis());
    Serial.println(tmpFilename);
    request->setAttribute("tmpFile", tmpFilename);
  }

  if (!request->hasAttribute("tmpFile")
        || request->getAttribute("tmpFile").isEmpty()) {
    return;
  }

  const String &tmpFilename = request->getAttribute("tmpFile");
  File file = LittleFS.open(tmpFilename.c_str(), "a");
  if (!file) {
    Serial.printf_P(PSTR("Failed to open `%s` for %s (%u)\n"), tmpFilename.c_str(), index ? "appending" : "writing", file.getWriteError());
    request->setAttribute("tmpFile", "");
    return;
  }

  file.write(data, len);
  file.close();

  if (index + len >= total) {
    Serial.printf("Config upload ended: %u bytes\n", index+len);
  }
  
}

void handleSetConfig(AsyncWebServerRequest *request) {

  if (!request->hasAttribute("tmpFile")
        || request->getAttribute("tmpFile").isEmpty()) {
    Serial.println("Will NOT rename config");
    return;
  } else {
    Serial.println("Will rename config");
  }
  const String &tmpFilename = request->getAttribute("tmpFile");

  AsyncWebServerResponse *response;
  LittleFS.remove(F(CONFIG_PATH));
  if (LittleFS.rename(tmpFilename, F(CONFIG_PATH))) {
    response = request->beginResponse(200, "application/json",  F("{\"status\": 1}"));
  } else {
    response = request->beginResponse(500, "application/json", F("{\"status\": 0}"));
  }
  request->send(response);

  // restart after response was sent to activate new config uploaded
  LittleFS.gc();          // flush changes to "disk"
  delay(1000);            // wait for the response to be sent to the client
  ESP.restart();          // restart to reload changed configuration

}

void statusClientConnected(AsyncEventSourceClient *client) {

  if(client->lastId()){
    Serial.printf_P(PSTR("Client reconnected! Last message ID that it got is: %u\n"), client->lastId());
  }
  if (client->connected()) {
    numberOfStatusEventsClients += 1;
    Serial.printf_P(PSTR("Connect status client: %p -> %u\n"), client, numberOfStatusEventsClients);
    updateStatusClients(STATUS_UPDATE_ALL);
  }

}

void statusClientDisconnected(AsyncEventSourceClient *client) {

  numberOfStatusEventsClients -= 1;
  Serial.printf_P(PSTR("Disconnect status client: %p -> %u\n"), client, numberOfStatusEventsClients);

}

void updateStatusClients(uint8_t what) {

  if (numberOfStatusEventsClients == 0) {
    return;
  }

  JsonDocument doc;
  if (what == STATUS_UPDATE_ALL) {
    doc[F("heapAfterSetup")] = heapAfterSetup;
  }
  if (what & STATUS_UPDATE_ERROR) {
    doc[F("error")] = error;
    doc[F("heap")] = ESP.getFreeHeap();
  }
  if (what & STATUS_UPDATE_RSSI) {
    doc[F("rssi")] = WiFi.RSSI();
  }
  if (what & STATUS_UPDATE_TIME) {
    if (now != 0) {  // wait for first NTP update
      time(&now);
      char isoTimestamp[sizeof "2011-10-08T07:07:09.000Z"];
      strftime(isoTimestamp, sizeof isoTimestamp, "%FT%T.000Z", gmtime(&now));
      doc[F("currentDate")] = isoTimestamp;
    }
  }
  if (what & STATUS_UPDATE_WATERLEVEL) {
    addWaterLevelStatus(doc);
  }
  if (what & STATUS_UPDATE_WATERPRESSURE) {
    addWaterPressureStatus(doc);
  }
  if (what & STATUS_UPDATE_WELLPUMP) {
    addWellPumpStatus(doc);
  }
  if (what & STATUS_UPDATE_IRRIGATIONPUMP) {
    addIrrigationPumpStatus(doc);
  }
  char initialStatusEvent[300];
  serializeJson(doc, initialStatusEvent);
  statusEvents.send(initialStatusEvent, what == STATUS_UPDATE_ALL ? F("INIT") : F("UPDATE"), millis(), 1000);

}

