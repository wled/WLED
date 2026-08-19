#ifndef FSEQ_PLAYER_H
#define FSEQ_PLAYER_H

static constexpr int16_t RECORDING_REPEAT_LOOP = -1;
static constexpr int16_t RECORDING_REPEAT_DEFAULT = 0;

#include "wled.h"
#include "sd_adapter_compat.h"

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

  static void loadRecordingForSegment(uint8_t segmentId, const char *filepath,
                                      float secondsElapsed = 0.0f,
                                      bool loop = false);
  static void clearSegmentPlayback(uint8_t segmentId);
  static void setSegmentLooping(uint8_t segmentId, bool loop);
  static bool isSegmentPlaying(uint8_t segmentId);
  static bool isAnySegmentPlaying();
  static bool isAnyPlaybackActive();
  static void renderSegmentFrame(uint8_t segmentId, Segment &segment);

  static void loadRecording(const char *filepath,
                            float secondsElapsed = 0.0f,
                            bool loop = false);
  static void clearLastPlayback();
  static void syncPlayback(float secondsElapsed);
  static bool isPlaying();
  static void setLooping(bool loop);
  static String getFileName();
  static float getElapsedSeconds();
  static void renderRealtimeFrame();

private:
  FSEQPlayer() {}

  struct PlaybackState {
    File recordingFile;
    String currentFileName = "";
    float secondsElapsed = 0.0f;
    int32_t recordingRepeats = RECORDING_REPEAT_DEFAULT;
    uint32_t now = 0;
    uint32_t next_time = 0;  // scheduler only
    uint32_t playback_start_time = 0;
    uint32_t frame = 0;
    uint32_t file_size = 0;
    uint32_t frame_data_offset = 0;
    bool frame_position_dirty = true;
    FileHeader file_header{};

    float syncErrorFilteredMs = 0.0f;
    float syncSlewMs = 0.0f;
    float syncCarryMs = 0.0f;
  };

  static const int FSEQ_DEFAULT_STEP_TIME = 50;
  static PlaybackState realtimeState;
  static PlaybackState segmentStates[MAX_NUM_SEGMENTS];

  static inline uint32_t readUInt32(File &file);
  static inline uint32_t readUInt24(File &file);
  static inline uint16_t readUInt16(File &file);
  static inline uint8_t readUInt8(File &file);

  static bool fileOnSD(const char *filepath);
  static bool fileOnFS(const char *filepath);
  static void printHeaderInfo(const PlaybackState &state);
  static void processFrameDataForSegment(PlaybackState &state, Segment &segment);
  static void processFrameDataRealtime(PlaybackState &state);
  static bool stopBecauseAtTheEnd(PlaybackState &state);
  static bool ensureFrameDataPosition(PlaybackState &state);
  static bool skipRemainingFrameData(PlaybackState &state, uint32_t bytesToSkip);
  static void scheduleNextFrame(PlaybackState &state);
  static void resetTimingFromCurrentFrame(PlaybackState &state, uint32_t now);
  static void alignFrameToLocalTime(PlaybackState &state, uint32_t now);
  static void playNextRecordingFrameForSegment(PlaybackState &state, Segment &segment);
  static void playNextRealtimeFrame(PlaybackState &state);
  static void loadRecordingIntoState(PlaybackState &state, const char *filepath,
                                     float secondsElapsed, bool loop);
  static void clearPlaybackState(PlaybackState &state);
  static bool isStatePlaying(const PlaybackState &state);
  static void setStateLooping(PlaybackState &state, bool loop);
  static float getElapsedSeconds(const PlaybackState &state);
};

uint16_t FSEQ_refreshFileIndexCache();
uint16_t FSEQ_getFileIndexCount();
bool FSEQ_getFileNameByIndex(uint16_t index, String &outName);
int16_t FSEQ_findFileIndexByName(const String &name);
void FSEQ_invalidateFileIndexCache();
void FSEQ_markFppControlActivity();
void FSEQ_clearFppOverride();
bool FSEQ_isFppOverrideActive();

#endif // FSEQ_PLAYER_H
