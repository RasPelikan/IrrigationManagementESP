#define WATERLEVEL_EMPTY 0
#define WATERLEVEL_1 1
#define WATERLEVEL_2 2
#define WATERLEVEL_3 3
#define WATERLEVEL_4 4
#define WATERLEVEL_FULL 5

uint8_t waterLevel = 101; // means print current level on startup
uint8_t waterStatusHysteresis = 0;
int waterPressure = 0;

const char *waterLevelLabel(uint8_t level) {
  switch (level) {
    case WATERLEVEL_EMPTY: return "0%";
    case WATERLEVEL_1:     return "0-25%";
    case WATERLEVEL_2:     return "25-50%";
    case WATERLEVEL_3:     return "50-75%";
    case WATERLEVEL_4:     return "75-100%";
    case WATERLEVEL_FULL:  return "100%";
    default:               return "unknown";
  }
}

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

  // Determine the level by the HIGHEST wet sensor (LOW = wet, HIGH = dry).
  // Checking top-down makes the reading tolerant against air bubbles on lower
  // sensors: a bubble can falsely report "dry" but it cannot falsely report
  // "wet", so we trust the highest wet signal and ignore any dry sensors below.
  uint8_t newLevel;
  if (!digitalRead(GPIO_WATERLEVEL_FULL)) {
    newLevel = WATERLEVEL_FULL;
  } else if (!digitalRead(GPIO_WATERLEVEL_3)) {
    newLevel = WATERLEVEL_4;
  } else if (!digitalRead(GPIO_WATERLEVEL_2)) {
    newLevel = WATERLEVEL_3;
  } else if (!digitalRead(GPIO_WATERLEVEL_1)) {
    newLevel = WATERLEVEL_2;
  } else if (!digitalRead(GPIO_WATERLEVEL_EMPTY)) {
    newLevel = WATERLEVEL_1;
  } else {                                          // all sensors dry
    newLevel = WATERLEVEL_EMPTY;
  }

  if (newLevel == waterLevel) {
    return;
  }

  uint8_t previousLevel = waterLevel;
  waterStatusHysteresis = irrigationConfig.waterLevelHysteresis;
  waterLevel = newLevel;
  irrigationPumpEnabled = (newLevel != WATERLEVEL_EMPTY);

  updateStatusClients(STATUS_UPDATE_WATERLEVEL);

  Serial.print(F("Waterlevel changed: "));
  Serial.print(waterLevelLabel(previousLevel));
  Serial.print(F(" -> "));
  Serial.println(waterLevelLabel(newLevel));

  if (newLevel == WATERLEVEL_EMPTY) {
    // close valves and end any active cycles before the pressure can drain through open valves
    abortCyclesDueToWaterShortage();
  }

}

void addWaterLevelStatus(JsonDocument &doc) {

  doc["waterLevel"] = waterLevelLabel(waterLevel);

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
