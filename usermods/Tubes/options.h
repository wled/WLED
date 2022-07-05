#pragma once

typedef enum SyncMode {
  All=0,
  SinDrift=1,
  Pulse=2,
  Swing=3,
  SwingDrift=4,
} SyncMode;

typedef enum Duration: uint8_t {
  ShortDuration=0,
  MediumDuration=10,
  LongDuration=20,
  ExtraLongDuration=30,
} Duration;

typedef enum Energy: uint8_t {
  LowEnergy=0,
  MediumEnergy=10,
  HighEnergy=20,
} Energy;

typedef struct ControlParameters {
  public:
    Duration duration=MediumDuration;
    Energy energy=LowEnergy;

  ControlParameters(Duration duration=MediumDuration, Energy energy=LowEnergy) {
    this->duration=duration;
    this->energy=energy;
  };

} ControlParams;

typedef enum PenMode: uint8_t {
  Draw=0,
  Erase=1,
  Blend=2,
  Invert=3,
  White=4,
  Black=5,
  Brighten=6,
  Darken=7,
  Flicker=8,
} PenMode;

typedef enum EffectMode: uint8_t {
  None=0,
  Glitter=1,
  Bubble=2,
  Beatbox1=3,
  Beatbox2=4,
  Spark=5,
  Flash=6,
} EffectMode;

typedef enum BeatPulse: uint8_t {
  Continuous=0,
  Eighth=1,
  Quarter=2,
  Half=4,
  Beat=8,
  TwoBeats=16,
  Measure=32,
  TwoMeasures=64,
  Phrase=128,
} BeatPulse;

class EffectParameters {
  public:
    EffectMode effect;
    PenMode pen=Draw;
    BeatPulse beat=Beat;
    uint8_t chance=255;

  EffectParameters(EffectMode effect=None, PenMode pen=Draw, BeatPulse beat=Beat, uint8_t chance=255) {
    this->effect=effect;
    this->pen=pen;
    this->beat=beat;
    this->chance=chance;
  };
};
