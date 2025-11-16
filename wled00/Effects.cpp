#include "wled.h"
#include "FX.h"
#include <new>

// Namespace Effects: Global effects list
// Construct-on-first-use idiom to avoid static initialization fiasco with WS2812FX
static std::vector<const Effect*>& _globalEffectsList() {
  static std::vector<const Effect*>* _g = new std::vector<const Effect*>{};
  return *_g;
};

// Highest used effect ID
static uint8_t _highestId = 0;

void Effects::reserve(size_t s) { 
  _globalEffectsList().reserve(s);
}

void Effects::addEffect(const Effect* new_effect) {
  if (new_effect) {
    _globalEffectsList().push_back(new_effect);
    auto id = getIdForEffect(new_effect);
    if (id > _highestId) {
      _highestId = id;
    }
  }
}

uint8_t Effects::addEffect(const char *mode_name, effect_function mode_fn, uint8_t id) {
  if (id == 255) { // find next available id
    // Build a quick bitmap of the used IDs
    uint32_t used_ids[8] = {0};
    for(auto& e: _globalEffectsList()) {
      auto id = getIdForEffect(e);
      if (id != 255) {
        // Do math together so the compiler identifies the division+remainder 
        auto chunk = id/32;
        auto bit = id%32;
        used_ids[chunk] |= (1u << bit);
      }
    }
    // Now find the lowest value
    for(auto chunk = 0U; chunk < 8; ++chunk) {
      auto inverted = ~used_ids[chunk];
      if (inverted) {
        // found one!
        id = (chunk*32) + __builtin_ctz(inverted);  // Find lowest set bit with compiler intrinsic
        break;
      }
    }
  }

  addEffect(new(std::nothrow) Effect { mode_name, mode_fn, id });
  return id;
}

const Effect* Effects::getEffectByName(const char* name, size_t len) {
  auto effect_iter = std::find_if(_globalEffectsList().begin(), _globalEffectsList().end(), [=](const Effect* e) { 
      auto match = strncmp_P(e->data, name, len);
      return (match == 0) && ((e->data[len] == '@') || (e->data[len] == 0));
    });
  if (effect_iter == _globalEffectsList().end()) effect_iter = _globalEffectsList().begin(); // set solid mode, always the first element of the list  
  return *effect_iter;  
}

const Effect* Effects::getEffectById(uint8_t id) {
  auto effect_iter = std::find_if(_globalEffectsList().begin(), _globalEffectsList().end(), [=](const Effect* e) { return getIdForEffect(e) == id; });
  if (effect_iter == _globalEffectsList().end()) effect_iter = _globalEffectsList().begin(); // set solid mode, always the first element of the list  
  return *effect_iter;
}

uint8_t Effects::getHighestId() { return _highestId; };

size_t Effects::getCount() { return _globalEffectsList().size(); }
size_t Effects::getCapacity() { return _globalEffectsList().capacity(); }
  
// Range interface: for(auto& effect: Effects.all())
std::vector<const Effect*>::iterator Effects::asRange::begin() { return _globalEffectsList().begin(); }
std::vector<const Effect*>::iterator Effects::asRange::end() { return _globalEffectsList().end(); }


// Effect parsing utilities
String Effect::getName() const {
#ifdef ESP8266
  char lineBuffer[256];
  strncpy_P(lineBuffer, data, sizeof(lineBuffer)/sizeof(char)-1);
  lineBuffer[sizeof(lineBuffer)/sizeof(char)-1] = '\0'; // terminate string
  char* p = strchr(lineBuffer, '@');
  if (p) {
    *p = '\0'; // terminate there
  }
  return String(lineBuffer);
#else
  char* p = strchr(data, '@');
  if (p) {
    return String(data, p - data);
  } else {
    return String(data);
  }
#endif  
}

size_t Effect::getName(char* dest, size_t dest_size) const {
  char lineBuffer[256];
  strncpy_P(lineBuffer, data, sizeof(lineBuffer)/sizeof(char)-1);
  lineBuffer[sizeof(lineBuffer)/sizeof(char)-1] = '\0'; // terminate string
  size_t j = 0;
  for (; j < dest_size; j++) {
    if (lineBuffer[j] == '\0' || lineBuffer[j] == '@') break;
    dest[j] = lineBuffer[j];
  }
  dest[j] = 0; // terminate string
  return strlen(dest);
}
