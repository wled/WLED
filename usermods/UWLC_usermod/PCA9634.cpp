//
//    FILE: PCA9634.cpp
//  AUTHOR: Rob Tillaart
//    DATE: 2022-01-03
// VERSION: 0.4.0
// PURPOSE: Arduino library for PCA9634 I2C LED driver
//     URL: https://github.com/RobTillaart/PCA9634


#include "PCA9634.h"


//////////////////////////////////////////////////////////////
//
//  Constructor
//
PCA9634::PCA9634(const uint8_t deviceAddress, TwoWire *wire)
{
  _address         = deviceAddress;
  _wire            = wire;
  _channelCount    = 8;
  _error           = PCA963X_OK;
  _OutputEnablePin = 255;
}


bool PCA9634::begin(uint8_t mode1_mask, uint8_t mode2_mask)
{
  if (! isConnected()) return false;
  configure(mode1_mask, mode2_mask);
  return true;
}


bool PCA9634::isConnected()
{
  _wire->beginTransmission(_address);
  _error = _wire->endTransmission();
  return (_error == 0);
}


uint8_t PCA9634::getAddress()
{
  return _address;
}


/////////////////////////////////////////////////////
//
//  CONFIGURATION
//
uint8_t PCA9634::configure(uint8_t mode1_mask, uint8_t mode2_mask)
{
  _error = PCA963X_OK;

  uint8_t r1 = setMode1(mode1_mask);
  uint8_t r2 = setMode2(mode2_mask);

  if ((r1 != PCA963X_OK) || (r2 != PCA963X_OK))
  {
    return PCA963X_ERROR;
  }
  return _error;
}


uint8_t PCA9634::channelCount()
{
  return _channelCount;
}


/////////////////////////////////////////////////////
//
//  LED DRIVER MODE
//
uint8_t PCA9634::setLedDriverMode(uint8_t channel, uint8_t mode)
{
  if (channel >= _channelCount)
  {
    _error  = PCA963X_ERR_CHAN;
    return _error;
  }
  if (mode > 3)
  {
    _error  = PCA963X_ERR_MODE;
    return _error;
  }

  uint8_t reg = PCA963X_LEDOUT_BASE + (channel >> 2);
  //  some bit magic
  uint8_t shift = (channel & 0x03) * 2;  // 0,2,4,6 places
  uint8_t setmask = mode << shift;
  uint8_t clrmask = ~(0x03 << shift);
  uint8_t value = (readReg(reg) & clrmask) | setmask;
  writeReg(reg, value);
  _error = PCA963X_OK;
  return _error;
}


//  returns 0..3 if OK, other values indicate an error
uint8_t PCA9634::getLedDriverMode(uint8_t channel)
{
  if (channel >= _channelCount)
  {
    _error  = PCA963X_ERR_CHAN;
    return _error;
  }

  uint8_t reg = PCA963X_LEDOUT_BASE + (channel >> 2);
  uint8_t shift = (channel & 0x03) * 2;  //  0, 2, 4, 6 places
  uint8_t value = (readReg(reg) >> shift ) & 0x03;
  _error = PCA963X_OK;
  return value;
}


/////////////////////////////////////////////////////
//
//  WRITE MODE
//
uint8_t PCA9634::writeMode(uint8_t reg, uint8_t value)
{
  if ((reg == PCA963X_MODE1) || (reg == PCA963X_MODE2))
  {
    writeReg(reg, value);
    return PCA963X_OK;
  }
  _error = PCA963X_ERR_REG;
  return _error;
}


//  Note 0xFF can also mean an error....  ==>  check error flag
uint8_t PCA9634::readMode(uint8_t reg)
{
  if ((reg == PCA963X_MODE1) || (reg == PCA963X_MODE2))
  {
    _error = PCA963X_OK;
    uint8_t value = readReg(reg);
    return value;
  }
  _error = PCA963X_ERR_REG;
  return _error;
}


uint8_t PCA9634::setMode1(uint8_t value)
{
  return writeMode(PCA963X_MODE1, value);
}


uint8_t PCA9634::setMode2(uint8_t value)
{
  return writeMode(PCA963X_MODE2, value);
}


uint8_t PCA9634::getMode1()
{
  return readMode(PCA963X_MODE1);
}


uint8_t PCA9634::getMode2()
{
  return readMode(PCA963X_MODE2);
}


/////////////////////////////////////////////////////
//
//  GROUP PWM
//
void PCA9634::setGroupPWM(uint8_t value)
{
  writeReg(PCA963X_GRPPWM, value);
}


uint8_t PCA9634::getGroupPWM()
{
  return readReg(PCA963X_GRPPWM);
}


void PCA9634::setGroupFREQ(uint8_t value)
{
  writeReg(PCA963X_GRPFREQ, value);
}


uint8_t PCA9634::getGroupFREQ()
{
  return readReg(PCA963X_GRPFREQ);
}


/////////////////////////////////////////////////////
//
//  WRITE
//
//  write value to single PWM registers
uint8_t PCA9634::write1(uint8_t channel, uint8_t value)
{
  return writeN(channel, &value, 1);
}


//  write three values in consecutive PWM registers
//  typically for RGB values
uint8_t PCA9634::write3(uint8_t channel, uint8_t R, uint8_t G, uint8_t B)
{
  uint8_t arr[3] = { R, G, B };
  return writeN(channel, arr, 3);
}


//  write count values in consecutive PWM registers
//  checks if [channel + count - 1 > 8]
uint8_t PCA9634::writeN(uint8_t channel, uint8_t* arr, uint8_t count)
{
  if (channel + count > _channelCount)
  {
    _error = PCA963X_ERR_CHAN;
    return PCA963X_ERROR;
  }
  uint8_t base = PCA963X_PWM(channel);
  _wire->beginTransmission(_address);
  _wire->write(base);
  for(uint8_t i = 0; i < count; i++)
  {
    _wire->write(arr[i]);
  }
  _error = _wire->endTransmission();
  if (_error != 0)
  {
    _error = PCA963X_ERR_I2C;
    return PCA963X_ERROR;
  }
  _error = PCA963X_OK;
  return _error;
}


uint8_t PCA9634::writeN_noStop(uint8_t channel, uint8_t* arr, uint8_t count)
{
  if (channel + count > _channelCount)
  {
    _error = PCA963X_ERR_WRITE;
    return PCA963X_ERROR;
  }
  uint8_t base = PCA963X_PWM(channel);
  _wire->beginTransmission(_address);
  _wire->write(base);
  for(uint8_t i = 0; i < count; i++)
  {
    _wire->write(arr[i]);
  }
  //  OK so far
  return PCA963X_OK;
}


uint8_t PCA9634::writeStop()
{
  _error = _wire->endTransmission();
  if (_error != 0)
  {
    _error = PCA963X_ERR_I2C;
    return PCA963X_ERROR;
  }
  return PCA963X_OK;
}


/////////////////////////////////////////////////////
//
//  ERROR
//
int PCA9634::lastError()
{
  int e = _error;
  _error = PCA963X_OK;
  return e;
}


/////////////////////////////////////////////////////
//
//  SUB CALL  -   ALL CALL
//
bool PCA9634::enableSubCall(uint8_t nr)
{
  if ((nr == 0) || (nr > 3)) return false;
  uint8_t prev = getMode1();
  uint8_t mask = prev;
  if (nr == 1)      mask |= PCA963X_MODE1_SUB1;
  else if (nr == 2) mask |= PCA963X_MODE1_SUB2;
  else              mask |= PCA963X_MODE1_SUB3;
  //  only update if changed.
  if (mask != prev)
  {
    setMode1(mask);
    //  TODO error handling ...
  }
  return true;
}


bool PCA9634::disableSubCall(uint8_t nr)
{
  if ((nr == 0) || (nr > 3)) return false;
  uint8_t prev = getMode1();
  uint8_t mask = prev;
  if (nr == 1)      mask &= ~PCA963X_MODE1_SUB1;
  else if (nr == 2) mask &= ~PCA963X_MODE1_SUB2;
  else              mask &= ~PCA963X_MODE1_SUB3;
  //  only update if changed.
  if (mask != prev)
  {
    setMode1(mask);
    //  TODO error handling ...
  }
  return true;
}


bool PCA9634::isEnabledSubCall(uint8_t nr)
{
  if ((nr == 0) || (nr > 3)) return false;
  uint8_t mask = getMode1();
  if (nr == 1) return (mask & PCA963X_MODE1_SUB1) > 0;
  if (nr == 2) return (mask & PCA963X_MODE1_SUB2) > 0;
  return (mask & PCA963X_MODE1_SUB3) > 0;
}


bool PCA9634::setSubCallAddress(uint8_t nr, uint8_t address)
{
  if ((nr == 0) || (nr > 3))
  {
    //  _error = ??  TODO
    return false;
  }
  writeReg(PCA963X_SUBADR(nr), address);
  return true;
}


uint8_t PCA9634::getSubCallAddress(uint8_t nr)
{
  if ((nr == 0) || (nr > 3))
  {
    //  _error = ??  TODO
    return 0;
  }
  uint8_t address = readReg(PCA963X_SUBADR(nr));
  return address;
}


bool PCA9634::enableAllCall()
{
  uint8_t prev = getMode1();
  uint8_t mask = prev | PCA963X_MODE1_ALLCALL;
  //  only update if changed.
  if (mask != prev)
  {
    setMode1(mask);
    //  error handling TODO
  }
  return true;
}


bool PCA9634::disableAllCall()
{
  uint8_t prev = getMode1();
  uint8_t mask = prev & ~PCA963X_MODE1_ALLCALL;
  //  only update if changed.
  if (mask != prev)
  {
    setMode1(mask);
    //  error handling TODO
  }
  return true;
}


bool PCA9634::isEnabledAllCall()
{
  uint8_t mask = getMode1();
  return mask & PCA963X_MODE1_ALLCALL;
}


bool PCA9634::setAllCallAddress(uint8_t address)
{
  writeReg(PCA963X_ALLCALLADR, address);
  return true;
}


uint8_t PCA9634::getAllCallAddress()
{
  uint8_t address = readReg(PCA963X_ALLCALLADR);
  return address;
}


/////////////////////////////////////////////////////
//
//  OUTPUT ENABLE (OE) control
//
//  active LOW see page 5 par 6.2 datasheet
//
bool PCA9634::setOutputEnablePin(uint8_t pin)
{
  _OutputEnablePin = pin;
  if (_OutputEnablePin != 255)
  {
    pinMode(_OutputEnablePin, OUTPUT);
    digitalWrite(_OutputEnablePin, HIGH);
    return true;
  }
  //  must it be set to HIGH now?
  return false;
}


bool PCA9634::setOutputEnable(bool on)
{
  if (_OutputEnablePin != 255)
  {
    digitalWrite(_OutputEnablePin, on ? LOW : HIGH);
    return true;
  }
  return false;
}


uint8_t PCA9634::getOutputEnable()
{
  if (_OutputEnablePin != 255)
  {
    return digitalRead(_OutputEnablePin);
  }
  return HIGH;
}


//////////////////////////////////////////////////////
//
//  EXPERIMENTAL
//
int PCA9634::I2C_SoftwareReset(uint8_t method)
{
  //  only support 0 and 1
  if (method > 1) return -999;
  if (method == 1)
  {
    //  from https://github.com/RobTillaart/PCA9634/issues/10#issuecomment-1206326417
   const uint8_t SW_RESET = 0x03;
   _wire->beginTransmission(SW_RESET);
   _wire->write(0xA5);
   _wire->write(0x5A);
   return _wire->endTransmission(true);
  }

  //  default - based upon NXP specification - UM10204.pdf - page 16
  _wire->beginTransmission(0x00);
  _wire->write(0x06);
  return _wire->endTransmission(true);
}


///////////////////////////////////////////////////////////////
//
//  EXPERIMENTAL LEDOUT
//
uint8_t PCA9634::writeLedOut(uint8_t reg, uint8_t mask)
{
  if (reg > 1) return PCA963X_ERROR;
  writeReg(PCA963X_LEDOUT_BASE + reg, mask);
  return PCA963X_OK;
}


uint8_t PCA9634::readLedOut(uint8_t reg)
{
  if (reg > 1) return 0x00;
  return readReg(PCA963X_LEDOUT_BASE + reg);
}


//  TODO move to right section after testing.
uint8_t PCA9634::setLedDriverMode(uint8_t mode)
{
  if (mode > 3) return PCA963X_ERR_MODE;
  uint8_t mask = 0b00000000;
  switch(mode)
  {
    case PCA963X_LEDGRPPWM:
      mask = 0b11111111;
      break;
    case PCA963X_LEDPWM:
      mask = 0b10101010;
      break;
    case PCA963X_LEDON:
      mask = 0b01010101;
      break;
    default:
      mask = 0b00000000;
      break;
  }
  for (int reg = 0; reg < 2; reg++)
  {
    writeLedOut(reg, mask);
  }
  return PCA963X_OK;
}


/////////////////////////////////////////////////////
//
//  PRIVATE
//
uint8_t PCA9634::writeReg(uint8_t reg, uint8_t value)
{
  _wire->beginTransmission(_address);
  _wire->write(reg);
  _wire->write(value);
  _error = _wire->endTransmission();

  if (_error == 0) _error = PCA963X_OK;
  else _error = PCA963X_ERR_I2C;
  return _error;
}


uint8_t PCA9634::readReg(uint8_t reg)
{
  _wire->beginTransmission(_address);
  _wire->write(reg);
  _error = _wire->endTransmission();

  if (_wire->requestFrom(_address, (uint8_t)1) != 1)
  {
    _error = PCA963X_ERROR;
    return 0;
  }
  _error = PCA963X_OK;
  return _wire->read();
}


//  -- END OF FILE --