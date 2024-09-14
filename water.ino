#define WATERLEVEL_EMPTY 0
#define WATERLEVEL_1 2
#define WATERLEVEL_2 35
#define WATERLEVEL_3 67
#define WATERLEVEL_4 68
#define WATERLEVEL_FULL 100

uint8 waterLevel = 101; // means print current level on startup
uint8 waterStatusHysteresis = 0;
int waterPressure = 0;

void setupWaterLevel() {

  portExpander.pinMode(GPIO_WATERLEVEL_EMPTY, INPUT_PULLUP); // configure button pin for input with pull up
  portExpander.pinMode(GPIO_WATERLEVEL_1, INPUT_PULLUP); // configure button pin for input with pull up
  portExpander.pinMode(GPIO_WATERLEVEL_2, INPUT_PULLUP); // configure button pin for input with pull up
  portExpander.pinMode(GPIO_WATERLEVEL_3, INPUT_PULLUP); // configure button pin for input with pull up
  portExpander.pinMode(GPIO_WATERLEVEL_FULL, INPUT_PULLUP); // configure button pin for input with pull up

}

void updateWaterLevel() {

  // ignore changes in a row cause by small waves in the container
  if (waterStatusHysteresis > 0) {
    --waterStatusHysteresis;
    return;
  }
  
  if (portExpander.digitalRead(GPIO_WATERLEVEL_EMPTY)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_EMPTY) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_EMPTY;
      irrigationPumpEnabled = false;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel "));
      Serial.print(WATERLEVEL_EMPTY);
      Serial.println(F("%"));
    }
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_1)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_1) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_1;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel "));
      Serial.print(WATERLEVEL_EMPTY);
      Serial.print(F("-"));
      Serial.print(WATERLEVEL_1);
      Serial.println(F("%"));
    }
    /*
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_2)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_2) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_2;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print("Waterlevel ");
      Serial.print(WATERLEVEL_1);
      Serial.print("-");
      Serial.print(WATERLEVEL_2);
      Serial.println("%");
    }
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_3)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_3) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_3;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print("Waterlevel ");
      Serial.print(WATERLEVEL_2);
      Serial.print("-");
      Serial.print(WATERLEVEL_3);
      Serial.println("%");
    }
    */
  } else if (portExpander.digitalRead(GPIO_WATERLEVEL_FULL)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_4) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_4;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel "));
      Serial.print(WATERLEVEL_3);
      Serial.print(F("-"));
      Serial.print(WATERLEVEL_FULL);
      Serial.println(F("%"));
    }
  } else { // all waterlevel sensors are low, means container is full
    if (waterLevel != WATERLEVEL_FULL) {
      waterStatusHysteresis = WATERLEVEL_HYSTERESIS;
      waterLevel = WATERLEVEL_FULL;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel "));
      Serial.print(WATERLEVEL_FULL);
      Serial.println(F("%"));
    }
  }

}

void addWaterLevelStatus(JsonDocument &doc) {

  doc["waterLevel"] = waterLevel;

}

void addWaterPressureStatus(JsonDocument &doc) {

  doc["waterPressure"] = waterPressure;

}

void updateWaterPressure() {

  int previousPressure = waterPressure;
  waterPressure = analogRead(A0);
  if (abs(previousPressure - waterPressure) > 1) {
    updateStatusClients(STATUS_UPDATE_WATERPRESSURE);
  }

}

bool isWaterPressureLow() {

  return waterPressure < WATERPRESSURE_LOW_END;

}

bool isWaterPressureHigh() {

  return waterPressure > WATERPRESSURE_HIGH_END;

}
