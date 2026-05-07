#define WATERLEVEL_EMPTY 0
#define WATERLEVEL_1 20
#define WATERLEVEL_2 40
#define WATERLEVEL_3 69
#define WATERLEVEL_4 80
#define WATERLEVEL_FULL 100

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
      Serial.print(F("Waterlevel "));
      Serial.print(WATERLEVEL_EMPTY);
      Serial.println(F("%"));
    }
  } else if (digitalRead(GPIO_WATERLEVEL_1)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_1) {
      waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
      waterLevel = WATERLEVEL_1;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print(F("Waterlevel "));
      Serial.print(WATERLEVEL_EMPTY);
      Serial.print(F("-"));
      Serial.print(WATERLEVEL_1);
      Serial.println(F("%"));
    }
  } else if (digitalRead(GPIO_WATERLEVEL_2)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_2) {
      waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
      waterLevel = WATERLEVEL_2;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print("Waterlevel ");
      Serial.print(WATERLEVEL_1);
      Serial.print("-");
      Serial.print(WATERLEVEL_2);
      Serial.println("%");
    }
  } else if (digitalRead(GPIO_WATERLEVEL_3)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_3) {
      waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
      waterLevel = WATERLEVEL_3;
      irrigationPumpEnabled = true;

      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
      Serial.print("Waterlevel ");
      Serial.print(WATERLEVEL_2);
      Serial.print("-");
      Serial.print(WATERLEVEL_3);
      Serial.println("%");
    }
  } else if (digitalRead(GPIO_WATERLEVEL_FULL)) { // pulled-up means no water
    if (waterLevel != WATERLEVEL_4) {
      waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
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
      waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
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
