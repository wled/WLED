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
    this->last_millis = this->now_millis = millis();
    this->last_micros = this->now_micros = micros();
  }

  void update()
  {
    this->last_millis = this->now_millis;
    this->now_millis = millis();
    this->delta_millis = this->now_millis - this->last_millis;
    
    this->last_micros = this->now_micros;
    this->now_micros = micros();
    this->delta_micros = this->now_micros - this->last_micros;
  }
};

GlobalTimer globalTimer;



class Timer {
  public:
    uint32_t markTime;

  void start(uint32_t duration_ms) {
    this->markTime = globalTimer.now_millis + duration_ms;
  }

  void stop() {
    this->start(0);
  }

  uint32_t since_mark() {
    if (globalTimer.now_millis < this->markTime)
      return 0;
    return globalTimer.now_millis - this->markTime;
  }

  void snooze(uint32_t duration_ms) {
    while (this->markTime < globalTimer.now_millis)
      this->markTime += duration_ms;
  }

  bool ended() {
    return globalTimer.now_millis > this->markTime;
  }

  bool every(uint32_t duration_ms) {
    if (!this->ended())
      return 0;
    this->snooze(duration_ms);
    return 1;
  }
};