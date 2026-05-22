#include "fseq_player.h"
#include "usermod_fseq.h"
#include "wled.h"
#include "sd_adapter_compat.h"
#include <Arduino.h>

// Static member definitions
const char UsermodFseq::_name[] PROGMEM = "FSEQ";
uint8_t UsermodFseq::fseqEffectId = 0;

FSEQPlayer::PlaybackState FSEQPlayer::realtimeState;
FSEQPlayer::PlaybackState FSEQPlayer::segmentStates[MAX_NUM_SEGMENTS];

namespace {
// Keep the cache larger than the effect UI range.
// The effect uses c3 (0..31) for easier local selection,
// but FPP/file-name based playback must still be able to
// find more files than that by name.
constexpr uint16_t FSEQ_MAX_INDEXED_FILES = 128;
constexpr uint32_t FSEQ_FPP_OVERRIDE_TIMEOUT_MS = 3000;
constexpr size_t FSEQ_SHARED_FRAME_BUFFER_SIZE = 1024;
constexpr uint32_t FSEQ_SYNC_DEBUG_INTERVAL_MS = 5000;
constexpr uint32_t FSEQ_REALTIME_LOCK_REFRESH_MS = 1000;

uint32_t gLastSoftSyncDebugMs = 0;
uint32_t gLastRealtimeLockMs = 0;

String gIndexedFseqFiles[FSEQ_MAX_INDEXED_FILES];
uint16_t gIndexedFseqFileCount = 0;
bool gIndexedFseqFilesDirty = true;
uint32_t gFppLastControlMs = 0;
bool gFppOverrideActive = false;

// Shared read buffer to avoid large stack allocations.
// Rendering is done sequentially in the main loop, so one shared buffer is fine.
static uint8_t gFseqFrameBuffer[FSEQ_SHARED_FRAME_BUFFER_SIZE];

bool isFseqFileName(const String &name) {
  return name.endsWith(".fseq") || name.endsWith(".FSEQ");
}

int compareFseqName(const String &a, const String &b) {
  String al = a;
  String bl = b;
  al.toLowerCase();
  bl.toLowerCase();
  return al.compareTo(bl);
}

void insertSortedFseqName(const String &name) {
  if (gIndexedFseqFileCount >= FSEQ_MAX_INDEXED_FILES) return;

  uint16_t insertPos = gIndexedFseqFileCount;
  for (uint16_t i = 0; i < gIndexedFseqFileCount; i++) {
    if (compareFseqName(name, gIndexedFseqFiles[i]) < 0) {
      insertPos = i;
      break;
    }
  }

  for (uint16_t i = gIndexedFseqFileCount; i > insertPos; i--) {
    gIndexedFseqFiles[i] = gIndexedFseqFiles[i - 1];
  }

  gIndexedFseqFiles[insertPos] = name;
  gIndexedFseqFileCount++;
}
}  // namespace

void FSEQ_invalidateFileIndexCache() { gIndexedFseqFilesDirty = true; }

uint16_t FSEQ_refreshFileIndexCache() {
  if (!gIndexedFseqFilesDirty) return gIndexedFseqFileCount;

  gIndexedFseqFileCount = 0;

  File root = SD_ADAPTER.open("/");
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return 0;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = file.name();
      if (name.length() > 0 && !name.startsWith("/")) name = "/" + name;
      if (isFseqFileName(name)) insertSortedFseqName(name);
    }

    file.close();
    file = root.openNextFile();
  }

  root.close();
  gIndexedFseqFilesDirty = false;
  return gIndexedFseqFileCount;
}

uint16_t FSEQ_getFileIndexCount() { return FSEQ_refreshFileIndexCache(); }

bool FSEQ_getFileNameByIndex(uint16_t index, String &outName) {
  FSEQ_refreshFileIndexCache();
  if (index >= gIndexedFseqFileCount) {
    outName = "";
    return false;
  }
  outName = gIndexedFseqFiles[index];
  return true;
}

int16_t FSEQ_findFileIndexByName(const String &name) {
  String normalized = name;
  if (!normalized.startsWith("/")) normalized = "/" + normalized;

  FSEQ_refreshFileIndexCache();

  for (uint16_t i = 0; i < gIndexedFseqFileCount; i++) {
    if (gIndexedFseqFiles[i].equalsIgnoreCase(normalized)) {
      return (int16_t)i;
    }
  }

  return -1;
}

void FSEQ_markFppControlActivity() {
  gFppLastControlMs = millis();
  gFppOverrideActive = true;
}

void FSEQ_clearFppOverride() { gFppOverrideActive = false; }

bool FSEQ_isFppOverrideActive() {
  if (gFppOverrideActive &&
      (millis() - gFppLastControlMs > FSEQ_FPP_OVERRIDE_TIMEOUT_MS)) {
    gFppOverrideActive = false;
  }
  return gFppOverrideActive;
}

inline uint32_t FSEQPlayer::readUInt32(File &file) {
  uint8_t buffer[4];
  if (file.read(buffer, 4) != 4) return 0;
  return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
}

inline uint32_t FSEQPlayer::readUInt24(File &file) {
  uint8_t buffer[3];
  if (file.read(buffer, 3) != 3) return 0;
  return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16);
}

inline uint16_t FSEQPlayer::readUInt16(File &file) {
  uint8_t buffer[2];
  if (file.read(buffer, 2) != 2) return 0;
  return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

inline uint8_t FSEQPlayer::readUInt8(File &file) {
  int c = file.read();
  return (c < 0) ? 0 : (uint8_t)c;
}

bool FSEQPlayer::fileOnSD(const char *filepath) {
  uint8_t cardType = SD_ADAPTER.cardType();
  if (cardType == CARD_NONE) return false;
  return SD_ADAPTER.exists(filepath);
}

bool FSEQPlayer::fileOnFS(const char *filepath) { return false; }

void FSEQPlayer::printHeaderInfo(const PlaybackState &state) {
  DEBUG_PRINTLN("FSEQ file header:");
  DEBUG_PRINTF(" channel_data_offset = %d\n", state.file_header.channel_data_offset);
  DEBUG_PRINTF(" minor_version       = %d\n", state.file_header.minor_version);
  DEBUG_PRINTF(" major_version       = %d\n", state.file_header.major_version);
  DEBUG_PRINTF(" header_length       = %d\n", state.file_header.header_length);
  DEBUG_PRINTF(" channel_count       = %lu\n", (unsigned long)state.file_header.channel_count);
  DEBUG_PRINTF(" frame_count         = %lu\n", (unsigned long)state.file_header.frame_count);
  DEBUG_PRINTF(" step_time           = %d\n", state.file_header.step_time);
  DEBUG_PRINTF(" flags               = %d\n", state.file_header.flags);
}

bool FSEQPlayer::ensureFrameDataPosition(PlaybackState &state) {
  if (!state.frame_position_dirty) return true;
  if (!state.recordingFile.seek(state.frame_data_offset)) {
    DEBUG_PRINTLN("[FSEQ] Failed to seek to frame data");
    clearPlaybackState(state);
    return false;
  }
  state.frame_position_dirty = false;
  return true;
}

bool FSEQPlayer::skipRemainingFrameData(PlaybackState &state, uint32_t bytesToSkip) {
  if (bytesToSkip == 0) return true;
  (void)bytesToSkip;

  const uint32_t newOffset = state.frame_data_offset + state.file_header.channel_count;
  if (!state.recordingFile.seek(newOffset)) {
    DEBUG_PRINTLN("[FSEQ] Failed to skip remaining frame data");
    clearPlaybackState(state);
    return false;
  }
  return true;
}

void FSEQPlayer::scheduleNextFrame(PlaybackState &state) {
  const uint32_t step = state.file_header.step_time;
  if (step == 0) {
    state.next_time = 0;
    return;
  }

  state.next_time = state.playback_start_time + ((state.frame + 1U) * step);
}

void FSEQPlayer::resetTimingFromCurrentFrame(PlaybackState &state, uint32_t now) {
  const uint32_t step = state.file_header.step_time;
  if (step == 0) {
    state.playback_start_time = now;
    state.next_time = 0;
    return;
  }

  const uint32_t frameOffsetMs = state.frame * step;
  // Use wrap-safe unsigned timing so late-join can anchor to an elapsed time
  // that is already larger than the local millis() uptime.
  state.playback_start_time = now - frameOffsetMs;
  scheduleNextFrame(state);
}

void FSEQPlayer::alignFrameToLocalTime(PlaybackState &state, uint32_t now) {
  if (!isStatePlaying(state)) return;

  const uint32_t step = state.file_header.step_time;
  if (step == 0) return;
  if (state.file_header.frame_count == 0) return;

  // When we already advanced past the last frame, let stopBecauseAtTheEnd()
  // handle loop/stop logic instead of snapping back to the last frame again.
  if (state.frame >= state.file_header.frame_count) return;

  const uint32_t maxFrame = state.file_header.frame_count - 1U;
  uint32_t localFrame = (uint32_t)(now - state.playback_start_time) / step;
  if (localFrame > maxFrame) localFrame = maxFrame;

  if (localFrame != state.frame) {
    state.frame = localFrame;
    state.frame_data_offset =
        state.file_header.channel_data_offset +
        ((uint32_t)state.file_header.channel_count * state.frame);
    state.frame_position_dirty = true;
  }

  scheduleNextFrame(state);
}

void FSEQPlayer::processFrameDataForSegment(PlaybackState &state, Segment &segment) {
  const uint32_t packetLength = state.file_header.channel_count;
  const uint16_t segLen = segment.length();
  const uint16_t maxLeds = min((uint32_t)segLen, packetLength / 3U);
  const uint32_t bytesToRender = min(packetLength, (uint32_t)maxLeds * 3U);
  uint32_t bytesRemainingToRender = bytesToRender;
  uint16_t index = 0;

  while (bytesRemainingToRender > 0) {
    const uint16_t length =
        (uint16_t)min(bytesRemainingToRender, (uint32_t)FSEQ_SHARED_FRAME_BUFFER_SIZE);

    const size_t bytesRead =
        state.recordingFile.readBytes((char *)gFseqFrameBuffer, length);
    if (bytesRead != length) {
      DEBUG_PRINTF("[FSEQ] Short SD read in segment playback (%u/%u), stopping state\n",
                   (unsigned)bytesRead, (unsigned)length);
      clearPlaybackState(state);
      return;
    }

    bytesRemainingToRender -= length;

    for (uint16_t offset = 0; offset + 2 < length; offset += 3) {
      segment.setPixelColor(
          index,
          RGBW32(gFseqFrameBuffer[offset], gFseqFrameBuffer[offset + 1],
                 gFseqFrameBuffer[offset + 2], 0));
      if (++index >= maxLeds) break;
    }
  }

  for (uint16_t i = index; i < segLen; i++) {
    segment.setPixelColor(i, BLACK);
  }

  if (!skipRemainingFrameData(state, packetLength - bytesToRender)) return;

  state.frame_data_offset += state.file_header.channel_count;
}

static inline void refreshRealtimeLockIfNeeded(uint32_t now) {
  if (gLastRealtimeLockMs == 0 ||
      (uint32_t)(now - gLastRealtimeLockMs) >= FSEQ_REALTIME_LOCK_REFRESH_MS) {
    realtimeLock(3000, REALTIME_MODE_FSEQ);
    gLastRealtimeLockMs = now;
  }
}

void FSEQPlayer::processFrameDataRealtime(PlaybackState &state) {
  const uint32_t packetLength = state.file_header.channel_count;
  const uint16_t totalLen = strip.getLengthTotal();
  const uint16_t maxLeds = min((uint32_t)totalLen, packetLength / 3U);
  const uint32_t bytesToRender = min(packetLength, (uint32_t)maxLeds * 3U);
  uint32_t bytesRemainingToRender = bytesToRender;
  uint16_t index = 0;

  while (bytesRemainingToRender > 0) {
    const uint16_t length =
        (uint16_t)min(bytesRemainingToRender, (uint32_t)FSEQ_SHARED_FRAME_BUFFER_SIZE);

    const size_t bytesRead =
        state.recordingFile.readBytes((char *)gFseqFrameBuffer, length);
    if (bytesRead != length) {
      DEBUG_PRINTF("[FSEQ] Short SD read in realtime playback (%u/%u), stopping state\n",
                   (unsigned)bytesRead, (unsigned)length);
      clearPlaybackState(state);
      return;
    }

    bytesRemainingToRender -= length;

    for (uint16_t offset = 0; offset + 2 < length; offset += 3) {
      setRealtimePixel(index, gFseqFrameBuffer[offset],
                       gFseqFrameBuffer[offset + 1],
                       gFseqFrameBuffer[offset + 2], 0);
      if (++index >= maxLeds) break;
    }
  }

  if (index < totalLen) {
    for (uint16_t i = index; i < totalLen; i++) {
      setRealtimePixel(i, 0, 0, 0, 0);
    }
  }

  if (!skipRemainingFrameData(state, packetLength - bytesToRender)) return;

  refreshRealtimeLockIfNeeded(state.now);
  state.frame_data_offset += state.file_header.channel_count;
}

bool FSEQPlayer::stopBecauseAtTheEnd(PlaybackState &state) {
  if (state.frame >= state.file_header.frame_count) {
    if (state.recordingRepeats == RECORDING_REPEAT_LOOP) {
      state.frame = 0;
      state.frame_data_offset = state.file_header.channel_data_offset;
      state.frame_position_dirty = true;
      state.syncErrorFilteredMs = 0.0f;
      state.syncSlewMs = 0.0f;
      state.syncCarryMs = 0.0f;
      state.playback_start_time = millis();
      scheduleNextFrame(state);
      return false;
    }

    if (state.recordingRepeats > 0) {
      state.recordingRepeats--;
      state.frame = 0;
      state.frame_data_offset = state.file_header.channel_data_offset;
      state.frame_position_dirty = true;
      state.syncErrorFilteredMs = 0.0f;
      state.syncSlewMs = 0.0f;
      state.syncCarryMs = 0.0f;
      state.playback_start_time = millis();
      scheduleNextFrame(state);
      DEBUG_PRINTF("Repeat recording again for: %d\n", state.recordingRepeats);
      return false;
    }

    DEBUG_PRINTLN("Finished playing recording");
    clearPlaybackState(state);
    return true;
  }

  return false;
}


void FSEQPlayer::playNextRecordingFrameForSegment(PlaybackState &state, Segment &segment) {
  if (stopBecauseAtTheEnd(state)) return;
  if (!ensureFrameDataPosition(state)) return;

  processFrameDataForSegment(state, segment);
  if (!isStatePlaying(state)) return;

  state.frame++;
  scheduleNextFrame(state);
}

void FSEQPlayer::playNextRealtimeFrame(PlaybackState &state) {
  if (stopBecauseAtTheEnd(state)) return;
  if (!ensureFrameDataPosition(state)) return;

  processFrameDataRealtime(state);
  if (!isStatePlaying(state)) return;

  state.frame++;
  scheduleNextFrame(state);
}

void FSEQPlayer::loadRecordingIntoState(PlaybackState &state, const char *filepath,
                                        float secondsElapsed, bool loop) {
  clearPlaybackState(state);

  DEBUG_PRINTF("FSEQ load animation: %s\n", filepath);
  if (fileOnSD(filepath)) {
    DEBUG_PRINTF("Read file from SD: %s\n", filepath);
    state.recordingFile = SD_ADAPTER.open(filepath, "rb");
  } else if (fileOnFS(filepath)) {
    DEBUG_PRINTF("Read file from FS: %s\n", filepath);
    state.recordingFile = WLED_FS.open(filepath, "rb");
  } else {
    DEBUG_PRINTF("File %s not found on SD or FS\n", filepath);
    return;
  }

  if (!state.recordingFile) {
    DEBUG_PRINTF("Failed to open %s\n", filepath);
    return;
  }

  state.file_size = (uint32_t)state.recordingFile.size();
  state.currentFileName = String(filepath);
  if (state.currentFileName.startsWith("/")) {
    state.currentFileName = state.currentFileName.substring(1);
  }

  if (state.file_size < sizeof(state.file_header)) {
    DEBUG_PRINTF("Invalid file size: %lu\n", (unsigned long)state.file_size);
    clearPlaybackState(state);
    return;
  }

  for (int i = 0; i < 4; i++) state.file_header.identifier[i] = readUInt8(state.recordingFile);
  state.file_header.channel_data_offset = readUInt16(state.recordingFile);
  state.file_header.minor_version = readUInt8(state.recordingFile);
  state.file_header.major_version = readUInt8(state.recordingFile);
  state.file_header.header_length = readUInt16(state.recordingFile);
  state.file_header.channel_count = readUInt32(state.recordingFile);
  state.file_header.frame_count = readUInt32(state.recordingFile);
  state.file_header.step_time = readUInt8(state.recordingFile);
  state.file_header.flags = readUInt8(state.recordingFile);
  printHeaderInfo(state);

  if (state.file_header.identifier[0] != 'P' || state.file_header.identifier[1] != 'S' ||
      state.file_header.identifier[2] != 'E' || state.file_header.identifier[3] != 'Q') {
    DEBUG_PRINTF("Error reading FSEQ file %s header, invalid identifier\n", filepath);
    clearPlaybackState(state);
    return;
  }

  if (state.file_header.frame_count == 0 || state.file_header.channel_count == 0) {
    DEBUG_PRINTF("Error reading FSEQ file %s header, empty data\n", filepath);
    clearPlaybackState(state);
    return;
  }

  if (state.file_header.header_length > state.file_header.channel_data_offset) {
    DEBUG_PRINTF("Error reading FSEQ file %s header, invalid header offsets\n", filepath);
    clearPlaybackState(state);
    return;
  }

  const uint64_t requiredSize =
      (uint64_t)state.file_header.channel_data_offset +
      ((uint64_t)state.file_header.channel_count * (uint64_t)state.file_header.frame_count);

  if (requiredSize > state.file_size) {
    DEBUG_PRINTF("Error reading FSEQ file %s header, truncated frame data\n", filepath);
    clearPlaybackState(state);
    return;
  }

  if (requiredSize > UINT32_MAX) {
    DEBUG_PRINTF("Error reading FSEQ file %s header, file too long (max 4gb)\n", filepath);
    clearPlaybackState(state);
    return;
  }

  if (state.file_header.step_time < 1) {
    DEBUG_PRINTF("Invalid step time %d, using default %d instead\n",
                 state.file_header.step_time, FSEQ_DEFAULT_STEP_TIME);
    state.file_header.step_time = FSEQ_DEFAULT_STEP_TIME;
  }

  state.recordingRepeats = loop ? RECORDING_REPEAT_LOOP : RECORDING_REPEAT_DEFAULT;

  secondsElapsed = max(0.0f, secondsElapsed);
  const float startMs = secondsElapsed * 1000.0f;
  const float stepMs = (float)state.file_header.step_time;

  state.frame = (uint32_t)(startMs / stepMs);
  if (state.frame >= state.file_header.frame_count) {
    state.frame = state.file_header.frame_count - 1U;
  }

  state.frame_data_offset =
      state.file_header.channel_data_offset +
      (state.file_header.channel_count * state.frame);
  state.frame_position_dirty = true;

  const uint32_t now = millis();
  const uint32_t startMsU32 = (uint32_t)startMs;
  // Wrap-safe anchor: allows joining an already-running sequence even when
  // local uptime is still smaller than the elapsed sequence time.
  state.playback_start_time = now - startMsU32;

  state.secondsElapsed = secondsElapsed;
  if (&state == &realtimeState) gLastRealtimeLockMs = 0;
  state.syncErrorFilteredMs = 0.0f;
  state.syncSlewMs = 0.0f;
  state.syncCarryMs = 0.0f;
  alignFrameToLocalTime(state, now);
  scheduleNextFrame(state);
}

void FSEQPlayer::clearPlaybackState(PlaybackState &state) {
  state.frame = 0;
  state.next_time = 0;
  state.playback_start_time = 0;
  state.now = 0;
  state.secondsElapsed = 0;
  state.recordingRepeats = RECORDING_REPEAT_DEFAULT;
  state.file_size = 0;
  state.frame_data_offset = 0;
  state.frame_position_dirty = true;
  state.file_header = {};
  state.syncErrorFilteredMs = 0.0f;
  state.syncSlewMs = 0.0f;
  state.syncCarryMs = 0.0f;
  if (state.recordingFile) state.recordingFile.close();
  if (&state == &realtimeState) gLastRealtimeLockMs = 0;
  state.currentFileName = "";
}

bool FSEQPlayer::isStatePlaying(const PlaybackState &state) {
  return state.recordingFile && state.file_header.frame_count > 0 &&
         state.frame <= state.file_header.frame_count;
}

void FSEQPlayer::setStateLooping(PlaybackState &state, bool loop) {
  state.recordingRepeats = loop ? RECORDING_REPEAT_LOOP : RECORDING_REPEAT_DEFAULT;
}

float FSEQPlayer::getElapsedSeconds(const PlaybackState &state) {
  if (!isStatePlaying(state)) return 0;

  const uint32_t step = state.file_header.step_time;
  if (step == 0) return 0;

  uint32_t now = millis();
  uint32_t elapsedMs = now - state.playback_start_time;

  const uint32_t maxMs = (state.file_header.frame_count - 1U) * step;
  if (elapsedMs > maxMs) elapsedMs = maxMs;
  return (float)elapsedMs / 1000.0f;
}

void FSEQPlayer::loadRecordingForSegment(uint8_t segmentId, const char *filepath,
                                         float secondsElapsed, bool loop) {
  if (segmentId >= MAX_NUM_SEGMENTS) return;
  loadRecordingIntoState(segmentStates[segmentId], filepath, secondsElapsed, loop);
}

void FSEQPlayer::clearSegmentPlayback(uint8_t segmentId) {
  if (segmentId >= MAX_NUM_SEGMENTS) return;
  clearPlaybackState(segmentStates[segmentId]);
}

void FSEQPlayer::setSegmentLooping(uint8_t segmentId, bool loop) {
  if (segmentId >= MAX_NUM_SEGMENTS) return;
  setStateLooping(segmentStates[segmentId], loop);
}

bool FSEQPlayer::isSegmentPlaying(uint8_t segmentId) {
  if (segmentId >= MAX_NUM_SEGMENTS) return false;
  return isStatePlaying(segmentStates[segmentId]);
}

bool FSEQPlayer::isAnySegmentPlaying() {
  for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++) {
    if (isStatePlaying(segmentStates[i])) return true;
  }
  return false;
}

bool FSEQPlayer::isAnyPlaybackActive() {
  return isStatePlaying(realtimeState) || isAnySegmentPlaying();
}

void FSEQPlayer::renderSegmentFrame(uint8_t segmentId, Segment &segment) {
  if (segmentId >= MAX_NUM_SEGMENTS) return;
  PlaybackState &state = segmentStates[segmentId];
  state.now = millis();
  if (!isStatePlaying(state)) return;

  if (state.frame >= state.file_header.frame_count) {
    if (stopBecauseAtTheEnd(state)) return;
  }

  const bool forceRender = state.frame_position_dirty;
  if (!forceRender && state.next_time != 0 &&
      (int32_t)(state.now - state.next_time) < 0) {
    return;
  }

  alignFrameToLocalTime(state, state.now);
  playNextRecordingFrameForSegment(state, segment);
}

void FSEQPlayer::loadRecording(const char *filepath, float secondsElapsed, bool loop) {
  loadRecordingIntoState(realtimeState, filepath, secondsElapsed, loop);
}

void FSEQPlayer::clearLastPlayback() { clearPlaybackState(realtimeState); }

bool FSEQPlayer::isPlaying() { return isStatePlaying(realtimeState); }

void FSEQPlayer::setLooping(bool loop) { setStateLooping(realtimeState, loop); }

String FSEQPlayer::getFileName() { return realtimeState.currentFileName; }

float FSEQPlayer::getElapsedSeconds() { return getElapsedSeconds(realtimeState); }

void FSEQPlayer::renderRealtimeFrame() {
  PlaybackState &state = realtimeState;
  state.now = millis();
  if (!isStatePlaying(state)) return;

  if (state.frame >= state.file_header.frame_count) {
    if (stopBecauseAtTheEnd(state)) return;
  }

  const bool forceRender = state.frame_position_dirty;
  if (!forceRender && state.next_time != 0 &&
      (int32_t)(state.now - state.next_time) < 0) {
    return;
  }

  alignFrameToLocalTime(state, state.now);
  playNextRealtimeFrame(state);
  if (!useMainSegmentOnly) strip.show();
  else strip.trigger();
}

void FSEQPlayer::syncPlayback(float secondsElapsed) {
  PlaybackState &state = realtimeState;

  if (!isStatePlaying(state)) {
    DEBUG_PRINTLN("[FSEQ] Sync: Playback not active, cannot sync.");
    return;
  }

  if (state.file_header.step_time == 0 || state.file_header.frame_count == 0) {
    DEBUG_PRINTLN("[FSEQ] Sync: Invalid timing info.");
    return;
  }

  const float stepMs = (float)state.file_header.step_time;
  const uint32_t now = millis();

  float targetMs = secondsElapsed * 1000.0f;
  const float maxMs = (float)(state.file_header.frame_count - 1U) * stepMs;
  targetMs = constrain(targetMs, 0.0f, maxMs);
  state.secondsElapsed = targetMs / 1000.0f;

  bool timingAdjusted = false;
  if (state.playback_start_time == 0 && (state.frame != 0 || state.secondsElapsed > 0.0f)) {
    const uint32_t targetMsU32 = (uint32_t)targetMs;
    state.playback_start_time = now - targetMsU32;
    timingAdjusted = true;
  }

  const float localMs = (float)(uint32_t)(now - state.playback_start_time);
  const float errorMs = targetMs - localMs;  // >0 = wir sind hinten

  if (fabsf(errorMs) >= 150.0f) {
    uint32_t targetFrame = (uint32_t)(targetMs / stepMs);
    if (targetFrame >= state.file_header.frame_count) {
      targetFrame = state.file_header.frame_count - 1U;
    }

    state.frame = targetFrame;
    state.frame_data_offset =
        state.file_header.channel_data_offset +
        ((uint32_t)state.file_header.channel_count * state.frame);
    state.frame_position_dirty = true;

    const uint32_t targetMsU32 = (uint32_t)targetMs;
    state.playback_start_time = now - targetMsU32;
    state.syncErrorFilteredMs = 0.0f;
    state.syncSlewMs = 0.0f;
    state.syncCarryMs = 0.0f;
    scheduleNextFrame(state);

    DEBUG_PRINTF("[FSEQ] HARD Sync -> frame=%lu err=%.2fms\n",
                 (unsigned long)targetFrame, errorMs);
    return;
  }

  if (fabsf(errorMs) < 1.0f) {
    state.syncErrorFilteredMs *= 0.92f;
    state.syncSlewMs *= 0.85f;
    return;
  }

  state.syncErrorFilteredMs =
      state.syncErrorFilteredMs * 0.92f + errorMs * 0.08f;

  float desiredSlewMs = state.syncErrorFilteredMs * 0.02f;
  desiredSlewMs = constrain(desiredSlewMs, -0.75f, 0.75f);

  state.syncSlewMs = state.syncSlewMs * 0.85f + desiredSlewMs * 0.15f;
  state.syncCarryMs += state.syncSlewMs;

  int32_t adjustMs = 0;
  if (state.syncCarryMs >= 1.0f) {
    adjustMs = (int32_t)floorf(state.syncCarryMs);
  } else if (state.syncCarryMs <= -1.0f) {
    adjustMs = (int32_t)ceilf(state.syncCarryMs);
  }
  state.syncCarryMs -= (float)adjustMs;

  if (adjustMs != 0) {
    state.playback_start_time = (uint32_t)(state.playback_start_time - adjustMs);
    timingAdjusted = true;
  }

  if (timingAdjusted) {
    alignFrameToLocalTime(state, now);
  }

  if ((uint32_t)(now - gLastSoftSyncDebugMs) >= FSEQ_SYNC_DEBUG_INTERVAL_MS) {
    gLastSoftSyncDebugMs = now;
    DEBUG_PRINTF("[FSEQ] Soft Sync err=%.2fms filt=%.2fms slew=%.3fms adj=%ldms\n",
                 errorMs,
                 state.syncErrorFilteredMs,
                 state.syncSlewMs,
                 (long)adjustMs);
  }
}

