#define GPIO_PUMP_IRRIGATION 8 // GPB0
#define GPIO_PUMP_IRRIGATION_LED 5 // GPA5

bool irrigationPumpEnabled = false;
bool irrigationPumpActive = false;

void setupIrrigationPump() {

  portExpander.pinMode(GPIO_PUMP_IRRIGATION_LED, OUTPUT);

  portExpander.pinMode(GPIO_PUMP_IRRIGATION, OUTPUT);
  portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_OFF);

}

void controlIrrigationPump() {

  if (irrigationPumpEnabled) {
    
    if (irrigationPumpActive && isWaterPressureHigh()) {

      irrigationPumpActive = false;
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_OFF); // turn off irrigation pump
      updateStatusClients(STATUS_UPDATE_IRRIGATIONPUMP);
      Serial.println(F("Switched off irrigation pump because water-pressure beyond upper boundary"));

    } else if (!irrigationPumpActive && isWaterPressureLow()) {

      irrigationPumpActive = true;
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_ON); // turn on irrigation pump
      updateStatusClients(STATUS_UPDATE_IRRIGATIONPUMP);
      Serial.println(F("Switched on irrigation pump because water-pressure below lower boundary"));
    }

  } else if (irrigationPumpActive) {

    irrigationPumpActive = false;
    //irrigationPumpHysteresis = PUMP_IRRIGATION_HYSTERESIS;
    portExpander.digitalWrite(GPIO_PUMP_IRRIGATION, RELAIS_OFF); // turn off irrigation pump
    updateStatusClients(STATUS_UPDATE_IRRIGATIONPUMP);
    Serial.println(F("Switched off irrigation pump because no water left in container"));

  }

}

void blinkIrrigationPumpLed() {

  if (irrigationPumpActive) {
    if (interval >> 2 == 0) { // blinking slow
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION_LED, HIGH);
    } else {
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION_LED, LOW);
    }
  } else if (!irrigationPumpEnabled) {
    if (interval % 2 == 0) { // blinking fast
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION_LED, HIGH);
    } else {
      portExpander.digitalWrite(GPIO_PUMP_IRRIGATION_LED, LOW);
    }
  } else {
    portExpander.digitalWrite(GPIO_PUMP_IRRIGATION_LED, LOW);
  }

}
