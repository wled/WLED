#pragma once

typedef enum SyncMode: uint8_t {
  All=0,
  SinDrift=1,
  Pulse=2,
  Swing=3,
  SwingDrift=4,
} SyncMode;

typedef enum Duration: uint8_t {
  ExtraShortDuration=0,
  ShortDuration=10,
  MediumDuration=20,
  LongDuration=30,
  ExtraLongDuration=40,
} Duration;

typedef enum Energy: uint8_t {
  Boring=0,  // a "boring" pattern is slow or whatever but -needs- effects to be interesting
  Chill=10,   // A "chill" pattern is only slow fades, no flashes
  MediumEnergy=20,
  HighEnergy=230
} Energy;


typedef struct ControlParameters {
  ControlParameters(Duration d=MediumDuration, Energy e=Chill) : duration(d), energy(e) {};

  public:
    Duration duration;
    Energy energy;
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
    EffectParameters(EffectMode e=None, PenMode p=Draw, BeatPulse b=Beat, uint8_t c=255) :
      effect(e),
      pen(p),
      beat(b),
      chance(c)
      { };

    EffectMode effect;
    PenMode pen=Draw;
    BeatPulse beat=Beat;
    uint8_t chance=255;
};
