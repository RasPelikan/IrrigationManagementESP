bool *activeCycles = NULL;                 // tracks which cycle is active

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

        Serial.printf_P(PSTR("Deactivating cycle %s\n"), activeCycle->area->name);
        activeCycles[cycleIndex] = false;
        // reset tracking of how long irrigation happend
        if (activeCycle->area->resetOnActivation) {
          activeCycle->area->irrigatedPeriod = 0;
        }

      }

    }
    // cycle needs to be activated
    else if (calculatedCycles[cycleIndex] != NULL) {

      Serial.printf_P(PSTR("Activating cycle %s\n"), calculatedCycles[cycleIndex]->area->name);
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
        Serial.printf_P(PSTR("%04d Activating valve %s\n"), time, valves[valveIndex].id);
        valves[valveIndex].active = true;
      }
    } else {
      if (valves[valveIndex].active) {
        Serial.printf_P(PSTR("%04d Deactivating valve %s\n"), time, valves[valveIndex].id);
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

  portExpander.pinMode(GPIO_VALVE_1, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_1, RELAIS_OFF);
  portExpander.pinMode(GPIO_VALVE_2, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_2, RELAIS_OFF);
  portExpander.pinMode(GPIO_VALVE_3, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_3, RELAIS_OFF);
  portExpander.pinMode(GPIO_VALVE_4, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_4, RELAIS_OFF);
  portExpander.pinMode(GPIO_VALVE_5, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_5, RELAIS_OFF);
  portExpander.pinMode(GPIO_VALVE_6, OUTPUT);
  portExpander.digitalWrite(GPIO_VALVE_6, RELAIS_OFF);

}
