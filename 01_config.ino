#define CREDENTIALS_PATH "/credentials.json"
#define CONFIG_PATH "/config.json"

char *configError = NULL;

IrrigationConfig irrigationConfig;
WifiConfig wifiConfig;
Area *areas;
uint8_t numberOfAreas;
Cycle *cycles;
uint8_t numberOfCycles;
Valve *valves;
uint8_t numberOfValves;

char* copyString(const char *source) {
  if (source == NULL) {
    return NULL;
  }
  uint8_t length = strlen(source);
  char *result = new char[length + 1];
  strncpy(result, source, length);
  result[length] = 0;
  return result;
}

bool isValidTime(uint16_t time) {
  if (time > 2359) {
    return false;
  }
  uint8_t hour = time % 100;
  if (hour > 59) {
    return false;
  }
  return true;
}

void listDir(const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = LittleFS.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.println(F("Failed to open directory"));
    return;
  }

  File entry = root.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      Serial.printf("  DIR: %s\n", entry.name());
      if (levels) {
        listDir(entry.path(), levels - 1);
      }
    } else {
      const char *name = entry.name();
      if (strncmp(name, "tmp_", 4) == 0) {
        Serial.printf("  CLEARING: %s\tSIZE: %u\n", name, entry.size());
        String path = entry.path();
        entry.close();
        LittleFS.remove(path.c_str());
      } else {
        Serial.printf("  FILE: %s\tSIZE: %u\n", name, entry.size());
      }
    }
    entry = root.openNextFile();
  }
}

bool readCredentialsFile(File credentialsFile) {

  JsonDocument doc;
  DeserializationError jsonError = deserializeJson(doc, credentialsFile);
  if (jsonError) {
    Serial.printf("JSON deserialization error: %s\n", jsonError.c_str());
    return false;
  }

  //serializeJsonPretty(doc, Serial);
  //Serial.println();

  // WIFI

  JsonObject docWifiConfig = doc["wifi"];
  if (docWifiConfig.isNull()) {
    setError("Credentials JSON has no or empty section 'wifi'!");
    Serial.println(error);
    return false;
  }
  const char *docSsid = docWifiConfig["ssid"];
  if ((docSsid == NULL) || (strlen(docSsid) == 0)) {
    setError("Credentials JSON has no or empty value 'wifi.ssid'!");
    Serial.println(error);
    return false;
  }
  wifiConfig.ssid = copyString(docSsid);
  const char *docWifiPwd = docWifiConfig["password"];
  if ((docWifiPwd == NULL) || (strlen(docWifiPwd) == 0)) {
    setError("Credentials JSON has no or empty value 'wifi.password'!");
    Serial.println(error);
    return false;
  }
  wifiConfig.password = copyString(docWifiPwd);

  // HTTP

  JsonObject docHttpConfig = doc["http"];
  const char *docHttpUsername = docHttpConfig["username"];
  if ((docHttpUsername == NULL) || (strlen(docHttpUsername) == 0)) {
    wifiConfig.httpUsername = NULL;
  } else {
    wifiConfig.httpUsername = copyString(docHttpUsername);
  }
  const char *docHttpPassword = docHttpConfig["password"];
  if ((docHttpPassword == NULL) || (strlen(docHttpPassword) == 0)) {
    wifiConfig.httpPassword = NULL;
  } else {
    wifiConfig.httpPassword = copyString(docHttpPassword);
  }

  return true;

}



bool readConfigFile(File configFile) {

  JsonDocument doc;
  DeserializationError jsonError = deserializeJson(doc, configFile);
  if (jsonError) {
    Serial.printf("JSON deserialization error: %s\n", jsonError.c_str());
    return false;
  }

  //serializeJsonPretty(doc, Serial);
  //Serial.println();

  // WIFI

  JsonObject docWifiConfig = doc["wifi"];
  if (!docWifiConfig.isNull()) {
    wifiConfig.channel = docWifiConfig["channel"];
  }
  wifiConfig.mac = NULL;

  // PUMPS

  JsonObject docPumpsConfig = doc["pumps"];
  if (docPumpsConfig.isNull()) {
    setError("Config JSON has no or empty section 'pumps'!");
    return false;
  }
  JsonObject docWellPumpConfig = docPumpsConfig["well"];
  if (docWellPumpConfig.isNull()) {
    setError("Config JSON has no or empty section 'pumps.well'!");
    return false;
  }
  JsonObject docWellPumpCycleConfig = docWellPumpConfig["cycle"];
  if (docWellPumpConfig.isNull()) {
    setError("Config JSON has no or empty section 'pumps.well.cycle'!");
    return false;
  }
  irrigationConfig.wellPumpCycleOn = docWellPumpCycleConfig["on"];
  if (irrigationConfig.wellPumpCycleOn == 0) {
    setError("Config JSON has no or empty value 'pumps.well.cycle.on'!");
    return false;
  }
  irrigationConfig.wellPumpCycleOff = docWellPumpCycleConfig["off"];
  if (irrigationConfig.wellPumpCycleOff == 0) {
    setError("Config JSON has no or empty value 'pumps.well.cycle.off'!");
    return false;
  }
  JsonObject docIrrigationPumpConfig = docPumpsConfig["irrigation"];
  if (docIrrigationPumpConfig.isNull()) {
    setError("Config JSON has no or empty section 'pumps.irrigation'!");
    return false;
  }
  irrigationConfig.irrigationPumpHysteresis = docIrrigationPumpConfig["hysteresis"];
  if (irrigationConfig.irrigationPumpHysteresis == 0) {
    setError("Config JSON has no or empty value 'pumps.irrigation.hysteresis'!");
    return false;
  }

  // WATER

  JsonObject docWaterConfig = doc["water"];
  if (docPumpsConfig.isNull()) {
    setError("Config JSON has no or empty section 'water'!");
    return false;
  }
  JsonObject docWaterLevelConfig = docWaterConfig["level"];
  if (docWaterLevelConfig.isNull()) {
    setError("Config JSON has no or empty section 'water.level'!");
    return false;
  }
  irrigationConfig.waterLevelHysteresis = docWaterLevelConfig["hysteresis"];
  if (irrigationConfig.waterLevelHysteresis == 0) {
    setError("Config JSON has no or empty value 'water.level.hysteresis'!");
    return false;
  }
  JsonObject docPressureConfig = docWaterConfig["pressure"];
  if (docPressureConfig.isNull()) {
    setError("Config JSON has no or empty section 'water.pressure'!");
    return false;
  }
  float lowInBar = docPressureConfig["low"];
  if (lowInBar == 0) {
    setError("Config JSON has no or empty value 'water.pressure.low'!");
    return false;
  }
  float highInBar = docPressureConfig["high"];
  if (highInBar == 0) {
    setError("Config JSON has no or empty value 'water.pressure.high'!");
    return false;
  }
  JsonObject docPressureReferenceConfig = docPressureConfig["reference"];
  if (docPressureReferenceConfig.isNull()) {
    setError("Config JSON has no or empty section 'water.pressure.reference'!");
    return false;
  }
  JsonObject docPressureReferenceLowConfig = docPressureReferenceConfig["low"];
  if (docPressureReferenceLowConfig.isNull()) {
    setError("Config JSON has no or empty section 'water.pressure.reference.low'!");
    return false;
  }
  float lowReferenceBar = docPressureReferenceLowConfig["bar"];
  if (lowReferenceBar == 0) {
    setError("Config JSON has no or empty value 'water.pressure.reference.low.bar'!");
    return false;
  }
  uint16_t lowReferenceAdc = docPressureReferenceLowConfig["value"];
  if (lowReferenceAdc == 0) {
    setError("Config JSON has no or empty value 'water.pressure.reference.low.value'!");
    return false;
  }
  JsonObject docPressureReferenceHighConfig = docPressureReferenceConfig["high"];
  if (docPressureReferenceLowConfig.isNull()) {
    setError("Config JSON has no or empty section 'water.pressure.reference.high'!");
    return false;
  }
  float highReferenceBar = docPressureReferenceHighConfig["bar"];
  if (highReferenceBar == 0) {
    setError("Config JSON has no or empty value 'water.pressure.reference.high.bar'!");
    return false;
  }
  uint16_t highReferenceAdc = docPressureReferenceHighConfig["value"];
  if (highReferenceAdc == 0) {
    setError("Config JSON has no or empty value 'water.pressure.reference.high.value'!");
    return false;
  }
  if (lowReferenceBar >= highReferenceBar) {
    setError("Config JSON value 'water.pressure.reference.low.bar' is higher or equal 'water.pressure.reference.high.bar'!");
    return false;
  }
  if (lowReferenceAdc >= highReferenceAdc) {
    setError("Config JSON value 'water.pressure.reference.low.value' is higher or equal 'water.pressure.reference.high.value'!");
    return false;
  }
  irrigationConfig.pressureAdcGradient = (highReferenceAdc - lowReferenceAdc) / (highReferenceBar - lowReferenceBar);
  if (lowReferenceBar == 0) {
    irrigationConfig.pressureAdcOffset = lowReferenceAdc;
  } else {
    irrigationConfig.pressureAdcOffset = lowReferenceAdc - (lowReferenceBar * irrigationConfig.pressureAdcGradient);
  }
  irrigationConfig.waterPressureLow = irrigationConfig.pressureAdcOffset + (lowInBar * irrigationConfig.pressureAdcGradient);
  irrigationConfig.waterPressureHigh = irrigationConfig.pressureAdcOffset + (highInBar * irrigationConfig.pressureAdcGradient);

  // VALVES

  JsonArray docValves = doc["valves"];
  numberOfValves = docValves.size();
  if (docValves.isNull() || (numberOfValves == 0)) {
    setError("Config JSON has no or empty array 'valves'!");
    return false;
  }
  valves = new Valve[numberOfValves];
  uint8_t i;
  for (i = 0; i < numberOfValves; ++i) {
    JsonObject docValve = docValves[i];
    if (docValve.isNull()) {
      setError("Config JSON has valve at index %d defined as null!\n", i);
      return false;
    }
    Valve &valve = valves[i];
    valve.index = i;
    valve.active = false;
    valve.on = false;
    valve.mode = VALVE_MODE_AUTO;

    const char *id = docValve["id"];
    if (id == NULL) {
      char buffer[4];
      itoa(i, buffer, 10);
      valve.id = copyString(buffer);
    } else {
      valve.id = copyString(id);
    }

    const char *remoteUrl = docValve["remote"];
    if (remoteUrl != NULL) {
      valve.url = copyString(remoteUrl);
    } else {
      uint8_t gpio = docValve["gpio"];
      valve.gpio = gpio;
      valve.url = NULL;
    }
  }

  // AREAS

  JsonObject docAreas = doc["areas"];
  numberOfAreas = docAreas.size();
  if (docAreas.isNull() || (numberOfAreas == 0)) {
    setError("Config JSON has no or empty section 'areas'!");
    return false;
  }
  i = 0;
  areas = new Area[numberOfAreas];
  for (JsonPair docAreaPair : docAreas) {
    Area &area = areas[i];

    area.name = copyString(docAreaPair.key().c_str());
    Serial.printf("Area %s\n", areas[i].name);
    JsonObject docArea = docAreaPair.value();
    area.resetOnActivation = docArea["reset"];
    area.irrigatedPeriod = 0;

    JsonArray docSequence = docArea["sequence"];
    area.sizeOfSequence = docSequence.size();
    area.totalTimeOfSequences = 0;
    if (docSequence.isNull() || (area.sizeOfSequence == 0)) {
      setError("Config JSON has area '%s' with no or emtpy sequence array!\n", area.name);
      return false;
    }
    area.sequence = new Sequence[area.sizeOfSequence];
    for (uint8_t j = 0; j < area.sizeOfSequence; ++j) {
      JsonObject docSequenceItem = docSequence[j];
      if (docSequenceItem.isNull()) {
        setError("Config JSON has area '%s' with null sequence item at index %d!\n", area.name, j);
        return false;
      }
      Sequence &sequence = area.sequence[j];

      sequence.duration = docSequenceItem["duration"];
      if ((sequence.duration == 0) || (sequence.duration > 100)) {
        setError("Config JSON has area '%s' with sequence item at index %d with 0 duration or duration greater than 100!\n", area.name, j);
        return false;
      }
      area.totalTimeOfSequences += sequence.duration;

      JsonArray docValves = docSequenceItem["valves"];
      sequence.numberOfValves = docValves.size();
      if (docValves.isNull() || (sequence.numberOfValves == 0)) {
        setError("Config JSON has area '%s' with sequence item at index %d with null or empty 'valves' attribute!\n", area.name, j);
        return false;
      }
      sequence.valves = new Valve*[sequence.numberOfValves];

      for (uint8_t k = 0; k < sequence.numberOfValves; ++k) {
        const char *docValve = docValves[k];
        sequence.valves[k] = NULL;
        for (uint8_t l = 0; l < numberOfValves; ++l) {
          if (strcmp(docValve, valves[l].id) == 0) {
            sequence.valves[k] = &valves[l];
          }
        }
        if (sequence.valves[k] == NULL) {
          setError("Config JSON has area '%s' with sequence item at index %d with uknown valve in 'valves' attribute %s!\n", area.name, j, docValve);
          return false;
        }
      }
    }
    ++i;
  }

  // CYCLES

  JsonArray docCycles = doc["cycles"];
  numberOfCycles = docCycles.size();
  // allow empty cycles - only using manual control
  /*
  if (docCycles.isNull() || (numberOfCycles == 0)) {
    setError("Config JSON has no or empty section 'cycles'!");
    return false;
  }
  */
  cycles = new Cycle[numberOfCycles];
  i = 0;
  for (i = 0; i < numberOfCycles; ++i) {
    JsonObject docCycle = docCycles[i];
    if (docCycle.isNull()) {
      setError("Config JSON has cycle at index %d defined as null!\n", i);
      return false;
    }
    Cycle &cycle = cycles[i];

    const char *docArea = docCycle["area"];
    cycle.area = NULL;
    for (uint8_t j = 0; j < numberOfAreas; ++j) {
      if (strcmp(docArea, areas[j].name) == 0) {
        cycle.area = &areas[j];
      }
    }
    if (cycle.area == NULL) {
      setError("Config JSON has cycle at index %d using an area '{}' not found in areas section!\n", i, docArea);
      return false;
    }

    const char *docStart = docCycle["start"];
    uint time;
    int r = sscanf(docStart, "%0u", &time);
    if ((r == 0) || !isValidTime(time)) {
      setError("Config JSON has cycle at index %d using an invalid start '{}'!\n", i, docStart);
      return false;
    }
    cycle.start = time;

    const char *docEnd = docCycle["end"];
    r = sscanf(docEnd, "%0u", &time);
    if ((r == 0) || !isValidTime(time)) {
      setError("Config JSON has cycle at index %d using an invalid end '{}'!\n", i, docEnd);
      return false;
    }
    cycle.end = time;
  }

  return true;

}

bool readConfiguration() {

  listDir("/", 3);

  File credentialsFile = LittleFS.open(CREDENTIALS_PATH, "r");
  if (!credentialsFile || !credentialsFile.available()) {
    Serial.printf("Failed to open `%s` for reading\n", CREDENTIALS_PATH);
    return false;
  }

  bool credentialsFileSucessfullyRead = readCredentialsFile(credentialsFile);
  credentialsFile.close();
  if (!credentialsFileSucessfullyRead) {
    return false;
  }
  Serial.printf("Successfully read `%s`\n", CREDENTIALS_PATH);

  File configFile = LittleFS.open(CONFIG_PATH, "r");
  if (!configFile || !configFile.available()) {
    Serial.printf("Failed to open `%s` for reading\n", CONFIG_PATH);
    return false;
  }

  bool configFileSucessfullyRead = readConfigFile(configFile);
  configFile.close();
  if (!configFileSucessfullyRead) {
    return false;
  }
  Serial.printf("Successfully read `%s`\n", CONFIG_PATH);

  return true;

}
