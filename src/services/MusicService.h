#pragma once

#include <stdint.h>

#include <Audio.h>

#include "../bsp/DacDriver.h"
#include "SdCardService.h"

class MusicService {
public:
  static constexpr uint8_t kMaxNameLen = 64;
  static constexpr uint8_t kMaxPathLen = 96;
  static constexpr uint8_t kMaxAudioDebugLen = 96;
  static constexpr uint8_t kRowsPerPage = 4;
  static constexpr uint8_t kMaxVolume = 21;
  static constexpr uint8_t kSpectrumBandCount = 16;

  enum class Format : uint8_t {
    Unknown,
    Mp3,
    M4a,
    Aac,
    Wav,
    Flac,
    Opus,
    Ogg,
    Oga
  };

  enum class ListState : uint8_t {
    Idle,
    Scanning,
    Ready,
    SdMissing,
    Empty,
    Error
  };

  enum class PlaybackState : uint8_t {
    Stopped,
    Opening,
    Playing,
    Paused,
    Suspended,
    Error
  };

  enum class PlayMode : uint8_t {
    Sequential,
    Shuffle
  };

  struct TrackInfo {
    char path[kMaxPathLen] = {0};
    char name[kMaxNameLen] = {0};
    uint32_t sizeBytes = 0;
    Format format = Format::Unknown;
  };

  struct Page {
    TrackInfo tracks[kRowsPerPage];
    uint8_t count = 0;
    uint16_t pageIndex = 0;
  };

  bool begin();
  void tick(uint32_t nowMs);
  bool consumeChanged();
  void handleAudioInfo(Audio::msg_t msg);

  bool startListScan(SdCardService& sd);
  void exitMusic(SdCardService& sd);
  bool loadPage(uint16_t pageIndex);
  const Page& page() const;

  uint32_t trackCount() const;
  uint16_t pageCount() const;
  ListState listState() const;
  PlaybackState playbackState() const;
  PlayMode playMode() const;
  bool shuffleEnabled() const;
  const TrackInfo& currentTrack() const;
  uint32_t currentTrackIndex() const;
  uint32_t durationSeconds() const;
  uint32_t currentSeconds() const;
  uint16_t vuLevel() const;
  const uint8_t* spectrumBands() const;
  bool headphonesInserted() const;
  uint8_t volume() const;
  const char* errorText() const;
  const char* audioDebugText() const;

  bool playTrack(uint32_t absoluteIndex);
  bool togglePause(uint32_t nowMs);
  bool stop();
  bool next();
  bool previous();
  void setShuffleEnabled(bool enabled);
  void cycleVolume();
  bool adjustVolume(int8_t delta);
  bool isFavorite(const TrackInfo& track) const;
  bool toggleFavorite(const TrackInfo& track);

private:
  struct IndexHeader {
    char magic[4];
    uint16_t version;
    uint16_t recordSize;
    uint32_t recordCount;
    uint8_t truncated;
    uint8_t reserved[15];
  };

  enum class ScanPhase : uint8_t {
    Idle,
    Mount,
    OpenRoot,
    BeginIndex,
    ScanFiles,
    FinalizeIndex,
    Finish
  };

  void processListScan(SdCardService& sd);
  bool beginIndexWrite();
  bool scanNextIndexBatch();
  bool finalizeIndexWrite();
  void closeScanFiles();
  bool readRecord(uint32_t index, TrackInfo& out);
  bool appendRecord(const TrackInfo& track);
  bool isSupportedExtension(const char* name, Format& format) const;
  void basenameFromPath(const char* path, char* out, size_t outSize) const;
  bool readFavoriteLine(File& file, char* out, size_t outSize) const;
  void setError(const char* error);
  void clearAudioDebug();
  void clearSpectrumBands();
  void resetPlaybackRuntime();
  void markChanged();
  void suspendForPauseTimeout();
  bool resumeSuspended();
  void updateDurationCache();
  uint32_t nextTrackIndex() const;
  uint32_t previousTrackIndex() const;
  uint32_t shuffledTrackIndex() const;
  bool advanceAfterTrackEnd();

  Audio audio_;
  DacDriver dac_;
  ListState listState_ = ListState::Idle;
  PlaybackState playbackState_ = PlaybackState::Stopped;
  PlayMode playMode_ = PlayMode::Sequential;
  Page page_;
  TrackInfo currentTrack_;
  uint32_t currentTrackIndex_ = 0;
  uint32_t trackCount_ = 0;
  uint32_t durationSeconds_ = 0;
  uint32_t currentSeconds_ = 0;
  uint16_t vuLevel_ = 0;
  uint8_t spectrumBands_[kSpectrumBandCount] = {0};
  uint32_t suspendedSecond_ = 0;
  uint32_t openingStartMs_ = 0;
  uint32_t lastAudioInfoMs_ = 0;
  uint32_t pauseStartMs_ = 0;
  uint32_t lastHpCheckMs_ = 0;
  uint8_t volume_ = 12;
  bool changed_ = false;
  bool streamReady_ = false;
  bool trackEndPending_ = false;
  bool scanPending_ = false;
  ScanPhase scanPhase_ = ScanPhase::Idle;
  SdCardService* scanSd_ = nullptr;
  File scanRoot_;
  File scanIndex_;
  char errorText_[40] = {0};
  char audioDebugText_[kMaxAudioDebugLen] = {0};

  static constexpr const char* kMusicDir = "/music";
  static constexpr const char* kIndexPath = "/music/.openbread_music.idx";
  static constexpr const char* kTmpIndexPath = "/music/.openbread_music.tmp";
  static constexpr const char* kFavoritesPath = "/music/.openbread_favorites.idx";
  static constexpr const char* kTmpFavoritesPath = "/music/.openbread_favorites.tmp";
  static constexpr uint16_t kIndexVersion = 1;
  static constexpr uint8_t kScanFilesPerTick = 4;
  static constexpr uint32_t kPausePowerOffMs = 60000;
  static constexpr uint32_t kHpCheckIntervalMs = 500;
  static constexpr uint32_t kOpenDurationTimeoutMs = 12000;
  static constexpr uint32_t kOpenNoProgressTimeoutMs = 5000;
};
