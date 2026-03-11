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
const char UsermodFseq::_name[] PROGMEM = "FSEQ";

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
  uint8_t buffer[4];
  if (recordingFile.read(buffer, 4) != 4)
    return 0;
  return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
}

inline uint32_t FSEQPlayer::readUInt24() {
  uint8_t buffer[3];
  if (recordingFile.read(buffer, 3) != 3)
    return 0;
  return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16);
}

inline uint16_t FSEQPlayer::readUInt16() {
  uint8_t buffer[2];
  if (recordingFile.read(buffer, 2) != 2)
    return 0;
  return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

inline uint8_t FSEQPlayer::readUInt8() {
  int c = recordingFile.read();
  return (c < 0) ? 0 : (uint8_t)c;
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
  uint32_t packetLength = file_header.channel_count;
  uint16_t lastLed =
      min((uint32_t)playbackLedStop, (uint32_t)playbackLedStart + (packetLength / 3));
    char frame_data[48];   // fixed size; buffer_size is always 48
  CRGB *crgb = reinterpret_cast<CRGB *>(frame_data);
  uint32_t bytes_remaining = packetLength;
  uint16_t index = playbackLedStart;
  while (index < lastLed && bytes_remaining > 0) {
    uint16_t length = (uint16_t)min(bytes_remaining, (uint32_t)sizeof(frame_data));
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

  // If we reached the last frame
  if (frame >= file_header.frame_count) {

    if (recordingRepeats == RECORDING_REPEAT_LOOP) {
      frame = 0;
      recordingFile.seek(file_header.channel_data_offset);
      return false;
    }

    if (recordingRepeats > 0) {
      recordingRepeats--;
      frame = 0;
      recordingFile.seek(file_header.channel_data_offset);
      DEBUG_PRINTF("Repeat recording again for: %d\n", recordingRepeats);
      return false;
    }

    DEBUG_PRINTLN("Finished playing recording, disabling realtime mode");
    realtimeLock(10, REALTIME_MODE_INACTIVE);
    clearLastPlayback();
    return true;
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

void FSEQPlayer::loadRecording(const char *filepath,
                               uint16_t startLed,
                               uint16_t stopLed,
                               float secondsElapsed,
                               bool loop)
{
  if (recordingFile.available()) {
    clearLastPlayback();
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
	recordingFile = WLED_FS.open(filepath, "rb");
    currentFileName = String(filepath);
    if (currentFileName.startsWith("/"))
      currentFileName = currentFileName.substring(1);
  } else {
    DEBUG_PRINTF("File %s not found (%s)\n", filepath);
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
  recordingRepeats = loop
    ? RECORDING_REPEAT_LOOP
    : RECORDING_REPEAT_DEFAULT;
	
  playNextRecordingFrame();
  //playNextRecordingFrame();
}

void FSEQPlayer::clearLastPlayback() {
  for (uint16_t i = playbackLedStart; i < playbackLedStop; i++) {
    setRealtimePixel(i, 0, 0, 0, 0);
  }
  frame = 0;
  recordingFile.close();
  currentFileName = "";
}

bool FSEQPlayer::isPlaying() {
  return recordingFile && frame < file_header.frame_count;
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

  uint32_t expectedFrame =
      (uint32_t)((secondsElapsed * 1000.0f) / file_header.step_time);

  int32_t diff = (int32_t)expectedFrame - (int32_t)frame;

  // -------------------------------
  // Hard Resync
  // -------------------------------
  if (abs(diff) > 30) {

    frame = expectedFrame;

    uint32_t offset =
        file_header.channel_data_offset +
        (uint32_t)file_header.channel_count * frame;

    if (recordingFile.seek(offset)) {
      DEBUG_PRINTF("[FSEQ] HARD Sync -> frame=%lu (diff=%ld)\n",
                   expectedFrame, diff);
    } else {
      DEBUG_PRINTLN("[FSEQ] HARD Sync failed to seek");
    }

    return;
  }

  // -----------------------------------------
  // Soft Sync
  // -----------------------------------------
  if (abs(diff) > 1) {

    // Proportionaler Faktor wächst mit Drift
    float correctionFactor = 0.05f * abs(diff);

    // Begrenzen damit es nicht aggressiv wird
    correctionFactor = constrain(correctionFactor, 0.05f, 0.4f);

    int32_t timeAdjustment =
        (int32_t)(diff * file_header.step_time * correctionFactor);

    next_time -= timeAdjustment;

    DEBUG_PRINTF(
        "[FSEQ] Soft Sync diff=%ld factor=%.3f adjust=%ldus\n",
        diff,
        correctionFactor,
        timeAdjustment
    );

  } else {

    DEBUG_PRINTF(
        "[FSEQ] Sync OK (current=%lu expected=%lu)\n",
        frame,
        expectedFrame
    );
  }
}
