#include "fseq_player.h"
#include "usermod_fseq.h"
#include "wled.h"
#include <Arduino.h>

#ifdef WLED_USE_SD_SPI
#include <SD.h>
#include <SPI.h>
#elif defined(WLED_USE_SD_MMC)
#include "SD_MMC.h"
#endif

// Static member definitions moved from header to avoid multiple definition
// errors
const char UsermodFseq::_name[] PROGMEM = "usermod FSEQ sd card";

#ifdef WLED_USE_SD_SPI
int8_t UsermodFseq::configPinSourceSelect = 5;
int8_t UsermodFseq::configPinSourceClock = 18;
int8_t UsermodFseq::configPinPoci = 19;
int8_t UsermodFseq::configPinPico = 23;
#endif

File FSEQPlayer::recordingFile;
String FSEQPlayer::currentFileName = "";
float FSEQPlayer::secondsElapsed = 0;

uint8_t FSEQPlayer::colorChannels = 3;
int32_t FSEQPlayer::recordingRepeats = RECORDING_REPEAT_DEFAULT;
uint32_t FSEQPlayer::now = 0;
uint32_t FSEQPlayer::next_time = 0;
uint16_t FSEQPlayer::playbackLedStart = 0;
uint16_t FSEQPlayer::playbackLedStop = uint16_t(-1);
uint32_t FSEQPlayer::frame = 0;
uint16_t FSEQPlayer::buffer_size = 48;
FSEQPlayer::FileHeader FSEQPlayer::file_header;

inline uint32_t FSEQPlayer::readUInt32() {
  char buffer[4];
  if (recordingFile.readBytes(buffer, 4) < 4)
    return 0;
  return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
}

inline uint32_t FSEQPlayer::readUInt24() {
  char buffer[3];
  if (recordingFile.readBytes(buffer, 3) < 3)
    return 0;
  return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16);
}

inline uint16_t FSEQPlayer::readUInt16() {
  char buffer[2];
  if (recordingFile.readBytes(buffer, 2) < 2)
    return 0;
  return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

inline uint8_t FSEQPlayer::readUInt8() {
  char buffer[1];
  if (recordingFile.readBytes(buffer, 1) < 1)
    return 0;
  return (uint8_t)buffer[0];
}

bool FSEQPlayer::fileOnSD(const char *filepath) {
  uint8_t cardType = SD_ADAPTER.cardType();
  if (cardType == CARD_NONE)
    return false;
  return SD_ADAPTER.exists(filepath);
}

bool FSEQPlayer::fileOnFS(const char *filepath) { return false; }

void FSEQPlayer::printHeaderInfo() {
  DEBUG_PRINTLN("FSEQ file header:");
  DEBUG_PRINTF(" channel_data_offset = %d\n", file_header.channel_data_offset);
  DEBUG_PRINTF(" minor_version       = %d\n", file_header.minor_version);
  DEBUG_PRINTF(" major_version       = %d\n", file_header.major_version);
  DEBUG_PRINTF(" header_length       = %d\n", file_header.header_length);
  DEBUG_PRINTF(" channel_count       = %d\n", file_header.channel_count);
  DEBUG_PRINTF(" frame_count         = %d\n", file_header.frame_count);
  DEBUG_PRINTF(" step_time           = %d\n", file_header.step_time);
  DEBUG_PRINTF(" flags               = %d\n", file_header.flags);
}

void FSEQPlayer::processFrameData() {
  uint16_t packetLength = file_header.channel_count;
  uint16_t lastLed =
      min(playbackLedStop, uint16_t(playbackLedStart + (packetLength / 3)));
  char frame_data[buffer_size];
  CRGB *crgb = reinterpret_cast<CRGB *>(frame_data);
  uint16_t bytes_remaining = packetLength;
  uint16_t index = playbackLedStart;
  while (index < lastLed && bytes_remaining > 0) {
    uint16_t length = min(bytes_remaining, buffer_size);
    recordingFile.readBytes(frame_data, length);
    bytes_remaining -= length;
    for (uint16_t offset = 0; offset < length / 3; offset++) {
      setRealtimePixel(index, crgb[offset].r, crgb[offset].g, crgb[offset].b,
                       0);
      if (++index > lastLed)
        break;
    }
  }
  strip.show();
  realtimeLock(3000, REALTIME_MODE_FSEQ);
  next_time = now + file_header.step_time;
}

bool FSEQPlayer::stopBecauseAtTheEnd() {
  if (!recordingFile.available()) {
    if (recordingRepeats == RECORDING_REPEAT_LOOP) {
      // Reset file pointer and frame counter for continuous loop
      recordingFile.seek(0);
      frame = 0;
    } else if (recordingRepeats > 0) {
      recordingFile.seek(0);
      recordingRepeats--;
      frame = 0;
      DEBUG_PRINTF("Repeat recording again for: %d\n", recordingRepeats);
    } else {
      DEBUG_PRINTLN("Finished playing recording, disabling realtime mode");
      realtimeLock(10, REALTIME_MODE_INACTIVE);
      recordingFile.close();
      clearLastPlayback();
      return true;
    }
  }
  return false;
}

void FSEQPlayer::playNextRecordingFrame() {
  if (stopBecauseAtTheEnd())
    return;
  uint32_t offset = file_header.channel_count * frame++;
  offset += file_header.channel_data_offset;
  if (!recordingFile.seek(offset)) {
    if (recordingFile.position() != offset) {
      DEBUG_PRINTLN("Failed to seek to proper offset for channel data!");
      return;
    }
  }
  processFrameData();
}

void FSEQPlayer::handlePlayRecording() {
  now = millis();
  if (realtimeMode != REALTIME_MODE_FSEQ)
    return;
  if (now < next_time)
    return;
  playNextRecordingFrame();
}

void FSEQPlayer::loadRecording(const char *filepath, uint16_t startLed,
                               uint16_t stopLed, float secondsElapsed) {
  if (recordingFile.available()) {
    clearLastPlayback();
    recordingFile.close();
  }
  playbackLedStart = startLed;
  playbackLedStop = stopLed;
  if (playbackLedStart == uint16_t(-1) || playbackLedStop == uint16_t(-1)) {
    Segment sg = strip.getSegment(-1);
    playbackLedStart = sg.start;
    playbackLedStop = sg.stop;
  }
  DEBUG_PRINTF("FSEQ load animation on LED %d to %d\n", playbackLedStart,
               playbackLedStop);
  if (fileOnSD(filepath)) {
    DEBUG_PRINTF("Read file from SD: %s\n", filepath);
    recordingFile = SD_ADAPTER.open(filepath, "rb");
    currentFileName = String(filepath);
    if (currentFileName.startsWith("/"))
      currentFileName = currentFileName.substring(1);
  } else if (fileOnFS(filepath)) {
    DEBUG_PRINTF("Read file from FS: %s\n", filepath);
  } else {
    DEBUG_PRINTF("File %s not found (%s)\n", filepath,
                 USED_STORAGE_FILESYSTEMS);
    return;
  }
  if ((uint64_t)recordingFile.available() < sizeof(file_header)) {
    DEBUG_PRINTF("Invalid file size: %d\n", recordingFile.available());
    recordingFile.close();
    return;
  }
  for (int i = 0; i < 4; i++) {
    file_header.identifier[i] = readUInt8();
  }
  file_header.channel_data_offset = readUInt16();
  file_header.minor_version = readUInt8();
  file_header.major_version = readUInt8();
  file_header.header_length = readUInt16();
  file_header.channel_count = readUInt32();
  file_header.frame_count = readUInt32();
  file_header.step_time = readUInt8();
  file_header.flags = readUInt8();
  printHeaderInfo();
  if (file_header.identifier[0] != 'P' || file_header.identifier[1] != 'S' ||
      file_header.identifier[2] != 'E' || file_header.identifier[3] != 'Q') {
    DEBUG_PRINTF("Error reading FSEQ file %s header, invalid identifier\n",
                 filepath);
    recordingFile.close();
    return;
  }
  if (((uint64_t)file_header.channel_count *
       (uint64_t)file_header.frame_count) +
          file_header.header_length >
      UINT32_MAX) {
    DEBUG_PRINTF("Error reading FSEQ file %s header, file too long (max 4gb)\n",
                 filepath);
    recordingFile.close();
    return;
  }
  if (file_header.step_time < 1) {
    DEBUG_PRINTF("Invalid step time %d, using default %d instead\n",
                 file_header.step_time, FSEQ_DEFAULT_STEP_TIME);
    file_header.step_time = FSEQ_DEFAULT_STEP_TIME;
  }
  if (realtimeOverride == REALTIME_OVERRIDE_ONCE) {
    realtimeOverride = REALTIME_OVERRIDE_NONE;
  }
  frame = (uint32_t)((secondsElapsed * 1000.0f) / file_header.step_time);
  if (frame >= file_header.frame_count) {
    frame = file_header.frame_count - 1;
  }
  // Set loop mode if secondsElapsed is exactly 1.0f
  if (secondsElapsed == 1.0f) {
    recordingRepeats = RECORDING_REPEAT_LOOP;
  } else {
    recordingRepeats = RECORDING_REPEAT_DEFAULT;
  }
  playNextRecordingFrame();
  playNextRecordingFrame();
}

void FSEQPlayer::clearLastPlayback() {
  for (uint16_t i = playbackLedStart; i < playbackLedStop; i++) {
    setRealtimePixel(i, 0, 0, 0, 0);
  }
  if (recordingFile)
    recordingFile.close();
  frame = 0;
  currentFileName = "";
}

bool FSEQPlayer::isPlaying() {
  return recordingFile && recordingFile.available();
}

String FSEQPlayer::getFileName() { return currentFileName; }

float FSEQPlayer::getElapsedSeconds() {
  if (!isPlaying())
    return 0;
  // Calculate approximate elapsed seconds based on frame and step time
  // Or if secondsElapsed is updated elsewhere, return it.
  // Ideally secondsElapsed should be updated during playback.
  // But for now, let's just calculate it from frame count
  return (float)frame * (float)file_header.step_time / 1000.0f;
}

void FSEQPlayer::syncPlayback(float secondsElapsed) {
  if (!isPlaying()) {
    DEBUG_PRINTLN("[FSEQ] Sync: Playback not active, cannot sync.");
    return;
  }

  // Update internal secondsElapsed if we were tracking it
  // FSEQPlayer::secondsElapsed = secondsElapsed; // If we were tracking it

  uint32_t expectedFrame =
      (uint32_t)((secondsElapsed * 1000.0f) / file_header.step_time);
  int32_t diff = (int32_t)expectedFrame - (int32_t)frame;

  if (abs(diff) > 2) {
    frame = expectedFrame;
    uint32_t offset =
        file_header.channel_data_offset + file_header.channel_count * frame;
    if (recordingFile.seek(offset)) {
      DEBUG_PRINTF("[FSEQ] Sync: Adjusted frame to %lu (diff=%ld)\n",
                   expectedFrame, diff);
    } else {
      DEBUG_PRINTLN("[FSEQ] Sync: Failed to seek to new frame");
    }
  } else {
    DEBUG_PRINTF("[FSEQ] Sync: No adjustment needed (current frame: %lu, "
                 "expected: %lu)\n",
                 frame, expectedFrame);
  }
}