#pragma once

#include "wled.h"
#include "fseq_player.h"

uint16_t FSEQ_getFileIndexCount();
bool FSEQ_getFileNameByIndex(uint16_t index, String &outName);
bool FSEQ_isFppOverrideActive();
bool FPP_sendEffectSyncMessage(uint8_t action, const String &fileName,
                               uint32_t currentFrame, float secondsElapsed,
                               bool sendMulticast, bool sendBroadcast);

static uint16_t _fseq_lastIndex[MAX_NUM_SEGMENTS];
static bool _fseq_lastLoop[MAX_NUM_SEGMENTS];
static uint16_t _fseq_lastFileCount[MAX_NUM_SEGMENTS];
static bool _fseq_stateInit = false;
static uint32_t _fseq_syncStartMs[MAX_NUM_SEGMENTS];
static uint32_t _fseq_lastSyncMs[MAX_NUM_SEGMENTS];
static bool _fseq_wasPlaying[MAX_NUM_SEGMENTS];
static bool _fseq_wasSyncing[MAX_NUM_SEGMENTS];
static bool _fseq_lastSendMulticast[MAX_NUM_SEGMENTS];
static bool _fseq_lastSendBroadcast[MAX_NUM_SEGMENTS];

static constexpr uint32_t FSEQ_EFFECT_SYNC_INTERVAL_MS = 500;

static void resetFseqSyncState(uint8_t segId) {
  _fseq_syncStartMs[segId] = 0;
  _fseq_lastSyncMs[segId] = 0;
  _fseq_wasPlaying[segId] = false;
  _fseq_wasSyncing[segId] = false;
  _fseq_lastSendMulticast[segId] = false;
  _fseq_lastSendBroadcast[segId] = false;
}

static void stopFseqSyncIfNeeded(uint8_t segId, const String &fileName = String()) {
  if (!_fseq_wasSyncing[segId]) return;

  FPP_sendEffectSyncMessage(1, fileName, 0, 0.0f,
                            _fseq_lastSendMulticast[segId],
                            _fseq_lastSendBroadcast[segId]);
  resetFseqSyncState(segId);
}

static void mode_fseq_player(void) {
  if (!_fseq_stateInit) {
    for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++) {
      _fseq_lastIndex[i] = 0xFFFF;
      _fseq_lastLoop[i] = false;
      _fseq_lastFileCount[i] = 0xFFFF;
      resetFseqSyncState(i);
    }
    _fseq_stateInit = true;
  }

  const uint8_t segId = strip.getCurrSegmentId();

  // While FPP is active, local segmented playback must stay out of the way.
  if (FSEQ_isFppOverrideActive()) {
    stopFseqSyncIfNeeded(segId);
    _fseq_wasPlaying[segId] = false;
    _fseq_syncStartMs[segId] = 0;
    _fseq_lastSyncMs[segId] = 0;
    SEGMENT.fill(BLACK);
    return;
  }

  const uint16_t fileCount = FSEQ_getFileIndexCount();
  const uint8_t selectedIndex = SEGMENT.custom3;
  const bool loop = SEGMENT.check1;
  const bool sendSyncMulticast = SEGMENT.check2;
  const bool sendSyncBroadcast = SEGMENT.check3;
  const bool sendSyncEnabled = sendSyncMulticast || sendSyncBroadcast;

  String currentFileName;
  const bool hasValidFile =
      (fileCount > 0) &&
      (selectedIndex < fileCount) &&
      FSEQ_getFileNameByIndex(selectedIndex, currentFileName) &&
      currentFileName.length() > 0;

  if (!hasValidFile) {
    if (FSEQPlayer::isSegmentPlaying(segId)) {
      FSEQPlayer::clearSegmentPlayback(segId);
    }
    stopFseqSyncIfNeeded(segId, currentFileName);
    _fseq_wasPlaying[segId] = false;
    _fseq_syncStartMs[segId] = 0;
    _fseq_lastSyncMs[segId] = 0;
    SEGMENT.fill(BLACK);
    _fseq_lastIndex[segId] = selectedIndex;
    _fseq_lastLoop[segId] = loop;
    _fseq_lastFileCount[segId] = fileCount;
    return;
  }

  const bool selectionChanged =
      (_fseq_lastIndex[segId] != selectedIndex) ||
      (_fseq_lastLoop[segId] != loop) ||
      (_fseq_lastFileCount[segId] != fileCount);

  if (selectionChanged) {
    char path[256];
    currentFileName.toCharArray(path, sizeof(path));
    FSEQPlayer::loadRecordingForSegment(segId, path, 0.0f, loop);
    _fseq_syncStartMs[segId] = millis();
    _fseq_lastSyncMs[segId] = 0;
  }

  _fseq_lastIndex[segId] = selectedIndex;
  _fseq_lastLoop[segId] = loop;
  _fseq_lastFileCount[segId] = fileCount;

  FSEQPlayer::setSegmentLooping(segId, loop);

  if (!FSEQPlayer::isSegmentPlaying(segId)) {
    stopFseqSyncIfNeeded(segId, currentFileName);
    _fseq_wasPlaying[segId] = false;
    _fseq_syncStartMs[segId] = 0;
    _fseq_lastSyncMs[segId] = 0;
    SEGMENT.fill(BLACK);
    return;
  }

  const uint32_t now = millis();
  if (!_fseq_wasPlaying[segId] || _fseq_syncStartMs[segId] == 0) {
    _fseq_syncStartMs[segId] = now;
  }

  if (!sendSyncEnabled) {
    stopFseqSyncIfNeeded(segId, currentFileName);
  } else {
    const float secondsElapsed =
        (float)(now - _fseq_syncStartMs[segId]) / 1000.0f;
    const bool needsStart = selectionChanged || !_fseq_wasPlaying[segId] ||
                            !_fseq_wasSyncing[segId] ||
                            (_fseq_lastSendMulticast[segId] != sendSyncMulticast) ||
                            (_fseq_lastSendBroadcast[segId] != sendSyncBroadcast);

    if (needsStart) {
      FPP_sendEffectSyncMessage(0, currentFileName, 0, secondsElapsed,
                                sendSyncMulticast, sendSyncBroadcast);
      _fseq_lastSyncMs[segId] = now;
      _fseq_wasSyncing[segId] = true;
    } else if ((now - _fseq_lastSyncMs[segId]) >= FSEQ_EFFECT_SYNC_INTERVAL_MS) {
      FPP_sendEffectSyncMessage(2, currentFileName, 0, secondsElapsed,
                                sendSyncMulticast, sendSyncBroadcast);
      _fseq_lastSyncMs[segId] = now;
      _fseq_wasSyncing[segId] = true;
    }

    _fseq_lastSendMulticast[segId] = sendSyncMulticast;
    _fseq_lastSendBroadcast[segId] = sendSyncBroadcast;
  }

  _fseq_wasPlaying[segId] = true;
  FSEQPlayer::renderSegmentFrame(segId, SEGMENT);
}

static const char _data_FX_MODE_FSEQ_PLAYER[] PROGMEM =
    "FSEQ Player@,,,,Index,Loop,Send Sync Multicast,Send Sync Broadcast;;;c3=0,o1=1,o2=0,o3=0";
