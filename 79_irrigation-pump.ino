#define MODE_IRRIGATIONPUMP_OFF 1
#define MODE_IRRIGATIONPUMP_AUTO 0
#define MODE_IRRIGATIONPUMP_PARAM "mode"

bool irrigationPumpEnabled = false;
bool irrigationPumpActive = false;
uint8_t irrigationPumpMode = MODE_IRRIGATIONPUMP_AUTO;

void setupIrrigationPump() {

  pinMode(GPIO_IRRIGATIONPUMP_LED, OUTPUT);

  pinMode(GPIO_IRRIGATIONPUMP, OUTPUT);
  digitalWrite(GPIO_IRRIGATIONPUMP, RELAIS_OFF);

}

void setupIrrigationPumpEndpoints() {

  httpRestServer.on("/api/irrigation-pump", HTTP_POST, handleIrrigationPumpMode);

}

void controlIrrigationPump() {

  if (irrigationPumpEnabled) {

    if (irrigationPumpActive
        && (isWaterPressureHigh() || (irrigationPumpMode == MODE_IRRIGATIONPUMP_OFF))) {

      irrigationPumpActive = false;
      digitalWrite(GPIO_IRRIGATIONPUMP, RELAIS_OFF); // turn off irrigation pump
      updateStatusClients(STATUS_UPDATE_IRRIGATIONPUMP);
      Serial.println(F("Switched off irrigation pump because water-pressure beyond upper boundary"));

    } else if (!irrigationPumpActive
        && isWaterPressureLow()
        && (irrigationPumpMode != MODE_IRRIGATIONPUMP_OFF)) {

      irrigationPumpActive = true;
      digitalWrite(GPIO_IRRIGATIONPUMP, RELAIS_ON); // turn on irrigation pump
      updateStatusClients(STATUS_UPDATE_IRRIGATIONPUMP);
      Serial.println(F("Switched on irrigation pump because water-pressure below lower boundary"));

    }

  } else if (irrigationPumpActive) {

    irrigationPumpActive = false;
    digitalWrite(GPIO_IRRIGATIONPUMP, RELAIS_OFF); // turn off irrigation pump
    updateStatusClients(STATUS_UPDATE_IRRIGATIONPUMP);
    Serial.println(F("Switched off irrigation pump because no water left in container"));

  }

}

void blinkIrrigationPumpLed() {

  if (irrigationPumpActive) {
    if (interval >> 2 == 0) { // blinking slow
      digitalWrite(GPIO_IRRIGATIONPUMP_LED, HIGH);
    } else {
      digitalWrite(GPIO_IRRIGATIONPUMP_LED, LOW);
    }
  } else if (!irrigationPumpEnabled) {
    if (interval % 2 == 0) { // blinking fast
      digitalWrite(GPIO_IRRIGATIONPUMP_LED, HIGH);
    } else {
      digitalWrite(GPIO_IRRIGATIONPUMP_LED, LOW);
    }
  } else {
    digitalWrite(GPIO_IRRIGATIONPUMP_LED, LOW);
  }

}

void addIrrigationPumpStatus(JsonDocument &doc) {

  doc[F("irrigationPump")] = irrigationPumpActive ? F("active") : !irrigationPumpEnabled ? F("out-of-water") : F("inactive");
  doc[F("irrigationPumpMode")] = irrigationPumpMode == MODE_IRRIGATIONPUMP_OFF ? F("off") : F("auto");

}

void handleIrrigationPumpMode(AsyncWebServerRequest *request) {

  request->send(200, F("text/plain"), F(""));
  if (request->hasParam(MODE_IRRIGATIONPUMP_PARAM, true)) {
    String value = request->getParam(MODE_IRRIGATIONPUMP_PARAM, true)->value();
    bool updated = false;
    if (value.equals(F("auto")) && (irrigationPumpMode != MODE_IRRIGATIONPUMP_AUTO)) {
      irrigationPumpMode = MODE_IRRIGATIONPUMP_AUTO;
      updated = true;
    } else if (value.equals(F("off")) && (irrigationPumpMode != MODE_IRRIGATIONPUMP_OFF)) {
      irrigationPumpMode = MODE_IRRIGATIONPUMP_OFF;
      updated = true;
    }
    if (updated) {
      updateStatusClients(STATUS_UPDATE_IRRIGATIONPUMP);
      controlIrrigationPump();
    }
  }

}
