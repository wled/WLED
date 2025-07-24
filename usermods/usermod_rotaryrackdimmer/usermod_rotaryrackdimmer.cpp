#include "wled.h"
#include "../usermods/usermod_rotaryrackdimmer/usermod_rotaryrackdimmer.h"

Usermod* usermod_rotary = nullptr;

void userSetup() {
  usermod_rotary = new Usermod_RotaryRackDimmer();
  usermods.add(usermod_rotary);
  Serial.println(F("[RotaryRackDimmer] usermod geladen!"));
}

void userConnected() {
  // Optioneel
}

void userLoop() {
  // Optioneel, laat usermod zelf zijn werk doen
}

void Usermod_RotaryRackDimmer::addToJsonInfo(JsonObject &root) {
  JsonObject user = root["usermod"];
  user["RotaryRackDimmer"] = "Actief";
}
