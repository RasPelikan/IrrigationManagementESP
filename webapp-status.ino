AsyncEventSource statusEvents("/api/status-events");
uint8 numberOfStatusEventsClients = 0;

void setWebAppStatusEndpoints() {

  statusEvents.onConnect(webappInitStatus);
  httpRestServer.addHandler(&statusEvents);

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
      addWaterLevelStatus(doc);
    }
    if ((what == STATUS_UPDATE_ALL) || (what == STATUS_UPDATE_WATERPRESSURE)) {
      addWaterPressureStatus(doc);
    }
    if ((what == STATUS_UPDATE_ALL) || (what == STATUS_UPDATE_WELLPUMP)) {
      addWellPumpStatus(doc);
    }
    if ((what == STATUS_UPDATE_ALL) || (what == STATUS_UPDATE_IRRIGATIONPUMP)) {
      addIrrigationPumpStatus(doc);
    }
    char initialStatusEvent[300];
    serializeJson(doc, initialStatusEvent);
    statusEvents.send(initialStatusEvent, what == 0 ? "INIT" : "UPDATE", millis(), 1000);

}

void disconnectStatusClient(void *arg, AsyncClient *client) {

  numberOfStatusEventsClients -= 1;
  Serial.printf("Disconnect status client: %u -> %u\n", client->remotePort(), numberOfStatusEventsClients);

}
