// see https://forum.arduino.cc/t/how-to-get-one-pointer-for-all-identical-string/1086116/16
#define FF(x) ((__FlashStringHelper*) x)

struct WifiConfig {
  char *ssid;
  char *password;
  uint8_t channel;
  char *mac;
  char *httpUsername;
  char *httpPassword;
};

struct IrrigationConfig {
  uint16_t irrigationPumpHysteresis; // seconds
  uint16_t wellPumpCycleOn;   // minutes
  uint16_t wellPumpCycleOff;  // minutes
  uint16_t wellPumpCycleOverfill; // seconds (0 = disabled)
  // Daylight gating for the well pump in AUTO mode: pump only fills the
  // containers during the productive PV window. daylightEnabled is implicit —
  // set true iff the optional "location" section parses successfully.
  bool daylightEnabled;
  float locationLatitude;             // decimal degrees, north positive
  float locationLongitude;            // decimal degrees, east positive
  uint16_t wellPumpDaylightStartOffsetMin; // minutes after sunrise
  uint16_t wellPumpDaylightEndOffsetMin;   // minutes before sunset
  uint16_t waterLevelHysteresis; // seconds
  uint16_t waterPressureLow;  // ADC value
  uint16_t waterPressureHigh; // ADC value
  uint16_t pressureAdcOffset; // ADC value of zero pressure
  float pressureAdcGradient;  // ADC value gradient for 1 bar
};

#define VALVE_MODE_AUTO 0
#define VALVE_MODE_ON   1
#define VALVE_MODE_OFF  2

struct Valve {
  uint8_t gpio;
  char *url;
  char *id;
  uint8_t index;
  bool active;
  bool on;
  uint8_t mode;
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
  bool aborted;                    // true while inside its time window after a water-shortage abort; prevents re-activation until window ends
};
