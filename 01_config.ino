char *configError = NULL;

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

  Dir root = LittleFS.openDir(dirname);
  while (root.next()) {
    if (root.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(root.fileName());
      if (levels) {
        char path[200];
        strcpy(path, dirname);
        if (path[strlen(dirname) - 1] != '/') {
          strcpy(path + strlen(dirname), "/");
        }
        strcpy(path + strlen(path), root.fileName().c_str());
        listDir(path, levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.println(root.fileName());
      Serial.print("\tSIZE: ");
      Serial.println(root.fileSize());
    }
  }
}

bool readConfiguration() {

  listDir("/", 3);

  File file = LittleFS.open(F(CONFIG_PATH), "r");
  if (!file) {

    setError(PSTR("Failed to open `%s` for reading\n"), CONFIG_PATH);
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    setError(PSTR("JSON deserialization error: %s\n"), error.c_str());
    return false;
  }

  JsonArray docValves = doc["valves"];
  numberOfValves = docValves.size();
  if (docValves.isNull() || (numberOfValves == 0)) {
    setError(PSTR("Config JSON has no or empty array 'valves'!"));
    return false;
  }
  valves = new Valve[numberOfValves];
  uint8_t i;
  for (i = 0; i < numberOfValves; ++i) {
    JsonObject docValve = docValves[i];
    if (docValve.isNull()) {
      setError(PSTR("Config JSON has valve at index %d defined as null!\n"), i);
      return false;
    }
    Valve &valve = valves[i];
    valve.index = i;
    valve.active = false;

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

  JsonObject docAreas = doc["areas"];
  numberOfAreas = docAreas.size();
  if (docAreas.isNull() || (numberOfAreas == 0)) {
    setError(PSTR("Config JSON has no or empty section 'areas'!"));
    return false;
  }
  i = 0;
  areas = new Area[numberOfAreas];
  for (JsonPair docAreaPair : docAreas) {
    Area &area = areas[i];

    area.name = copyString(docAreaPair.key().c_str());
    Serial.printf_P(PSTR("Area %s\n"), areas[i].name);
    JsonObject docArea = docAreaPair.value();
    area.resetOnActivation = docArea["reset"];
    area.irrigatedPeriod = 0;

    JsonArray docSequence = docArea["sequence"];
    area.sizeOfSequence = docSequence.size();
    area.totalTimeOfSequences = 0;
    if (docSequence.isNull() || (area.sizeOfSequence == 0)) {
      setError(PSTR("Config JSON has area '%s' with no or emtpy sequence array!\n"), area.name);
      return false;
    }
    area.sequence = new Sequence[area.sizeOfSequence];
    for (uint8_t j = 0; j < area.sizeOfSequence; ++j) {
      JsonObject docSequenceItem = docSequence[j];
      if (docSequenceItem.isNull()) {
        setError(PSTR("Config JSON has area '%s' with null sequence item at index %d!\n"), area.name, j);
        return false;
      }
      Sequence &sequence = area.sequence[j];
      
      sequence.duration = docSequenceItem["duration"];
      if ((sequence.duration == 0) || (sequence.duration > 100)) {
        setError(PSTR("Config JSON has area '%s' with sequence item at index %d with 0 duration or duration greater than 100!\n"), area.name, j);
        return false;
      }
      area.totalTimeOfSequences += sequence.duration;

      JsonArray docValves = docSequenceItem["valves"];
      sequence.numberOfValves = docValves.size();
      if (docValves.isNull() || (sequence.numberOfValves == 0)) {
        setError(PSTR("Config JSON has area '%s' with sequence item at index %d with null or empty 'valves' attribute!\n"), area.name, j);
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
          setError(PSTR("Config JSON has area '%s' with sequence item at index %d with uknown valve in 'valves' attribute %s!\n"), area.name, j, docValve);
          return false;
        }
      }
    }
    ++i;
  }

  JsonArray docCycles = doc["cycles"];
  numberOfCycles = docCycles.size();
  if (docCycles.isNull() || (numberOfCycles == 0)) {
    setError(PSTR("Config JSON has no or empty section 'cycles'!"));
    return false;
  }
  cycles = new Cycle[numberOfCycles];
  i = 0;
  for (i = 0; i < numberOfCycles; ++i) {
    JsonObject docCycle = docCycles[i];
    if (docCycle.isNull()) {
      setError(PSTR("Config JSON has cycle at index %d defined as null!\n"), i);
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
      setError(PSTR("Config JSON has cycle at index %d using an area '{}' not found in areas section!\n"), i, docArea);
      return false;
    }

    const char *docStart = docCycle["start"];
    uint time;
    int r = sscanf(docStart, "%0u", &time);
    if ((r == 0) || !isValidTime(time)) {
      setError(PSTR("Config JSON has cycle at index %d using an invalid start '{}'!\n"), i, docStart);
      return false;
    }
    cycle.start = time;

    const char *docEnd = docCycle["end"];
    r = sscanf(docEnd, "%0u", &time);
    if ((r == 0) || !isValidTime(time)) {
      setError(PSTR("Config JSON has cycle at index %d using an invalid end '{}'!\n"), i, docEnd);
      return false;
    }
    cycle.end = time;
  }

  Serial.println("Successfully read configuration");

  return true;

}

