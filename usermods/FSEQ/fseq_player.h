#ifndef FSEQ_PLAYER_H
#define FSEQ_PLAYER_H

#ifndef RECORDING_REPEAT_LOOP
#define RECORDING_REPEAT_LOOP -1
#endif
#ifndef RECORDING_REPEAT_DEFAULT
#define RECORDING_REPEAT_DEFAULT 0
#endif
#ifndef REALTIME_MODE_FSEQ
#define REALTIME_MODE_FSEQ 3
#endif

#include "wled.h"
#ifdef WLED_USE_SD_SPI
#include <SD.h>
#include <SPI.h>
#elif defined(WLED_USE_SD_MMC)
#include "SD_MMC.h"
#endif

class FSEQPlayer {
public:
  struct FileHeader {
    uint8_t identifier[4];
    uint16_t channel_data_offset;
    uint8_t minor_version;
    uint8_t major_version;
    uint16_t header_length;
    uint32_t channel_count;
    uint32_t frame_count;
    uint8_t step_time;
    uint8_t flags;
  };

  static void loadRecording(const char *filepath, uint16_t startLed,
                            uint16_t stopLed, float secondsElapsed = 0.0f);
  static void handlePlayRecording();
  static void clearLastPlayback();
  static void syncPlayback(float secondsElapsed);
  static bool isPlaying();
  static String getFileName();
  static float getElapsedSeconds();

private:
  FSEQPlayer() {}

  static const int FSEQ_DEFAULT_STEP_TIME = 50;

  static File recordingFile;
  static String currentFileName;
  static float secondsElapsed;
  static uint8_t colorChannels;
  static int32_t recordingRepeats;
  static uint32_t now;
  static uint32_t next_time;
  static uint16_t playbackLedStart;
  static uint16_t playbackLedStop;
  static uint32_t frame;
  static uint16_t buffer_size;
  static FileHeader file_header;

  static inline uint32_t readUInt32();
  static inline uint32_t readUInt24();
  static inline uint16_t readUInt16();
  static inline uint8_t readUInt8();

  static bool fileOnSD(const char *filepath);
  static bool fileOnFS(const char *filepath);
  static void printHeaderInfo();
  static void processFrameData();
  static bool stopBecauseAtTheEnd();
  static void playNextRecordingFrame();
};

#endif // FSEQ_PLAYER_H