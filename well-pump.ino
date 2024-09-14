#define GPIO_WELLPUMP 9 // GPB1
#define GPIO_WELLPUMP_LED 6 // GPA6

#define MODE_WELLPUMP_ON 1
#define MODE_WELLPUMP_AUTO 0
#define MODE_WELLPUMP_OFF 2
#define MODE_WELLPUMP_PARAM "mode"

bool wellPumpActive = false;
uint8 wellPumpMode = MODE_WELLPUMP_AUTO;
uint8 wellPumpInterval = WELLPUMP_SLEEP;  // means wait for sleep interval on startup

void setupWellPump() {

  portExpander.pinMode(GPIO_WELLPUMP_LED, OUTPUT);

  portExpander.pinMode(GPIO_WELLPUMP, OUTPUT);
  portExpander.digitalWrite(GPIO_WELLPUMP, RELAIS_OFF);

}

void setupWellPumpEndpoints() {

  httpRestServer.on("/api/well-pump", HTTP_POST, handleWellPumpMode);

}

// control well pump: if container not full, then pump at
// a cycle of 45/15 minutes (according to pump specification)
void controlWellPump() {

  switchOffWellPumpIfContainerIsFull();

  activateOrDeactivateWellPumpIfContainerIsNotFull();

}

void activateOrDeactivateWellPumpIfContainerIsNotFull() {

  if (wellPumpInterval > 0) {  // avoid overflow if cycle is set to 0
    --wellPumpInterval;
    updateStatusClients(STATUS_UPDATE_WELLPUMP);
  }

  if (waterLevel == WATERLEVEL_FULL) {
    return;
  }

  if (wellPumpMode == MODE_WELLPUMP_ON) {
    if (!wellPumpActive) {
      switchOnWellPump();
    } else if (wellPumpInterval == 0) {
      switchOffWellPump(false);
    }
    return;
  } else if (wellPumpMode == MODE_WELLPUMP_OFF) {
    if (wellPumpActive) {
      switchOffWellPump(false);
    }
    return;
  } else if (wellPumpInterval > 0) {
    return;
  }

  // after interval completed switch well pump on or off
  wellPumpMode = MODE_WELLPUMP_AUTO;
  if (wellPumpActive) {
    switchOffWellPump(true);
  } else {
    switchOnWellPump();
  }

}

void switchOnWellPump() {

    wellPumpActive = true;
    portExpander.digitalWrite(GPIO_WELLPUMP, RELAIS_ON);
    wellPumpInterval = WELLPUMP_RUN;

    updateStatusClients(STATUS_UPDATE_WELLPUMP);

    Serial.print(F("Switched on well pump for "));
    Serial.print(WELLPUMP_RUN);
    Serial.println(F(" minutes"));

}

void switchOffWellPump(bool setInterval) {

    wellPumpActive = false;
    portExpander.digitalWrite(GPIO_WELLPUMP, RELAIS_OFF);
    if (setInterval) {
      wellPumpInterval = WELLPUMP_SLEEP;
    } else {
      wellPumpInterval = 0;
    }

    updateStatusClients(STATUS_UPDATE_WELLPUMP);

    if (setInterval) {
      Serial.print(F("Switched off well pump for "));
      Serial.print(WELLPUMP_SLEEP);
      Serial.println(F(" minutes"));
    } else {
      Serial.println(F("Switched off well pump"));
    }

}

void switchOffWellPumpIfContainerIsFull() {

    // if container is full, then switch off well pump
    if (waterLevel == WATERLEVEL_FULL) {

      if (wellPumpActive) {

        wellPumpMode = MODE_WELLPUMP_AUTO;
        switchOffWellPump(true);

        Serial.println(F("Switched off well pump because container is full"));

      } else if (wellPumpMode == MODE_WELLPUMP_AUTO) {

        Serial.println(F("Disable well pump because container is full"));

      }

    }

}

void blinkWellPumpLed() {

  if (wellPumpActive) {
    if (interval >> 2 == 0) { // blinking slow
      portExpander.digitalWrite(GPIO_WELLPUMP_LED, HIGH);
    } else {
      portExpander.digitalWrite(GPIO_WELLPUMP_LED, LOW);
    }
  } else if (wellPumpInterval > 0) {
    if (interval == 0) { // flash
      portExpander.digitalWrite(GPIO_WELLPUMP_LED, HIGH);
    } else {
      portExpander.digitalWrite(GPIO_WELLPUMP_LED, LOW);
    }
  } else {
    portExpander.digitalWrite(GPIO_WELLPUMP_LED, LOW);
  }

}

void handleWellPumpMode(AsyncWebServerRequest *request) {

  request->send(200, F("text/plain"), F(""));
  if (request->hasParam(MODE_WELLPUMP_PARAM, true)) {
    String value = request->getParam(MODE_WELLPUMP_PARAM, true)->value();
    bool updated = false;
    if (value.equals(F("auto")) && (wellPumpMode != MODE_WELLPUMP_AUTO)) {
      wellPumpMode = MODE_WELLPUMP_AUTO;
      updated = true;
    } else if (value.equals(F("on")) && (wellPumpMode != MODE_WELLPUMP_ON)) {
      wellPumpMode = MODE_WELLPUMP_ON;
      updated = true;
    } else if (value.equals(F("off")) && (wellPumpMode != MODE_WELLPUMP_OFF)) {
      wellPumpMode = MODE_WELLPUMP_OFF;
      updated = true;
    }
    if (updated) {
      updateStatusClients(STATUS_UPDATE_WELLPUMP);
      controlWellPump();
    }
  }

}

void addWellPumpStatus(JsonDocument &doc) {
  
  doc["wellPump"] = wellPumpActive ? "active-cycle" : wellPumpInterval > 0 ? "inactive-cycle" : "inactive";
  doc["wellPumpCycle"] = wellPumpInterval;
  doc["wellPumpMode"] = wellPumpMode == 0 ? "auto" : wellPumpMode == 1 ? "on" : "off";

}