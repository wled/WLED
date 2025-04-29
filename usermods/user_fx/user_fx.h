#pragma once
#include "wled.h"

class UserFxUsermod : public Usermod {
 private:
 public:
  void setup();

  void loop();

  uint16_t getId();
};
