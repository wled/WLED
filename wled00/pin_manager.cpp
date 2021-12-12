#include "pin_manager.h"
#include "wled.h"

static void DebugPrintOwnerTag(PinOwner tag)
{
  uint32_t q = static_cast<uint8_t>(tag);
  if (q) {
    DEBUG_PRINTF("0x%02x (%d)", q, q);
  } else {
    DEBUG_PRINT(F("(no owner)"));
  }
}

/// Actual allocation/deallocation routines
bool PinManagerClass::deallocatePin(byte gpio, PinOwner tag)
{
  if (gpio == 0xFF) return true;           // explicitly allow clients to free -1 as a no-op
  if (!isPinOk(gpio, false)) return false; // but return false for any other invalid pin

  // if a non-zero ownerTag, only allow de-allocation if the owner's tag is provided
  if ((ownerTag[gpio] != PinOwner::None) && (ownerTag[gpio] != tag)) {
    DEBUG_PRINT(F("PIN DEALLOC: IO "));
    DEBUG_PRINT(gpio);
    DEBUG_PRINT(F(" allocated by "));
    DebugPrintOwnerTag(ownerTag[gpio]);
    DEBUG_PRINT(F(", but attempted de-allocation by "));
    DebugPrintOwnerTag(tag);
    return false;
  }
  
  byte by = gpio >> 3;
  byte bi = gpio - 8*by;
  bitWrite(pinAlloc[by], bi, false);
  ownerTag[gpio] = PinOwner::None;
  return true;
}
bool PinManagerClass::allocateMultiplePins(const managed_pin_type * mptArray, byte arrayElementCount, PinOwner tag)
{
  bool shouldFail = false;
  // first verify the pins are OK and not already allocated
  for (int i = 0; i < arrayElementCount; i++) {
    byte gpio = mptArray[i].pin;
    if (gpio == 0xFF) {
      // explicit support for io -1 as a no-op (no allocation of pin),
      // as this can greatly simplify configuration arrays
      continue;
    }
    if (!isPinOk(gpio, mptArray[i].isOutput)) {
      DEBUG_PRINT(F("PIN ALLOC: Invalid pin attempted to be allocated: "));
      DEBUG_PRINT(gpio);
      DEBUG_PRINTLN(F(""));
      shouldFail = true;
    }
    if (isPinAllocated(gpio)) {
      DEBUG_PRINT(F("PIN ALLOC: FAIL: IO ")); 
      DEBUG_PRINT(gpio);
      DEBUG_PRINT(F(" already allocated by "));
      DebugPrintOwnerTag(ownerTag[gpio]);
      DEBUG_PRINTLN(F(""));
      shouldFail = true;
    }
  }
  if (shouldFail) {
    return false;
  }

  // all pins are available .. track each one
  for (int i = 0; i < arrayElementCount; i++) {
    byte gpio = mptArray[i].pin;
    if (gpio == 0xFF) {
      // allow callers to include -1 value as non-requested pin
      // as this can greatly simplify configuration arrays
      continue;
    }
    byte by = gpio >> 3;
    byte bi = gpio - 8*by;
    bitWrite(pinAlloc[by], bi, true);
    ownerTag[gpio] = tag;
  }
  return true;
}
bool PinManagerClass::allocatePin(byte gpio, bool output, PinOwner tag)
{
  if (!isPinOk(gpio, output)) return false;
  if (isPinAllocated(gpio)) {
    DEBUG_PRINT(F("PIN ALLOC: Pin ")); 
    DEBUG_PRINT(gpio);
    DEBUG_PRINT(F(" already allocated by "));
    DebugPrintOwnerTag(ownerTag[gpio]);
    DEBUG_PRINTLN(F(""));
    return false;
  }

  byte by = gpio >> 3;
  byte bi = gpio - 8*by;
  bitWrite(pinAlloc[by], bi, true);
  ownerTag[gpio] = tag;
  
  return true;
}

// if tag is set to PinOwner::None, checks for ANY owner of the pin.
// if tag is set to any other value, checks if that tag is the current owner of the pin.
bool PinManagerClass::isPinAllocated(byte gpio, PinOwner tag)
{
  if (!isPinOk(gpio, false)) return true;
  if ((tag != PinOwner::None) && (ownerTag[gpio] != tag)) return false;
  byte by = gpio >> 3;
  byte bi = gpio - 8*by;
  return bitRead(pinAlloc[by], bi);
}

bool PinManagerClass::isPinOk(byte gpio, bool output)
{
  if (gpio <  6) return  true;
  if (gpio < 12) return false; //SPI flash pins
  
  #ifdef ESP8266
  if (gpio < 17) return true;
  #else //ESP32
  if (gpio < 34) return true;
  if (gpio < 40 && !output) return true; //34-39 input only
  #endif

  return false;
}

PinOwner PinManagerClass::getPinOwner(byte gpio) {
  if (!isPinOk(gpio, false)) return PinOwner::None;
  return ownerTag[gpio];
}

#ifdef ARDUINO_ARCH_ESP32
byte PinManagerClass::allocateLedc(byte channels)
{
  if (channels > 16 || channels == 0) return 255;
  byte ca = 0;
  for (byte i = 0; i < 16; i++) {
    byte by = i >> 3;
    byte bi = i - 8*by;
    if (bitRead(ledcAlloc[by], bi)) { //found occupied pin
      ca = 0;
    } else {
      ca++;
    }
    if (ca >= channels) { //enough free channels
      byte in = (i + 1) - ca;
      for (byte j = 0; j < ca; j++) {
        byte b = in + j;
        byte by = b >> 3;
        byte bi = b - 8*by;
        bitWrite(ledcAlloc[by], bi, true);
      }
      return in;
    }
  }
  return 255; //not enough consecutive free LEDC channels
}

void PinManagerClass::deallocateLedc(byte pos, byte channels)
{
  for (byte j = pos; j < pos + channels; j++) {
    if (j > 16) return;
    byte by = j >> 3;
    byte bi = j - 8*by;
    bitWrite(ledcAlloc[by], bi, false);
  }
}
#endif

PinManagerClass pinManager = PinManagerClass();
