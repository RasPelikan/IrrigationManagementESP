#define MODE_WELLPUMP_ON 1
#define MODE_WELLPUMP_AUTO 0
#define MODE_WELLPUMP_OFF 2
#define MODE_WELLPUMP_PARAM "mode"

bool wellPumpActive = false;
uint8_t wellPumpMode = MODE_WELLPUMP_AUTO;
uint16_t wellPumpInterval = 0; // seconds — counts down to next on/off transition
uint16_t wellPumpOverfillCountdown = 0; // seconds — pump keeps running after FULL is reached

void setupWellPump() {

  // means that the sleep interval is waited for at startup to ensure
  // that the sleep interval is adhered to
  wellPumpInterval = irrigationConfig.wellPumpCycleOff * 60;

  pinMode(GPIO_WELLPUMP_LED, OUTPUT);

  pinMode(GPIO_WELLPUMP, OUTPUT);
  digitalWrite(GPIO_WELLPUMP, RELAIS_OFF);

}

void setupWellPumpEndpoints() {

  applyApiAuth(httpRestServer.on("/api/well-pump", HTTP_POST, handleWellPumpMode));

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

  if (wellPumpMode == MODE_WELLPUMP_OFF) {
    if (wellPumpActive) {
      switchOffWellPump(false);
    }
    return;
  }

  if (wellPumpMode == MODE_WELLPUMP_ON) {
    // Manual ON ignores the 15-min wait and the FULL state. switchOffWellPumpIfContainerIsFull
    // and tickWellPumpOverfill handle the safety stop when waterLevel reaches FULL.
    if (!wellPumpActive) {
      switchOnWellPump();
    } else if (wellPumpInterval == 0) {
      // 45-min manual ON cycle finished — revert to AUTO and start 15-min wait.
      wellPumpMode = MODE_WELLPUMP_AUTO;
      switchOffWellPump(true);
      Serial.println(F("Manual ON cycle completed — switched to AUTO"));
    }
    return;
  }

  // AUTO mode below — respects the 15-min wait, the FULL state, and (if
  // location is configured) the PV-aligned daylight window.
  if (waterLevel == WATERLEVEL_FULL) {
    return;
  }

  // Daylight gating: in AUTO, only fill containers during the productive PV
  // window. If we're outside the window we stop a running pump immediately —
  // continuing past sunset would waste battery/grid power, which is exactly
  // what this feature is meant to prevent. Manual ON is intentionally
  // unaffected (handled in the MODE_WELLPUMP_ON branch above).
  if (!isInDaylightWindow()) {
    if (wellPumpActive) {
      switchOffWellPump(true);
      Serial.println(F("Daylight window ended — switching off well pump"));
    }
    return;
  }

  if (wellPumpInterval > 0) {
    return;
  }

  if (wellPumpActive) {
    switchOffWellPump(true);
  } else {
    switchOnWellPump();
  }

}

void switchOnWellPump() {

    wellPumpActive = true;
    digitalWrite(GPIO_WELLPUMP, RELAIS_ON);
    wellPumpInterval = irrigationConfig.wellPumpCycleOn * 60;

    updateStatusClients(STATUS_UPDATE_WELLPUMP);

    Serial.print(F("Switched on well pump for "));
    Serial.print(irrigationConfig.wellPumpCycleOn);
    Serial.println(F(" minutes"));

}

void switchOffWellPump(bool setInterval) {

    wellPumpActive = false;
    wellPumpOverfillCountdown = 0;
    digitalWrite(GPIO_WELLPUMP, RELAIS_OFF);
    if (setInterval) {
      wellPumpInterval = irrigationConfig.wellPumpCycleOff * 60;
    } else {
      wellPumpInterval = 0;
    }

    updateStatusClients(STATUS_UPDATE_WELLPUMP);

    if (setInterval) {
      Serial.print(F("Switched off well pump for "));
      Serial.print(irrigationConfig.wellPumpCycleOff);
      Serial.println(F(" minutes"));
    } else {
      Serial.println(F("Switched off well pump"));
    }

}

void switchOffWellPumpIfContainerIsFull() {

    // if container is full, then switch off well pump
    if (waterLevel != WATERLEVEL_FULL) {
      return;
    }

    if (wellPumpActive) {

      // If overfill is configured and not already running, enter the overfill phase
      // instead of stopping immediately. The pump keeps running for X seconds so the
      // water can settle across the connected IBC containers.
      if (irrigationConfig.wellPumpCycleOverfill > 0 && wellPumpOverfillCountdown == 0) {
        wellPumpOverfillCountdown = irrigationConfig.wellPumpCycleOverfill;
        updateStatusClients(STATUS_UPDATE_WELLPUMP);
        Serial.print(F("Container full — entering overfill phase for "));
        Serial.print(wellPumpOverfillCountdown);
        Serial.println(F("s"));
      } else if (wellPumpOverfillCountdown == 0) {
        // No overfill configured — stop immediately as before.
        // Force AUTO: see comment in tickWellPumpOverfill().
        wellPumpMode = MODE_WELLPUMP_AUTO;
        switchOffWellPump(true);
        Serial.println(F("Switched off well pump because container is full"));
      }
      // If overfill countdown is already running, do nothing —
      // tickWellPumpOverfill() will stop the pump when the countdown expires.

    }
    // Pump not active + FULL: nothing to do; the AUTO cycle will wait for the
    // level to drop. (No log here — the per-second cadence would spam Serial.)

}

// Called once per second. Decrements the overfill countdown and stops the pump
// when it reaches zero.
void tickWellPumpOverfill() {

  if (wellPumpOverfillCountdown == 0) {
    return;
  }

  --wellPumpOverfillCountdown;

  if (wellPumpOverfillCountdown == 0) {
    if (wellPumpActive) {
      // Force AUTO so the 15-min wait set by switchOffWellPump(true) is actually
      // respected: in MODE_WELLPUMP_ON the cycle logic would restart the pump
      // immediately as soon as waterLevel drops below FULL, bypassing the wait.
      // Same safety override as switchOffWellPumpIfContainerIsFull.
      wellPumpMode = MODE_WELLPUMP_AUTO;
      switchOffWellPump(true);
      Serial.println(F("Overfill phase completed — switched off well pump"));
    }
  } else {
    updateStatusClients(STATUS_UPDATE_WELLPUMP);
  }

}

// Abort any running overfill phase (e.g. when the user changes mode manually
// or when the water level drops far enough that overfilling no longer applies).
void cancelWellPumpOverfill() {
  if (wellPumpOverfillCountdown > 0) {
    Serial.println(F("Overfill phase cancelled"));
    wellPumpOverfillCountdown = 0;
    updateStatusClients(STATUS_UPDATE_WELLPUMP);
  }
}

void blinkWellPumpLed() {

  if (wellPumpActive) {
    if (interval >> 2 == 0) { // blinking slow
      digitalWrite(GPIO_WELLPUMP_LED, HIGH);
    } else {
      digitalWrite(GPIO_WELLPUMP_LED, LOW);
    }
  } else if (wellPumpInterval > 0) {
    if (interval == 0) { // flash
      digitalWrite(GPIO_WELLPUMP_LED, HIGH);
    } else {
      digitalWrite(GPIO_WELLPUMP_LED, LOW);
    }
  } else {
    digitalWrite(GPIO_WELLPUMP_LED, LOW);
  }

}

void handleWellPumpMode(AsyncWebServerRequest *request) {

  request->send(200, F("text/plain"), F(""));
  if (!request->hasParam(MODE_WELLPUMP_PARAM, true)) {
    return;
  }
  String value = request->getParam(MODE_WELLPUMP_PARAM, true)->value();

  if (value.equals(F("off"))) {
    // OFF: stop immediately, regardless of cycle state or waterLevel.
    wellPumpMode = MODE_WELLPUMP_OFF;
    if (wellPumpActive) {
      switchOffWellPump(false);
    } else {
      cancelWellPumpOverfill();
    }
    Serial.println(F("Well pump: manual OFF"));
  } else if (value.equals(F("on"))) {
    // ON: start (or restart 45-min countdown), regardless of waterLevel or wait.
    // At WATERLEVEL_FULL the overfill safety in switchOffWellPumpIfContainerIsFull
    // will stop the pump after the configured overfill duration.
    wellPumpMode = MODE_WELLPUMP_ON;
    cancelWellPumpOverfill();
    if (!wellPumpActive) {
      switchOnWellPump();
    } else {
      wellPumpInterval = irrigationConfig.wellPumpCycleOn * 60;
      updateStatusClients(STATUS_UPDATE_WELLPUMP);
    }
    Serial.print(F("Well pump: manual ON for "));
    Serial.print(irrigationConfig.wellPumpCycleOn);
    Serial.println(F(" minutes"));
  } else if (value.equals(F("auto"))) {
    wellPumpMode = MODE_WELLPUMP_AUTO;
    cancelWellPumpOverfill();
    Serial.println(F("Well pump: AUTO"));
  } else {
    return;
  }
  updateStatusClients(STATUS_UPDATE_WELLPUMP);

}

void addWellPumpStatus(JsonDocument &doc) {

  // inactive-daylight is reported only in AUTO and only when the daylight
  // feature is configured — manual ON/OFF semantics stay verbatim.
  const bool daylightBlock = irrigationConfig.daylightEnabled
      && wellPumpMode == MODE_WELLPUMP_AUTO
      && waterLevel != WATERLEVEL_FULL
      && !isInDaylightWindow();

  const char *state;
  if (wellPumpOverfillCountdown > 0) {
    state = "active-overfill";
  } else if (wellPumpActive) {
    state = "active-cycle";
  } else if (daylightBlock) {
    state = "inactive-daylight";
  } else if (wellPumpInterval > 0) {
    state = "inactive-cycle";
  } else {
    state = "inactive";
  }
  doc[F("wellPump")] = state;
  doc[F("wellPumpCycle")] = wellPumpInterval;
  doc[F("wellPumpOverfill")] = wellPumpOverfillCountdown;
  doc[F("wellPumpMode")] = wellPumpMode == 0 ? F("auto") : wellPumpMode == 1 ? F("on") : F("off");

  // daylightEnabled is sent unconditionally so the webapp can render the
  // "Pump times:" row with a "calculating…" placeholder from the very first
  // SSE message — before NTP/sunrise calc has produced actual values.
  doc[F("daylightEnabled")] = irrigationConfig.daylightEnabled;

  // Surface today's permitted-pumping window (sunrise+startOffset .. sunset-endOffset)
  // to the webapp. The user only cares about *when the well pump may run*, not
  // about astronomical sunrise/sunset — so we ship the window endpoints + duration
  // pre-formatted. Skip when not yet computed (NTP pending) or when the offsets
  // collapse the window to empty — the webapp keeps showing "calculating…" until
  // these fields arrive.
  if (irrigationConfig.daylightEnabled
      && sunriseLocalMin >= 0 && sunsetLocalMin >= 0) {
    int windowStart = sunriseLocalMin + (int)irrigationConfig.wellPumpDaylightStartOffsetMin;
    int windowEnd   = sunsetLocalMin  - (int)irrigationConfig.wellPumpDaylightEndOffsetMin;
    if (windowStart < windowEnd && windowStart >= 0 && windowEnd <= 1440) {
      int durationMin = windowEnd - windowStart;
      char fromBuf[6];      // HH:MM
      char toBuf[6];        // HH:MM
      char durationBuf[6];  // HH:MM
      snprintf(fromBuf,     sizeof fromBuf,     "%02d:%02d", windowStart / 60, windowStart % 60);
      snprintf(toBuf,       sizeof toBuf,       "%02d:%02d", windowEnd   / 60, windowEnd   % 60);
      snprintf(durationBuf, sizeof durationBuf, "%02d:%02d", durationMin / 60, durationMin % 60);
      doc[F("daylightFrom")]     = fromBuf;
      doc[F("daylightTo")]       = toBuf;
      doc[F("daylightDuration")] = durationBuf;
    }
  }

}
