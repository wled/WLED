#include "wled.h"

/*
 * Handles playlists, timed sequences of presets
 */

typedef struct PlaylistEntry {
  uint8_t preset; //ID of the preset to apply
  uint16_t dur;   //Duration of the entry (in tenths of seconds)
  uint16_t tr;    //Duration of the transition TO this entry (in tenths of seconds)
} ple;

static byte           playlistRepeat = 1;        //how many times to repeat the playlist (0 = infinitely)
static byte           playlistEndPreset = 0;     //what preset to apply after playlist end (0 = stay on last preset)
static byte           playlistOptions = 0;       //bit 0: shuffle playlist after each iteration. bits 1-7 TBD

static PlaylistEntry *playlistEntries = nullptr;
static byte           playlistLen;               //number of playlist entries
static int8_t         playlistIndex = -1;
static uint16_t       playlistEntryDur = 0;      //duration of the current entry in tenths of seconds

//values we need to keep about the parent playlist while inside sub-playlist
static int16_t        parentPlaylistIndex = -1;
static byte           parentPlaylistRepeat = 0;
static byte           parentPlaylistPresetId = 0; //for re-loading


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
  playlistLen = playlistEntryDur = playlistOptions = 0;
  DEBUG_PRINTLN(F("Playlist unloaded."));
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

  it = 0;
  JsonArray durations = playlistObj["dur"];
  if (durations.isNull()) {
    playlistEntries[0].dur = playlistObj["dur"] | 100; //10 seconds as fallback
    it = 1;
  } else {
    for (int dur : durations) {
      if (it >= playlistLen) break;
      playlistEntries[it].dur = constrain(dur, 0, 65530);
      it++;
    }
  }
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
  DEBUG_PRINTLN(F("Playlist loaded."));
  return currentPlaylist;
}


void handlePlaylist() {
  static unsigned long presetCycledTime = 0;
  if (currentPlaylist < 0 || playlistEntries == nullptr) return;

  if ((playlistEntryDur < UINT16_MAX && millis() - presetCycledTime > 100 * playlistEntryDur) || doAdvancePlaylist) {
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
    playlistEntryDur = playlistEntries[playlistIndex].dur > 0 ? playlistEntries[playlistIndex].dur : UINT16_MAX;
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
    dur.add(playlistEntries[i].dur);
    transition.add(playlistEntries[i].tr);
  }
}
