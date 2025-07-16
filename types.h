struct WifiConfig {
  char *ssid;
  char *password;
  uint8_t port;
  char *mac;
  char *httpUsername;
  char *httpPassword;
};

struct IrrigationConfig {
  uint16_t irrigationPumpHysteresis; // seconds
  uint16_t wellPumpCycleOn;   // minutes
  uint16_t wellPumpCycleOff;  // minutes
  uint16_t waterLevelHysteresis; // seconds
  uint16_t waterPressureLow;  // ADC value
  uint16_t waterPressureHigh; // ADC value
  uint16_t pressureAdcOffset; // ADC value of zero pressure
  float pressureAdcGradient;  // ADC value gradient for 1 bar
};

struct Valve {
  uint8_t gpio;
  char *url;
  char *id;
  uint8_t index;
  bool active;
};

struct Sequence {
  uint16_t duration;
  Valve **valves;
  uint8_t numberOfValves;
};

struct Area {
  char *name;
  bool resetOnActivation;
  uint16_t irrigatedPeriod;        // tracks how long irrigation happend

  Sequence* sequence;
  uint8_t sizeOfSequence;
  uint16_t totalTimeOfSequences;
};

struct Cycle {
  uint16_t start;
  uint16_t end;
  Area *area;
};
