#define WATERLEVEL_EMPTY 0
#define WATERLEVEL_1 1
#define WATERLEVEL_2 2
#define WATERLEVEL_3 3
#define WATERLEVEL_4 4
#define WATERLEVEL_FULL 5

uint8_t waterLevel = 101; // means print current level on startup
uint8_t waterStatusHysteresis = 0;
int waterPressure = 0;

void setupWaterLevel() {

  pinMode(GPIO_WATERLEVEL_EMPTY, INPUT_PULLUP);
  pinMode(GPIO_WATERLEVEL_1, INPUT_PULLUP);
  pinMode(GPIO_WATERLEVEL_2, INPUT_PULLUP);
  pinMode(GPIO_WATERLEVEL_3, INPUT_PULLUP);
  pinMode(GPIO_WATERLEVEL_FULL, INPUT_PULLUP);

}

void setupPressureControl() {

  analogSetAttenuation(ADC_11db); // 0-3.3V range
  analogReadResolution(10);       // 0-1023

}

void updateWaterLevel() {

  // ignore changes in a row cause by small waves in the container
  if (waterStatusHysteresis > 0) {
    --waterStatusHysteresis;
    return;
  }

  if (digitalRead(GPIO_WATERLEVEL_EMPTY)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_EMPTY) {
      waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
      waterLevel = WATERLEVEL_EMPTY;
      irrigationPumpEnabled = false;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel 0%"));
    }
  } else if (digitalRead(GPIO_WATERLEVEL_1)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_1) {
      waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
      waterLevel = WATERLEVEL_1;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel 0-25%"));
    }
  } else if (digitalRead(GPIO_WATERLEVEL_2)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_2) {
      waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
      waterLevel = WATERLEVEL_2;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print("Waterlevel 25-50%");
    }
  } else if (digitalRead(GPIO_WATERLEVEL_3)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_3) {
      waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
      waterLevel = WATERLEVEL_3;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print("Waterlevel 50-75%");
    }
  } else if (digitalRead(GPIO_WATERLEVEL_FULL)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_4) {
      waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
      waterLevel = WATERLEVEL_4;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel 75-100%"));
    }
  } else { // all waterlevel sensors are low, means container is full
    if (waterLevel != WATERLEVEL_FULL) {
      waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
      waterLevel = WATERLEVEL_FULL;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel 100%"));
    }
  }

}

void addWaterLevelStatus(JsonDocument &doc) {

  if (waterLevel == WATERLEVEL_EMPTY) {
    doc["waterLevel"] = F("0%");
  } else if (waterLevel == WATERLEVEL_1) {
    doc["waterLevel"] = F("0-25%");
  } else if (waterLevel == WATERLEVEL_2) {
    doc["waterLevel"] = F("25-50%");
  } else if (waterLevel == WATERLEVEL_3) {
    doc["waterLevel"] = F("50-75%");
  } else if (waterLevel == WATERLEVEL_4) {
    doc["waterLevel"] = F("75-100%");
  } else if (waterLevel == WATERLEVEL_FULL) {
    doc["waterLevel"] = F("100%");
  } else {
    doc["waterLevel"] = F("Unknown");
  }

}

void addWaterPressureStatus(JsonDocument &doc) {

  float pressure = (waterPressure - irrigationConfig.pressureAdcOffset) / irrigationConfig.pressureAdcGradient;
  doc["waterPressure"] = pressure < 0 ? 0 : pressure;
  doc["waterPressureAdc"] = waterPressure;

}

void updateWaterPressure() {

  int newWaterPressure = analogRead(GPIO_ADC_PRESSURE);

  if (abs(newWaterPressure - waterPressure) > 10) {
    waterPressure = newWaterPressure;
    updateStatusClients(STATUS_UPDATE_WATERPRESSURE);
  }

}

bool isWaterPressureLow() {

  return waterPressure < irrigationConfig.waterPressureLow;

}

bool isWaterPressureHigh() {

  return waterPressure > irrigationConfig.waterPressureHigh;

}
