#define WATERLEVEL_EMPTY 0
#define WATERLEVEL_1 1
#define WATERLEVEL_2 2
#define WATERLEVEL_3 3
#define WATERLEVEL_4 4
#define WATERLEVEL_FULL 5

uint8_t waterLevel = 101; // means print current level on startup
// Drop-confirmation tracking — see updateWaterLevel(). 0xFF means "no drop candidate pending".
uint8_t waterLevelDropCandidate = 0xFF;
uint8_t waterLevelDropConfirms = 0;
// Diagnostic snapshot of the per-sensor wet/dry result of the most recent burst,
// indexed top-to-bottom: [0]=FULL, [1]=3, [2]=2, [3]=1, [4]=EMPTY. 'W' = wet, '.' = dry.
// Pushed to clients on every change so the user can see chatter independently of the
// debounced waterLevel.
char waterLevelRaw[6] = "?????";
static char waterLevelRawLastBroadcast[6] = "?????";
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

// Read all five float switches once and pick the HIGHEST closed switch as the level
// (LOW = closed = float lifted = wet, HIGH = open = dry, via INPUT_PULLUP). Magnetic
// reed contacts settle in microseconds and don't chatter at the 1 Hz call cadence;
// any rare mid-transition read is harmless because updateWaterLevel() requires the
// drop-debounce window before committing a falling level.
static uint8_t readWaterLevelSample() {

  static const uint8_t pins[5] = {
    GPIO_WATERLEVEL_EMPTY,
    GPIO_WATERLEVEL_1,
    GPIO_WATERLEVEL_2,
    GPIO_WATERLEVEL_3,
    GPIO_WATERLEVEL_FULL,
  };
  bool wet[5];
  for (uint8_t s = 0; s < 5; ++s) {
    wet[s] = !digitalRead(pins[s]);
  }
  // top-to-bottom snapshot for diagnostics: index 0 = FULL, index 4 = EMPTY
  waterLevelRaw[0] = wet[4] ? 'W' : '.';
  waterLevelRaw[1] = wet[3] ? 'W' : '.';
  waterLevelRaw[2] = wet[2] ? 'W' : '.';
  waterLevelRaw[3] = wet[1] ? 'W' : '.';
  waterLevelRaw[4] = wet[0] ? 'W' : '.';
  waterLevelRaw[5] = '\0';

  if (wet[4]) return WATERLEVEL_FULL;
  if (wet[3]) return WATERLEVEL_4;
  if (wet[2]) return WATERLEVEL_3;
  if (wet[1]) return WATERLEVEL_2;
  if (wet[0]) return WATERLEVEL_1;
  return WATERLEVEL_EMPTY;

}

static void commitWaterLevel(uint8_t newLevel) {

  uint8_t previousLevel = waterLevel;
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

void updateWaterLevel() {

  uint8_t newLevel = readWaterLevelSample();
  bool committed = false;

  // Steady — clear any pending drop candidate.
  if (newLevel == waterLevel) {
    waterLevelDropCandidate = 0xFF;
    waterLevelDropConfirms = 0;
  }
  // Rising commits immediately — over-reporting the bracket for a single tick has no
  // operational impact (cycles/abort only react to falling levels). The sentinel start
  // value (101) also lands here so the first measurement on boot is shown right away
  // regardless of whether it would be a "drop" from 101.
  else if (newLevel > waterLevel || waterLevel > WATERLEVEL_FULL) {
    commitWaterLevel(newLevel);
    committed = true;
    waterLevelDropCandidate = 0xFF;
    waterLevelDropConfirms = 0;
  }
  // Falling — require waterLevelHysteresis consecutive seconds of the SAME drop reading
  // before committing, so a mechanical reed bouncing once during a float transition can
  // not trigger a false cycle abort. If subsequent reads show a different drop level we
  // reset the counter — only a stable signal commits.
  else if (newLevel != waterLevelDropCandidate) {
    waterLevelDropCandidate = newLevel;
    waterLevelDropConfirms = 1;
  }
  else {
    if (waterLevelDropConfirms < 255) {
      ++waterLevelDropConfirms;
    }
    if (waterLevelDropConfirms >= irrigationConfig.waterLevelHysteresis) {
      commitWaterLevel(newLevel);
      committed = true;
      waterLevelDropCandidate = 0xFF;
      waterLevelDropConfirms = 0;
    }
  }

  // Push raw sensor snapshot on any change so the user can see chatter independently
  // of the debounced level. commitWaterLevel already pushed, so skip the duplicate.
  if (strcmp(waterLevelRaw, waterLevelRawLastBroadcast) != 0) {
    strcpy(waterLevelRawLastBroadcast, waterLevelRaw);
    if (!committed) {
      updateStatusClients(STATUS_UPDATE_WATERLEVEL);
    }
  }

}

void addWaterLevelStatus(JsonDocument &doc) {

  doc["waterLevel"] = waterLevelLabel(waterLevel);
  doc["waterLevelRaw"] = waterLevelRaw;
  doc["waterLevelDropConfirms"] = waterLevelDropConfirms;

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
