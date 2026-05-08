AsyncEventSource statusEvents("/api/status-events");
uint8_t numberOfStatusEventsClients = 0;

void setWebAppStatusEndpoints() {

  statusEvents.onConnect(statusClientConnected);
  statusEvents.onDisconnect(statusClientDisconnected);
  applyApiAuth(statusEvents);
  httpRestServer.addHandler(&statusEvents);
  applyApiAuth(httpRestServer.on("/api/config", HTTP_GET, handleGetConfig));
  applyApiAuth(httpRestServer.on("/api/config", HTTP_POST, handleSetConfig, NULL, handleConfigUpload));
  applyApiAuth(httpRestServer.on("/api/webapp", HTTP_POST, handleWebappUploaded, handleWebappUpload));
  applyApiAuth(httpRestServer.on("/api/reboot", HTTP_GET, handleDoReboot));

}

void handleDoReboot(AsyncWebServerRequest *request) {

  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain");
  request->send(response);

  delay(1000);            // wait for the response to be sent to the client
  ESP.restart();          // restart to reload changed configuration

}

// Files that must live directly under /www/ (not /www/assets/) for the webapp
// to work. Add new entries here when introducing more root-level files.
//   index.html: the SPA entry point.
//   sw.js: service workers can only control URLs under their own path, so the
//          file must be served from the site root for the PWA to install.
static const char *const ROOT_WEBAPP_FILES[] = { "index.html", "sw.js" };
static const size_t ROOT_WEBAPP_FILES_COUNT = sizeof(ROOT_WEBAPP_FILES) / sizeof(ROOT_WEBAPP_FILES[0]);

static bool isRootWebappFile(const String &filename) {
  for (size_t i = 0; i < ROOT_WEBAPP_FILES_COUNT; ++i) {
    if (filename.equals(ROOT_WEBAPP_FILES[i])) return true;
  }
  return false;
}

void handleWebappUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {

  if (!index) {
    Serial.printf("Webapp upload started: %s\n", filename.c_str());
    char tmpFilename[100];
    snprintf(tmpFilename, sizeof tmpFilename, isRootWebappFile(filename) ? "/www/tmp_%s" : "/www/assets/tmp_%s", filename.c_str());
    request->setAttribute("tmpFile", tmpFilename);
  }

  if (!request->hasAttribute("tmpFile")
        || request->getAttribute("tmpFile").isEmpty()) {
    return;
  }

  const String &tmpFilename = request->getAttribute("tmpFile");
  File file = LittleFS.open(tmpFilename.c_str(), !index ? "w" : "a");
  if (!file) {
    Serial.printf("Failed to open `%s` for %s\n", tmpFilename.c_str(), index ? "appending" : "writing");
    request->setAttribute("tmpFile", "");
    return;
  }

  file.write(data, len);
  file.close();

  if (final) {
    Serial.printf("Webapp upload ended: %u bytes\n", index+len);
  }

}

void handleWebappUploaded(AsyncWebServerRequest *request) {

  Serial.println("Webapp uploaded");

  // Step 1: Delete old (non-tmp) asset files
  File assetsRoot = LittleFS.open("/www/assets");
  if (assetsRoot && assetsRoot.isDirectory()) {
    // First collect paths to delete (can't modify directory while iterating)
    String toDelete[32];
    uint8_t deleteCount = 0;
    File file = assetsRoot.openNextFile();
    while (file && deleteCount < 32) {
      if (!file.isDirectory()) {
        const char *name = file.name();
        // Check if it's NOT a tmp file
        const char *lastSlash = strrchr(name, '/');
        const char *baseName = lastSlash ? lastSlash + 1 : name;
        if (strncmp(baseName, "tmp_", 4) != 0) {
          toDelete[deleteCount++] = file.path();
          Serial.printf("  CLEARING: %s\n", file.path());
        }
      }
      file = assetsRoot.openNextFile();
    }
    assetsRoot.close();
    for (uint8_t i = 0; i < deleteCount; ++i) {
      LittleFS.remove(toDelete[i].c_str());
    }
  }

  // Step 2: Rename tmp_ files in /www/assets/ (remove tmp_ prefix)
  assetsRoot = LittleFS.open("/www/assets");
  if (assetsRoot && assetsRoot.isDirectory()) {
    String fromPaths[32];
    String toPaths[32];
    uint8_t renameCount = 0;
    File file = assetsRoot.openNextFile();
    while (file && renameCount < 32) {
      if (!file.isDirectory()) {
        const char *name = file.name();
        const char *lastSlash = strrchr(name, '/');
        const char *baseName = lastSlash ? lastSlash + 1 : name;
        if (strncmp(baseName, "tmp_", 4) == 0) {
          fromPaths[renameCount] = file.path();
          toPaths[renameCount] = String("/www/assets/") + String(baseName + 4);
          Serial.printf("  MOVING: %s > %s\n", fromPaths[renameCount].c_str(), toPaths[renameCount].c_str());
          ++renameCount;
        }
      }
      file = assetsRoot.openNextFile();
    }
    assetsRoot.close();
    for (uint8_t i = 0; i < renameCount; ++i) {
      LittleFS.rename(fromPaths[i].c_str(), toPaths[i].c_str());
    }
  }

  // Step 3: Replace each root-level file (index.html, sw.js, ...) for which a
  // tmp_ counterpart was uploaded. Files not in this batch are left intact.
  for (size_t i = 0; i < ROOT_WEBAPP_FILES_COUNT; ++i) {
    String tmpPath = String("/www/tmp_") + ROOT_WEBAPP_FILES[i];
    String finalPath = String("/www/") + ROOT_WEBAPP_FILES[i];
    if (!LittleFS.exists(tmpPath.c_str())) continue;
    if (LittleFS.exists(finalPath.c_str())) {
      Serial.printf("  CLEARING: %s\n", finalPath.c_str());
      LittleFS.remove(finalPath.c_str());
    }
    Serial.printf("  MOVING: %s > %s\n", tmpPath.c_str(), finalPath.c_str());
    LittleFS.rename(tmpPath.c_str(), finalPath.c_str());
  }

  AsyncWebServerResponse *response = request->beginResponse(307, "text/plain"); // Temporary Redirect
  response->addHeader(F("Location"), F("/"));
  request->send(response);

}

void handleGetConfig(AsyncWebServerRequest *request) {

  File file = LittleFS.open(CONFIG_PATH, "r");
  if (!file || !file.available()){
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
    snprintf(tmpFilename, sizeof tmpFilename, "/tmp_%lu.json", millis());
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
    Serial.printf("Failed to open `%s` for %s\n", tmpFilename.c_str(), index ? "appending" : "writing");
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
  LittleFS.remove(CONFIG_PATH);
  if (LittleFS.rename(tmpFilename, CONFIG_PATH)) {
    response = request->beginResponse(200, "application/json",  F("{\"status\": 1}"));
  } else {
    response = request->beginResponse(500, "application/json", F("{\"status\": 0}"));
  }
  request->send(response);

  // restart after response was sent to activate new config uploaded
  delay(1000);            // wait for the response to be sent to the client
  ESP.restart();          // restart to reload changed configuration

}

void statusClientConnected(AsyncEventSourceClient *client) {

  if(client->lastId()){
    Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
  }
  if (client->connected()) {
    numberOfStatusEventsClients += 1;
    Serial.printf("Connect status client: %p -> %u\n", client, numberOfStatusEventsClients);
    updateStatusClients(STATUS_UPDATE_ALL);
  }

}

void statusClientDisconnected(AsyncEventSourceClient *client) {

  numberOfStatusEventsClients -= 1;
  Serial.printf("Disconnect status client: %p -> %u\n", client, numberOfStatusEventsClients);

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
  if (what & STATUS_UPDATE_CYCLE) {
    addCycleStatus(doc);
  }
  String json;
  serializeJson(doc, json);
  statusEvents.send(json.c_str(), what == STATUS_UPDATE_ALL ? "INIT" : "UPDATE", millis(), 1000);

}
