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
