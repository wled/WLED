#include "wled.h"

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
static byte           playlistOptions = 0;       //bit 0: shuffle playlist after each iteration. bits 1-7 TBD

static PlaylistEntry *playlistEntries = nullptr;
static byte           playlistLen;               //number of playlist entries
static int8_t         playlistIndex = -1;
static uint32_t       playlistEntryDur = 0;      //duration of the current entry in milliseconds

//values we need to keep about the parent playlist while inside sub-playlist
static int16_t        parentPlaylistIndex = -1;
static byte           parentPlaylistRepeat = 0;
static byte           parentPlaylistPresetId = 0; //for re-loading

//fast-forwarding a playlist that should already have been running (see setPlaylistSeek())
static byte           seekPresetId = 0;          //preset ID of the playlist to fast-forward when it is loaded
static uint32_t       seekElapsed = 0;           //how long ago (ms) that playlist should have started
static uint32_t       seekEntryOffset = 0;       //how long (ms) the first entry played after a seek has already been running


void shufflePlaylist() {
  int currentIndex = playlistLen;
  PlaylistEntry temporaryValue;

  // While there remain elements to shuffle...
  while (currentIndex--) {
    // Pick a random element...
    int randomIndex = random(0, currentIndex);
    // And swap it with the current element.
    temporaryValue = playlistEntries[currentIndex];
    playlistEntries[currentIndex] = playlistEntries[randomIndex];
    playlistEntries[randomIndex] = temporaryValue;
  }
  DEBUG_PRINTLN(F("Playlist shuffle."));
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
  seekPresetId = 0;
  seekEntryOffset = 0;
  DEBUG_PRINTLN(F("Playlist unloaded."));
}


/*
 * Make playlist preset presetId, when it is next loaded, start where it would be had it started elapsedMs ago.
 * Call after applyPreset(), which discards any pending seek (by unloading the playlist).
 * Has no effect if the preset turns out not to be a playlist.
 */
void setPlaylistSeek(byte presetId, uint32_t elapsedMs) {
  seekPresetId = presetId;
  seekElapsed = elapsedMs;
}


/*
 * Fast-forward the just loaded playlist to where it would be had it started elapsedMs ago.
 * rep is the number of repetitions (0 = endless).
 * Nested playlists are not followed: if one would be playing, it starts from its beginning.
 */
static void seekPlaylist(uint32_t elapsedMs, int rep) {
  if (playlistOptions & PL_OPTION_SHUFFLE) return; // order is random, so there is no telling where it would be

  // an entry with no duration plays until advanced manually, so the playlist never gets past it
  uint64_t cycleMs = 0;
  bool hasEndlessEntry = false;
  for (unsigned i = 0; i < playlistLen; i++) {
    if (playlistEntries[i].dur == 0) { hasEndlessEntry = true; break; }
    cycleMs += playlistEntries[i].dur;
  }

  uint32_t cycles = 0;
  if (!hasEndlessEntry) {
    cycles = elapsedMs / cycleMs;
    if (rep > 0 && cycles >= (uint32_t)rep) {
      // it would have finished: let handlePlaylist() end it, staying on the last entry if there is no end preset
      if (!playlistEndPreset) playlistEndPreset = playlistEntries[playlistLen - 1].preset;
      playlistIndex = playlistLen - 1;
      playlistRepeat = 1;
      DEBUG_PRINTLN(F("Playlist seek: already finished."));
      return;
    }
    elapsedMs %= cycleMs;
  }

  unsigned i = 0;
  while (i < playlistLen - 1u && playlistEntries[i].dur > 0 && elapsedMs >= playlistEntries[i].dur) elapsedMs -= playlistEntries[i++].dur;
  playlistIndex = (int)i - 1; // handlePlaylist() advances to entry i
  // handlePlaylist() counts down a repetition when it wraps round to entry 0
  if (rep > 0) playlistRepeat = rep + 1 - cycles - (i > 0);
  seekEntryOffset = elapsedMs;
  DEBUG_PRINTF_P(PSTR("Playlist seek: entry %u, %u ms in.\n"), i, (unsigned)elapsedMs);
}


int16_t loadPlaylist(JsonObject playlistObj, byte presetId) {
  if (currentPlaylist > 0 && parentPlaylistPresetId > 0) return -1; // we are already in nested playlist, do nothing
  uint32_t seekMs = (presetId > 0 && presetId == seekPresetId) ? seekElapsed : 0; // read before unloadPlaylist() clears it
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
  shuffle = shuffle || playlistObj["r"];
  if (shuffle) playlistOptions |= PL_OPTION_SHUFFLE;

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
  if (seekMs && playlistIndex < 0) seekPlaylist(seekMs, rep);
  DEBUG_PRINTLN(F("Playlist loaded."));
  return currentPlaylist;
}


void handlePlaylist() {
  static unsigned long presetCycledTime = 0;
  if (currentPlaylist < 0 || playlistEntries == nullptr) return;

  if ((playlistEntryDur < UINT32_MAX && millis() - presetCycledTime > playlistEntryDur) || doAdvancePlaylist) {
    presetCycledTime = millis();
    if (bri == 0 || nightlightActive) return;

    ++playlistIndex %= playlistLen; // -1 at 1st run (limit to playlistLen)

    // playlist roll-over
    if (!playlistIndex) {
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

    jsonTransitionOnce = true;
    strip.setTransition(playlistEntries[playlistIndex].tr * 100);
    playlistEntryDur = playlistEntries[playlistIndex].dur > 0 ? playlistEntries[playlistIndex].dur : UINT32_MAX; // UINT32_MAX means infinite
    presetCycledTime -= seekEntryOffset; // after a seek, the entry has already been playing this long
    seekEntryOffset = 0;
    applyPresetFromPlaylist(playlistEntries[playlistIndex].preset);
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
  playlist["r"] = playlistOptions & PL_OPTION_SHUFFLE;
  for (int i=0; i<playlistLen; i++) {
    ps.add(playlistEntries[i].preset);
    dur.add((playlistEntries[i].dur) / 100); // convert ms back to tenths of seconds (backwards compatibility)
    transition.add(playlistEntries[i].tr);
  }
}
