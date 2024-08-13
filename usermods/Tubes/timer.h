#pragma once

class GlobalTimer {
  public:
    uint32_t now_millis;
    uint32_t now_micros;
    uint32_t last_micros;
    uint32_t last_millis;
    uint32_t delta_micros;
    uint32_t delta_millis;

  void setup()
  {
    last_millis = now_millis = millis();
    last_micros = now_micros = micros();
  }

  void update()
  {
    last_millis = now_millis;
    now_millis = millis();
    delta_millis = now_millis - last_millis;
    
    last_micros = now_micros;
    now_micros = micros();
    delta_micros = now_micros - last_micros;
  }
};

GlobalTimer globalTimer;



class Timer {
  public:
    uint32_t markTime;

  void start(uint32_t duration_ms) {
    markTime = globalTimer.now_millis + duration_ms;
  }

  void stop() {
    start(0);
  }

  uint32_t since_mark() const {
    if (globalTimer.now_millis < markTime)
      return 0;
    return globalTimer.now_millis - markTime;
  }

  void snooze(uint32_t duration_ms) {
    while (markTime < globalTimer.now_millis)
      markTime += duration_ms;
  }

  bool ended() const {
    return globalTimer.now_millis > markTime;
  }

  bool every(uint32_t duration_ms) {
    if (!ended())
      return 0;
    snooze(duration_ms);
    return 1;
  }
};