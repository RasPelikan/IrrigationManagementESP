#define MODE_VALVE_ON 1
#define MODE_VALVE_AUTO 0
#define MODE_VALVE_OFF 2
#define MODE_VALVE_PARAM "mode"
#define INDEX_VALVE_PARAM "index"

bool *activeCycles = NULL;                 // tracks which cycle is active

void setupIrrigationEndpoints() {

  httpRestServer.on("/api/irrigation/valve", HTTP_POST, handleValveMode);

}

void checkForValvesOfCycle(Cycle *cycle, bool *valveStatus) {

  // if cycle is longer active than the sum of all sequences then restart the sequences
  cycle->area->irrigatedPeriod %= cycle->area->totalTimeOfSequences;

  bool activeSequenceFound = false;
  uint8_t sizeOfSequence = cycle->area->sizeOfSequence;
  uint16_t calculatedDuration = 0;
  for (uint8_t sequenceIndex = 0; !activeSequenceFound && sequenceIndex < sizeOfSequence; ++sequenceIndex) {
    Sequence *sequence = &(cycle->area->sequence[sequenceIndex]);

    // sequence was already processed
    if ((calculatedDuration + sequence->duration) <= cycle->area->irrigatedPeriod) {
      calculatedDuration += sequence->duration;
    }
    // sequence is in progress
    else {
      activeSequenceFound = true;
      for (uint8_t valveIndex = 0; valveIndex < sequence->numberOfValves; ++valveIndex) {
        valveStatus[sequence->valves[valveIndex]->index] = true; // mark valve as active
      }
    }

  }

}

void checkForActiveCycles() {

  /* calculate active cycles */

  // calculatedCycles will hold all cycles to be actived
  Cycle **calculatedCycles = new Cycle*[numberOfCycles];
  for (uint8_t i = 0; i < numberOfCycles; ++i) {
    calculatedCycles[i] = NULL;
  }

  // scan past 24 hours to get cycles currently active. this is necessary because
  // starting within a cycle, the cycle would not be activated otherwise.
  for (uint8_t currentHour = 0; currentHour < 24; ++currentHour) {
    for (uint16_t currentMinute = 0; currentMinute < 60; ++currentMinute) {

      // calculate "current time" during scanning past 24 hours
      // (next 24 hours is the same as past 24h, but easier to calculate)
      uint16_t projectedMinute = currentMinute + tm_now.tm_min + 1;
      uint16_t currentTime;
      if (projectedMinute < 60) {
        currentTime = (projectedMinute + (currentHour + tm_now.tm_hour) * 100) % 2400;
      } else {
        currentTime = ((projectedMinute % 60) + (currentHour + tm_now.tm_hour + 1) * 100) % 2400;
      }

      // check for each cycle whether it needs to be activated or deactivated
      for (uint8_t cycleIndex = 0; cycleIndex < numberOfCycles; ++cycleIndex) {
        Cycle *cycle = &cycles[cycleIndex];

        // check for deactivation
        if (cycle->end == currentTime) {
          calculatedCycles[cycleIndex] = NULL;
        }

        // check for activation
        if (cycle->start == currentTime) {
          calculatedCycles[cycleIndex] = cycle;
        }

      }

    }
  }

  /* calculate active valves */

  // assume all valves to be inactive
  bool *valveStatus = new bool[numberOfValves];
  for (uint8_t i = 0; i < numberOfValves; ++i) {
    valveStatus[i] = false;
  }

  // check all cycles
  for (uint8_t cycleIndex = 0; cycleIndex < numberOfCycles; ++cycleIndex) {

    // if cycle active, then check whether to deactivate
    if (activeCycles[cycleIndex]) {

      Cycle *activeCycle = &cycles[cycleIndex];

      // irrigated period of active cycle is only increased if water is available/pump is active
      if (irrigationPumpEnabled) {
        ++(activeCycle->area->irrigatedPeriod);
      }

      // cycle needs to be deactivated
      if (calculatedCycles[cycleIndex] == NULL) {

        Serial.printf("Deactivating cycle %s\n", activeCycle->area->name);
        activeCycles[cycleIndex] = false;
        // reset tracking of how long irrigation happend
        if (activeCycle->area->resetOnActivation) {
          activeCycle->area->irrigatedPeriod = 0;
        }

      }

    }
    // cycle needs to be activated
    else if (calculatedCycles[cycleIndex] != NULL) {

      Serial.printf("Activating cycle %s\n", calculatedCycles[cycleIndex]->area->name);
      activeCycles[cycleIndex] = true;

    }

    // for active cycles check which valves have to be activated (if water is available)

    if (irrigationPumpEnabled && activeCycles[cycleIndex]) {
      checkForValvesOfCycle(&cycles[cycleIndex], valveStatus);
    }

  }

  // set active valves according to previous calculation

  for (uint8_t valveIndex = 0; valveIndex < numberOfValves; ++valveIndex) {
    if (valveStatus[valveIndex]) {
      if (!valves[valveIndex].active) {
        Serial.printf("%04d Activating valve %s\n", time, valves[valveIndex].id);
        valves[valveIndex].active = true;
      }
    } else {
      if (valves[valveIndex].active) {
        Serial.printf("%04d Deactivating valve %s\n", time, valves[valveIndex].id);
        valves[valveIndex].active = false;
      }
    }
  }

  delete[] valveStatus;
  delete[] calculatedCycles;

}

void setupCycles() {

    // initially, set active flag of all cycles to false
  activeCycles = new bool[numberOfCycles];
  for (uint8_t i = 0; i < numberOfCycles; ++i) {
    activeCycles[i] = false;
  }

}

void setupValves() {

  pinMode(GPIO_VALVE_1, OUTPUT);
  digitalWrite(GPIO_VALVE_1, RELAIS_OFF);
  pinMode(GPIO_VALVE_2, OUTPUT);
  digitalWrite(GPIO_VALVE_2, RELAIS_OFF);
  pinMode(GPIO_VALVE_3, OUTPUT);
  digitalWrite(GPIO_VALVE_3, RELAIS_OFF);
  pinMode(GPIO_VALVE_4, OUTPUT);
  digitalWrite(GPIO_VALVE_4, RELAIS_OFF);
  pinMode(GPIO_VALVE_5, OUTPUT);
  digitalWrite(GPIO_VALVE_5, RELAIS_OFF);
  
}

void switchValves() {

  // test for all valves to be switched on or off due to cycles or manual control
  bool atLeastOneValveChanged = false;
  for (uint8_t i = 0; i < numberOfValves; ++i) {
    if (valves[i].mode == VALVE_MODE_ON) {
      switchValve(i, true);
      atLeastOneValveChanged = true;
    } else if (valves[i].mode == VALVE_MODE_AUTO && (valves[i].active || valves[i].on != valves[i].active)) {
      switchValve(i, valves[i].active);
      atLeastOneValveChanged = true;
    } else if (valves[i].mode == VALVE_MODE_OFF && valves[i].on) {
      switchValve(i, false);
      atLeastOneValveChanged = true;
    }
  }

  if (atLeastOneValveChanged) {
    updateStatusClients(STATUS_UPDATE_CYCLE);
  }

}

void switchValve(uint8_t index, boolean on) {

  if (valves[index].url != NULL) {
    switchRemoteValve(valves[index].url, on);
  } else {
    switchGpioValve(valves[index].gpio, on);
  }
  valves[index].on = on;

}

void switchRemoteValve(char *url, boolean on) {
  
  HTTPClient http;
  http.begin(url);
  if (wifiConfig.httpUsername != NULL && wifiConfig.httpPassword != NULL) {
    http.setAuthorization(wifiConfig.httpUsername, wifiConfig.httpPassword);
  }
  int httpCode;
  if (on) {
    Serial.println("PUT ");
    httpCode = http.PUT("");
  } else {
    Serial.println("DELETE ");
    httpCode = http.sendRequest("DELETE");
  }
  Serial.println(url);
  if (httpCode < 0) {
    setError("Remote valve %s failed: %s", url, http.errorToString(httpCode).c_str());
  } else if (httpCode < 200 || httpCode >= 300) {
    setError("Remote valve %s returned HTTP %d", url, httpCode);
  } else if (error != NULL && strncmp(error, "Remote valve ", 13) == 0) {
    delete[] error;
    error = NULL;
    updateStatusClients(STATUS_UPDATE_ERROR);
  }
  http.end();

}

void switchGpioValve(uint8_t gpio, boolean on) {

  if ((gpio != GPIO_VALVE_1)
      && (gpio != GPIO_VALVE_2)
      && (gpio != GPIO_VALVE_3)
      && (gpio != GPIO_VALVE_4)
      && (gpio != GPIO_VALVE_5)) {
    return;
  }

  digitalWrite(gpio, on ? RELAIS_ON : RELAIS_OFF);

}

void addCycleStatus(JsonDocument &doc) {

  JsonArray valvesArray = doc[F("valves")].to<JsonArray>();
  for (uint8_t i = 0; i < numberOfValves; ++i) {
    JsonObject valveObj = valvesArray.add<JsonObject>();
    valveObj["id"] = valves[i].id;
    valveObj["mode"] = valves[i].mode == VALVE_MODE_ON
        ? "on"
        : valves[i].mode == VALVE_MODE_OFF
        ? "off"
        : "auto";
    valveObj["active"] = valves[i].active;
    valveObj["on"] = valves[i].on;
  }

}

void addPendingValve(uint8_t index) {
  uint8_t count = pendingValves[0];
  if (count < MAX_PENDING_VALVES) {
    pendingValves[1 + count] = index;
    pendingValves[0] = count + 1;
  }
}

bool applyValveMode(uint8_t index, const String &value) {

  if (value.equals(F("auto")) && (valves[index].mode != MODE_VALVE_AUTO)) {
    valves[index].mode = MODE_VALVE_AUTO;
    addPendingValve(index);
    return true;
  } else if (value.equals(F("off")) && (valves[index].mode != MODE_VALVE_OFF)) {
    valves[index].mode = MODE_VALVE_OFF;
    addPendingValve(index);
    return true;
  } else if (value.equals(F("on")) && (valves[index].mode != MODE_VALVE_ON)) {
    valves[index].mode = MODE_VALVE_ON;
    addPendingValve(index);
    return true;
  }
  return false;

}

void handleValveMode(AsyncWebServerRequest *request) {

  request->send(200, F("text/plain"), F(""));
  if (!request->hasParam(MODE_VALVE_PARAM, true)) {
    return;
  }
  String value = request->getParam(MODE_VALVE_PARAM, true)->value();
  bool updated = false;
  if (request->hasParam(INDEX_VALVE_PARAM, true)) {
    uint8_t index = constrain(request->getParam(INDEX_VALVE_PARAM, true)->value().toInt(), 0, 255);
    updated = applyValveMode(index, value);
  } else {
    for (uint8_t i = 0; i < numberOfValves; ++i) {
      updated |= applyValveMode(i, value);
    }
  }

}

void handleManualValveChanges() {

  if (pendingValves[0] == 0) {
    return;
  }

  uint8_t count = pendingValves[0];
  pendingValves[0] = 0;
  
  for (uint8_t i = 0; i < count; ++i) {
    uint8_t index = pendingValves[1 + i];
    if (valves[index].mode == VALVE_MODE_ON) {
      switchValve(index, true);
    } else if (valves[index].mode == VALVE_MODE_OFF) {
      switchValve(index, false);
    } else {
      switchValve(index, valves[index].active);
    }
  }

  updateStatusClients(STATUS_UPDATE_CYCLE);
  
}
