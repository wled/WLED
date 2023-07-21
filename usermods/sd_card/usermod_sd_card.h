#pragma once

#include "wled.h"

// SD connected via MMC / SPI
#if defined(WLED_USE_SD_MMC)
  #define USED_STORAGE_FILESYSTEMS "SD MMC, LittleFS"
  #define SD_ADAPTER SD_MMC
  #include "SD_MMC.h"
// SD connected via SPI (adjustable via usermod config)
#elif defined(WLED_USE_SD_SPI)
  #define SD_ADAPTER SD
  #define USED_STORAGE_FILESYSTEMS "SD SPI, LittleFS"
  #include "SD.h"
  #include "SPI.h"
#endif

#ifdef WLED_USE_SD_MMC
#elif defined(WLED_USE_SD_SPI)
  SPIClass spiPort = SPIClass(VSPI);
#endif

void listDir( const char * dirname, uint8_t levels);

class UsermodSdCard : public Usermod {
  private:
    bool sdInitDone = false;

    #ifdef WLED_USE_SD_SPI
      int8_t configPinSourceSelect = 16;
      int8_t configPinSourceClock = 14;
      int8_t configPinPoci = 36; // confusing names? Then have a look :)
      int8_t configPinPico = 15; // https://www.oshwa.org/a-resolution-to-redefine-spi-signal-names/

      //acquired and initialize the SPI port
      void init_SD_SPI()
      {
        if(!configSdEnabled) return;
        if(sdInitDone) return;

        PinManagerPinType pins[5] = {
        { configPinSourceSelect, true },
        { configPinSourceClock, true },
        { configPinPoci, false },
        { configPinPico, true }
        };

        if (!pinManager.allocateMultiplePins(pins, 4, PinOwner::UM_SdCard)) {
            DEBUG_PRINTF("[%s] SD (SPI) pin allocation failed!\n", _name);
            sdInitDone = false;
            return;
        }

        bool returnOfInitSD = false;

        #if defined(WLED_USE_SD_SPI)
          spiPort.begin(configPinSourceClock, configPinPoci, configPinPico, configPinSourceSelect);
          returnOfInitSD = SD_ADAPTER.begin(configPinSourceSelect, spiPort);
        #endif

        if(!returnOfInitSD) {
          DEBUG_PRINTF("[%s] SPI begin failed!\n", _name);
          sdInitDone = false;
          return;
        }

        sdInitDone = true;
      }

      //deinitialize the acquired SPI port
      void deinit_SD_SPI()
      {
        if(!sdInitDone) return;

        SD_ADAPTER.end();

        DEBUG_PRINTF("[%s] deallocate pins!\n", _name);
        pinManager.deallocatePin(configPinSourceSelect, PinOwner::UM_SdCard);
        pinManager.deallocatePin(configPinSourceClock,  PinOwner::UM_SdCard);
        pinManager.deallocatePin(configPinPoci,         PinOwner::UM_SdCard);
        pinManager.deallocatePin(configPinPico,         PinOwner::UM_SdCard);

        sdInitDone = false;
      }

      // some SPI pin was changed, while SPI was initialized, reinit to new port
      void reinit_SD_SPI()
      {
          deinit_SD_SPI();
          init_SD_SPI();
      }
    #endif

    #ifdef WLED_USE_SD_MMC
      void init_SD_MMC() {
        if(sdInitDone) return;
        bool returnOfInitSD = false;
        returnOfInitSD = SD_ADAPTER.begin();
        DEBUG_PRINTF("[%s] MMC begin\n", _name);

        if(!returnOfInitSD) {
          DEBUG_PRINTF("[%s] MMC begin failed!\n", _name);
          sdInitDone = false;
          return;
        }

        sdInitDone = true;
      }
    #endif

  public:
    static bool configSdEnabled;
    static const char _name[];

    void setup() {
      DEBUG_PRINTF("[%s] usermod loaded \n", _name);
      #if defined(WLED_USE_SD_SPI)
        init_SD_SPI();
      #elif defined(WLED_USE_SD_MMC)
        init_SD_MMC();
      #endif
 
      #if defined(SD_ADAPTER) && defined(SD_PRINT_HOME_DIR)  
        listDir("/", 0);        
      #endif
    }

    void loop(){
      
    }

    uint16_t getId()
    {
      return USERMOD_ID_SD_CARD;
    }

    void addToConfig(JsonObject& root)
    {
      #ifdef WLED_USE_SD_SPI
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top["pinSourceSelect"] = configPinSourceSelect;
      top["pinSourceClock"] = configPinSourceClock;
      top["pinPoci"] = configPinPoci;
      top["pinPico"] = configPinPico;
      top["sdEnabled"] = configSdEnabled;
      #endif
    }

    bool readFromConfig(JsonObject &root)
    {
      #ifdef WLED_USE_SD_SPI
        JsonObject top = root[FPSTR(_name)];
        if (top.isNull()) {
          DEBUG_PRINTF("[%s] No config found. (Using defaults.)\n", _name);
          return false;
        }

        uint8_t oldPinSourceSelect  = configPinSourceSelect;
        uint8_t oldPinSourceClock = configPinSourceClock;
        uint8_t oldPinPoci = configPinPoci;
        uint8_t oldPinPico = configPinPico;
        bool    oldSdEnabled = configSdEnabled;

        getJsonValue(top["pinSourceSelect"], configPinSourceSelect);
        getJsonValue(top["pinSourceClock"],  configPinSourceClock);
        getJsonValue(top["pinPoci"],         configPinPoci);
        getJsonValue(top["pinPico"],         configPinPico);
        getJsonValue(top["sdEnabled"],       configSdEnabled);

        if(configSdEnabled != oldSdEnabled) {
          configSdEnabled ? init_SD_SPI() : deinit_SD_SPI();
          DEBUG_PRINTF("[%s] SD card %s\n", _name, configSdEnabled ? "enabled" : "disabled");
        }

        if( configSdEnabled && (
            oldPinSourceSelect  != configPinSourceSelect ||
            oldPinSourceClock   != configPinSourceClock  ||
            oldPinPoci          != configPinPoci         ||
            oldPinPico          != configPinPico)
          )
        {
          DEBUG_PRINTF("[%s] Init SD card based of config\n", _name);
          DEBUG_PRINTF("[%s] Config changes \n - SS: %d -> %d\n - MI: %d -> %d\n - MO: %d -> %d\n - En: %d -> %d\n", _name, oldPinSourceSelect, configPinSourceSelect, oldPinSourceClock, configPinSourceClock, oldPinPoci, configPinPoci, oldPinPico, configPinPico);
          reinit_SD_SPI();
        }
      #endif

      return true;
    }
};

const char UsermodSdCard::_name[] PROGMEM = "SD Card";
bool UsermodSdCard::configSdEnabled = true;

#ifdef SD_ADAPTER
//checks if the file is available on SD card
bool file_onSD(const char *filepath)
{
  #ifdef WLED_USE_SD_SPI
    if(!UsermodSdCard::configSdEnabled) return false;
  #endif

  uint8_t cardType = SD_ADAPTER.cardType();
  if(cardType == CARD_NONE) {
    DEBUG_PRINTF("[%s] not attached / cardType none\n", UsermodSdCard::_name);
    return false; // no SD card attached
  }
  if(cardType == CARD_MMC || cardType == CARD_SD || cardType == CARD_SDHC)
  {
    return SD_ADAPTER.exists(filepath);
  }

  return false; // unknown card type
}

void listDir( const char * dirname, uint8_t levels){
    DEBUG_PRINTF("Listing directory: %s\n", dirname);

    File root = SD_ADAPTER.open(dirname);
    if(!root){
        DEBUG_PRINTF("Failed to open directory\n");
        return;
    }
    if(!root.isDirectory()){
        DEBUG_PRINTF("Not a directory\n");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            DEBUG_PRINTF("  DIR : %s\n",file.name());
            if(levels){
                listDir(file.name(), levels -1);
            }
        } else {
            DEBUG_PRINTF("  FILE: %s  SIZE: %d\n",file.name(), file.size());
        }
        file = root.openNextFile();
    }
}

#endif