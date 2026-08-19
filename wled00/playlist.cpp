#include "wled.h"
#include "prng.h"

/*
 * Handles playlists, timed sequences of presets
 */

typedef struct PlaylistEntry {
  uint8_t preset; //ID of the preset to apply
  uint16_t tr;    //Duration of the transition TO this entry (in tenths of seconds)
  uint32_t dur;   //Duration of the entry (in milliseconds)
} ple;

static byte           playlistRepeat = 1;        //how many times to repeat the playlist (0 = infinitely)
static byte           playlistEndPreset = 0;     //what preset to apply after playlist end (0 = stay on last preset)
static byte           playlistOptions = 0;       //PL_OPTION_*

static PlaylistEntry *playlistEntries = nullptr;
static byte           playlistLen;               //number of playlist entries
static int8_t         playlistIndex = -1;
static uint64_t       clockSyncEntryStart = UINT64_MAX; //cycle time where playlistIndex starts
static uint32_t       playlistEntryDur = 0;      //duration of the current entry in milliseconds
static uint64_t       playlistTotalDur = 0;      //sum of all entry durations in milliseconds
static uint32_t       playlistCycleNum = 0;      //current playlist cycle for deterministic shuffle
static PRNG           prng;
static bool           playlistEntriesAreShuffled = false;

//values we need to keep about the parent playlist while inside sub-playlist
static int16_t        parentPlaylistIndex = -1;
static byte           parentPlaylistRepeat = 0;
static byte           parentPlaylistPresetId = 0; //for re-loading

static uint16_t deterministicShuffleSeed() {
  uint32_t seed = 2166136261UL;
  seed = (seed ^ playlistCycleNum) * 16777619UL;
  seed = (seed ^ playlistLen) * 16777619UL;
  if (seed == 0) seed = 2166136261UL;
  return (uint16_t)((seed >> 16) ^ seed);
}

static void unShufflePlaylist() {
  if (playlistEntries == nullptr || playlistLen < 2 || !playlistEntriesAreShuffled || !(playlistOptions & PL_OPTION_DETERMINISTIC_SHUFFLE)) return;

  PlaylistEntry temp;
  for (int currentIndex = 1; currentIndex < playlistLen; currentIndex++) {
    int randomIndex = prng.random16Backwards(currentIndex + 1);
    temp = playlistEntries[currentIndex];
    playlistEntries[currentIndex] = playlistEntries[randomIndex];
    playlistEntries[randomIndex] = temp;
  }
  playlistEntriesAreShuffled = false;
  clockSyncEntryStart = UINT64_MAX;
  DEBUG_PRINTLN(F("Playlist unshuffled."));
}

static void shufflePlaylist() {
  if (playlistEntries == nullptr || playlistLen < 2) return;

  bool deterministic = playlistOptions & PL_OPTION_DETERMINISTIC_SHUFFLE;
  if (deterministic) {
    unShufflePlaylist();
    prng.setSeed(deterministicShuffleSeed());
  }

  PlaylistEntry temp;
  for (int currentIndex = playlistLen - 1; currentIndex > 0; currentIndex--) {
    int randomIndex = deterministic ? prng.random16(currentIndex + 1) : random(0, currentIndex + 1);
    temp = playlistEntries[currentIndex];
    playlistEntries[currentIndex] = playlistEntries[randomIndex];
    playlistEntries[randomIndex] = temp;
  }
  playlistEntriesAreShuffled = true;
  clockSyncEntryStart = UINT64_MAX;
  DEBUG_PRINTLN(F("Playlist shuffled."));
}

void unloadPlaylist() {
  if (playlistEntries != nullptr) {
    delete[] playlistEntries;
    playlistEntries = nullptr;
  }
  currentPlaylist = playlistIndex = -1;
  playlistLen = 0;
  playlistOptions = 0;
  playlistEntryDur = 0;
  playlistTotalDur = 0;
  playlistCycleNum = 0;
  playlistEntriesAreShuffled = false;
  clockSyncEntryStart = UINT64_MAX;
  DEBUG_PRINTLN(F("Playlist unloaded."));
}

// Convert absolute Toki time into a playlist cycle number and elapsed time within that cycle
static bool getClockSyncPlaylistCycle(uint32_t &cycleNum, uint64_t &cycleTime) {
  if (!(playlistOptions & PL_OPTION_CLOCK_SYNC) || playlistTotalDur == 0) return false;
  if (toki.getTimeSource() == TOKI_TS_NONE) return false;

  Toki::Time tm = toki.getTime();
  uint64_t absoluteMs = (uint64_t)tm.sec * 1000ULL + tm.ms;
  cycleNum = (uint32_t)(absoluteMs / playlistTotalDur);
  cycleTime = absoluteMs % playlistTotalDur;
  return true;
}

// Map elapsed time inside the current wall-clock cycleNum to a slot in playlistEntries
// and the elapsed time inside that slot
static bool mapCycleTimeToPlaylistSlot(uint64_t cycleTime, int8_t &entrySlot, uint32_t &entryOffset) {
  if (playlistLen == 0 || playlistEntries == nullptr) return false;

  byte startIndex = 0;
  uint64_t entryStart = 0;
  if (clockSyncEntryStart < playlistTotalDur && playlistIndex >= 0 && playlistIndex < playlistLen && cycleTime >= clockSyncEntryStart) {
    startIndex = playlistIndex;
    entryStart = clockSyncEntryStart;
    cycleTime -= clockSyncEntryStart;
  }

  for (byte i = startIndex; i < playlistLen; i++) {
    uint32_t dur = playlistEntries[i].dur;
    if (cycleTime < dur) {
      entrySlot = i;
      entryOffset = (uint32_t)cycleTime;
      clockSyncEntryStart = entryStart;
      return true;
    }
    cycleTime -= dur;
    entryStart += dur;
  }
  
  return false;
}

int16_t loadPlaylist(JsonObject playlistObj, byte presetId) {
  if (currentPlaylist > 0 && parentPlaylistPresetId > 0) return -1; // we are already in nested playlist, do nothing
  if (currentPlaylist > 0) {
    parentPlaylistIndex = playlistIndex;
    parentPlaylistRepeat = playlistRepeat;
    parentPlaylistPresetId = currentPlaylist;
  }
  unloadPlaylist();

  JsonArray presets = playlistObj["ps"];
  playlistLen = presets.size();
  if (playlistLen == 0) return -1;
  if (playlistLen > 100) playlistLen = 100;

  playlistEntries = new(std::nothrow) PlaylistEntry[playlistLen];
  if (playlistEntries == nullptr) return -1;

  byte it = 0;
  for (int ps : presets) {
    if (it >= playlistLen) break;
    playlistEntries[it].preset = ps;
    it++;
  }
  playlistEntriesAreShuffled = false;

  it = 0;
  JsonArray durations = playlistObj["dur"];
  if (durations.isNull()) {
    uint32_t durMs = playlistObj["dur"] | 100; // 10 seconds as fallback (tenths)
    durMs = constrain(durMs, 0L, 42949670L) * 100UL; // limit to max value and convert to ms
    playlistEntries[0].dur = (uint32_t)durMs;
    it = 1;
  } else {
    for (int dur : durations) {
      if (it >= playlistLen) break;
      uint32_t durMs = constrain(dur, 0L, 42949670L) * 100UL; // limit to max value and convert to ms
      playlistEntries[it].dur = (uint32_t)durMs;
      it++;
    }
  }
  if (it > 0) // should never happen but just in case
    for (int i = it; i < playlistLen; i++) playlistEntries[i].dur = playlistEntries[it -1].dur;
  playlistTotalDur = 0;
  for (int i = 0; i < playlistLen; i++) playlistTotalDur += playlistEntries[i].dur;

  it = 0;
  JsonArray tr = playlistObj[F("transition")];
  if (tr.isNull()) {
    playlistEntries[0].tr = playlistObj[F("transition")] | (transitionDelay / 100);
    it = 1;
  } else {
    for (int transition : tr) {
      if (it >= playlistLen) break;
      playlistEntries[it].tr = transition;
      it++;
    }
  }
  for (int i = it; i < playlistLen; i++) playlistEntries[i].tr = playlistEntries[it -1].tr;

  int rep = playlistObj[F("repeat")];
  bool shuffle = false;
  if (rep < 0) { //support negative values as infinite + shuffle
    rep = 0; shuffle = true;
  }

  playlistRepeat = rep;
  if (playlistRepeat > 0) playlistRepeat++; //add one extra repetition immediately since it will be deducted on first start
  playlistEndPreset = playlistObj["end"] | 0;
  // if end preset is 255 restore original preset (if any running) upon playlist end
  if (playlistEndPreset == 255 && currentPreset > 0) {
    playlistEndPreset = currentPreset;
    playlistOptions |= PL_OPTION_RESTORE; // for async save operation
  }
  if (playlistEndPreset > 250) playlistEndPreset = 0;
  shuffle = shuffle || playlistObj["r"].as<bool>();
  if (shuffle) playlistOptions |= PL_OPTION_SHUFFLE;

  bool clockSync = playlistObj[F("clockSync")].as<bool>();
  if (clockSync) playlistOptions |= PL_OPTION_CLOCK_SYNC;

  bool deterministicShuffle = playlistObj[F("deterministic")].as<bool>();
  if (shuffle && deterministicShuffle) playlistOptions |= PL_OPTION_DETERMINISTIC_SHUFFLE;

  if (parentPlaylistPresetId == 0 && parentPlaylistIndex > -1) {
    // we are re-loading playlist when returning from nested playlist
    playlistIndex = parentPlaylistIndex;
    playlistRepeat = parentPlaylistRepeat;
    parentPlaylistIndex = -1;
    parentPlaylistRepeat = 0;
  } else if (rep == 0) {
    // endless playlist will never return to parent so erase parent information if it was called from it
    parentPlaylistPresetId = 0;
    parentPlaylistIndex = -1;
    parentPlaylistRepeat = 0;
  }

  currentPlaylist = presetId;
  DEBUG_PRINTLN(F("Playlist loaded."));
  return currentPlaylist;
}


void handlePlaylist() {
  static unsigned long presetCycledTime = 0;
  if (currentPlaylist < 0 || playlistEntries == nullptr) return;

  unsigned long now = millis();
  uint64_t clockSyncCycleTime = 0;
  uint32_t targetCycleNum = playlistCycleNum;
  int8_t targetPlaylistIndex = playlistIndex;
  bool clockSyncTimeValid = false;
  bool needsRollover = false;
  bool shouldApplyEntry = false;

  // Derive wall-clock rollover state
  if (playlistOptions & PL_OPTION_CLOCK_SYNC) {
    uint32_t cycleNum = 0;
    if (getClockSyncPlaylistCycle(cycleNum, clockSyncCycleTime)) {
      clockSyncTimeValid = true;
      targetCycleNum = cycleNum;
      if (playlistIndex < 0 || playlistCycleNum != targetCycleNum) needsRollover = true;
      // Clock-synced playlists derive the active slot from wall time, so manual "next preset" cannot persist.
      doAdvancePlaylist = false;
    }
  }

  // Derive local-clock/manual advance state
  if (!clockSyncTimeValid && ((playlistEntryDur < UINT32_MAX && now - presetCycledTime > playlistEntryDur) || doAdvancePlaylist)) {
    presetCycledTime = now;

    targetPlaylistIndex = (playlistIndex + 1) % playlistLen; // -1 at 1st run (limit to playlistLen)

    if (!targetPlaylistIndex) {
      if (playlistIndex >= 0) targetCycleNum++;
      needsRollover = true;
    }

    shouldApplyEntry = true;
  }

  if (bri == 0 || nightlightActive) return;

  // Apply rollover
  if (needsRollover) {
    playlistCycleNum = targetCycleNum;
    if (playlistRepeat == 1) { //stop if all repetitions are done
      unloadPlaylist();
      if (parentPlaylistPresetId > 0) {
        applyPresetFromPlaylist(parentPlaylistPresetId); // reload previous playlist (unfortunately asynchronous)
        parentPlaylistPresetId = 0; // reset previous playlist but do not reset Index or Repeat (they will be loaded & reset in loadPlaylist())
      } else if (playlistEndPreset) applyPresetFromPlaylist(playlistEndPreset);
      return;
    }
    if (playlistRepeat > 1) playlistRepeat--; // decrease repeat count on each index reset if not an endless playlist
    // playlistRepeat == 0: endless loop
    if (playlistOptions & PL_OPTION_SHUFFLE) shufflePlaylist(); // shuffle playlist and start over
  }

  // Map wall-clock time to the active playlist slot
  if (clockSyncTimeValid) {
    uint32_t clockSyncEntryOffset = 0;
    if (mapCycleTimeToPlaylistSlot(clockSyncCycleTime, targetPlaylistIndex, clockSyncEntryOffset)) {
      presetCycledTime = now - clockSyncEntryOffset;
      shouldApplyEntry = needsRollover || playlistIndex != targetPlaylistIndex;
      strip.timebase = (unsigned long)clockSyncEntryOffset - now;
    }
  }

  // Apply the selected playlist entry
  if (shouldApplyEntry) {
    playlistIndex = targetPlaylistIndex;
    if (!clockSyncTimeValid) clockSyncEntryStart = UINT64_MAX;
    jsonTransitionOnce = true;
    PlaylistEntry &entry = playlistEntries[playlistIndex];
    strip.setTransition(entry.tr * 100);
    playlistEntryDur = entry.dur > 0 ? entry.dur : UINT32_MAX; // UINT32_MAX means infinite
    applyPresetFromPlaylist(entry.preset);
    doAdvancePlaylist = false;
  }
}


void serializePlaylist(JsonObject sObj) {
  JsonObject playlist = sObj.createNestedObject(F("playlist"));
  JsonArray ps = playlist.createNestedArray("ps");
  JsonArray dur = playlist.createNestedArray("dur");
  JsonArray transition = playlist.createNestedArray(F("transition"));
  playlist[F("repeat")] = (playlistIndex < 0 && playlistRepeat > 0) ? playlistRepeat - 1 : playlistRepeat; // remove added repetition count (if not yet running)
  playlist["end"] = playlistOptions & PL_OPTION_RESTORE ? 255 : playlistEndPreset;
  playlist["r"] = (playlistOptions & PL_OPTION_SHUFFLE) != 0;
  playlist[F("deterministic")] = (playlistOptions & PL_OPTION_DETERMINISTIC_SHUFFLE) != 0;
  playlist[F("clockSync")] = (playlistOptions & PL_OPTION_CLOCK_SYNC) != 0;
  for (int i=0; i<playlistLen; i++) {
    ps.add(playlistEntries[i].preset);
    dur.add((playlistEntries[i].dur) / 100); // convert ms back to tenths of seconds (backwards compatibility)
    transition.add(playlistEntries[i].tr);
  }
}
