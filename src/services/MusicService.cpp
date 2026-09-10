#include "MusicService.h"

#include <Arduino.h>
#include <SD_MMC.h>
#include <esp_system.h>
#include <cstdio>
#include <cstring>

#include "../bsp/BoardConfig.h"

namespace {
constexpr char kMagic[4] = {'O', 'B', 'M', 'I'};
MusicService* gActiveMusicService = nullptr;

#ifndef MUSIC_DEBUG_LOG
#define MUSIC_DEBUG_LOG 1
#endif

#if MUSIC_DEBUG_LOG
#define MUSIC_LOG(prefix, fmt, ...) Serial.printf(prefix " " fmt "\n", ##__VA_ARGS__)
#else
#define MUSIC_LOG(prefix, fmt, ...) \
  do {                              \
  } while (0)
#endif

bool endsWithIgnoreCase(const char* value, const char* suffix) {
  const size_t valueLen = strlen(value);
  const size_t suffixLen = strlen(suffix);
  if (suffixLen > valueLen) {
    return false;
  }
  const char* start = value + valueLen - suffixLen;
  for (size_t i = 0; i < suffixLen; ++i) {
    char a = start[i];
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
    if (a != b) {
      return false;
    }
  }
  return true;
}

void audioInfoCallback(Audio::msg_t msg) {
  if (gActiveMusicService != nullptr) {
    gActiveMusicService->handleAudioInfo(msg);
  }
}
}  // namespace

bool MusicService::begin() {
  gActiveMusicService = this;
  Audio::audio_info_callback = audioInfoCallback;
  if (!dac_.begin()) {
    setError("audio gpio init failed");
    return false;
  }
  audio_.setPinout(BoardConfig::kPinPcmBck, BoardConfig::kPinPcmLrck,
                   BoardConfig::kPinPcmDin);
  audio_.setVolume(volume_);
  return true;
}

void MusicService::tick(uint32_t nowMs) {
  if (scanPending_ && scanSd_ != nullptr) {
    processListScan(*scanSd_);
  }

  if (playbackState_ == PlaybackState::Playing || playbackState_ == PlaybackState::Opening) {
    const bool wasPlaying = playbackState_ == PlaybackState::Playing;
    const uint32_t beforeDuration = durationSeconds_;
    const uint32_t beforeCurrent = currentSeconds_;
    audio_.loop();
    updateDurationCache();
    vuLevel_ = audio_.getVUlevel();
    const uint8_t spectrumCount = audio_.getSpectrum(spectrumBands_, kSpectrumBandCount);
    for (uint8_t i = spectrumCount; i < kSpectrumBandCount; ++i) {
      spectrumBands_[i] = 0;
    }
    if (trackEndPending_) {
      trackEndPending_ = false;
      (void)advanceAfterTrackEnd();
      return;
    }
    if (playbackState_ == PlaybackState::Opening && (durationSeconds_ > 0 || streamReady_)) {
      playbackState_ = PlaybackState::Playing;
      markChanged();
    } else if (wasPlaying && beforeDuration > 0 &&
               beforeCurrent + 1U >= beforeDuration && durationSeconds_ == 0 &&
               currentSeconds_ == 0) {
      (void)advanceAfterTrackEnd();
      return;
    } else if (wasPlaying && durationSeconds_ > 0 && currentSeconds_ >= durationSeconds_) {
      (void)advanceAfterTrackEnd();
      return;
    } else if (playbackState_ == PlaybackState::Opening &&
               (nowMs - openingStartMs_) >= kOpenDurationTimeoutMs &&
               (lastAudioInfoMs_ == 0 ||
                (nowMs - lastAudioInfoMs_) >= kOpenNoProgressTimeoutMs)) {
      audio_.stopSong();
      dac_.powerOff();
      playbackState_ = PlaybackState::Error;
      vuLevel_ = 0;
      clearSpectrumBands();
      setError("audio open timeout");
    }
  }

  if (playbackState_ == PlaybackState::Paused &&
      (nowMs - pauseStartMs_) >= kPausePowerOffMs) {
    suspendForPauseTimeout();
  }

  if ((playbackState_ == PlaybackState::Playing || playbackState_ == PlaybackState::Opening) &&
      (nowMs - lastHpCheckMs_) >= kHpCheckIntervalMs) {
    lastHpCheckMs_ = nowMs;
    dac_.updateOutputRoute(true);
  }
}

bool MusicService::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

bool MusicService::startListScan(SdCardService& sd) {
  listState_ = ListState::Scanning;
  trackCount_ = 0;
  page_ = Page{};
  errorText_[0] = '\0';
  clearAudioDebug();
  closeScanFiles();
  scanPending_ = true;
  scanPhase_ = ScanPhase::Mount;
  scanSd_ = &sd;
  markChanged();
  return true;
}

void MusicService::processListScan(SdCardService& sd) {
  switch (scanPhase_) {
    case ScanPhase::Mount:
      if (sd.refresh() != SdCardService::Status::Ready) {
        scanPending_ = false;
        scanPhase_ = ScanPhase::Idle;
        scanSd_ = nullptr;
        listState_ = ListState::SdMissing;
        setError("sd not ready");
        MUSIC_LOG("[SD]", "music scan failed reason=sd_not_ready");
        return;
      }
      scanPhase_ = ScanPhase::OpenRoot;
      markChanged();
      return;

    case ScanPhase::OpenRoot: {
      MUSIC_LOG("[SD]", "music scan begin path=%s", kMusicDir);
      File root = SD_MMC.open(kMusicDir);
      if (!root || !root.isDirectory()) {
        if (root) root.close();
        scanPending_ = false;
        scanPhase_ = ScanPhase::Idle;
        scanSd_ = nullptr;
        listState_ = ListState::Empty;
        setError("music dir missing");
        return;
      }
      root.close();
      scanPhase_ = ScanPhase::BeginIndex;
      markChanged();
      return;
    }

    case ScanPhase::BeginIndex:
      if (!beginIndexWrite()) {
        scanPending_ = false;
        scanPhase_ = ScanPhase::Idle;
        scanSd_ = nullptr;
        listState_ = ListState::Error;
        closeScanFiles();
        markChanged();
        return;
      }
      scanPhase_ = ScanPhase::ScanFiles;
      markChanged();
      return;

    case ScanPhase::ScanFiles:
      if (!scanNextIndexBatch()) {
        scanPending_ = false;
        scanPhase_ = ScanPhase::Idle;
        scanSd_ = nullptr;
        listState_ = ListState::Error;
        closeScanFiles();
        markChanged();
        return;
      }
      if (!scanRoot_) {
        scanPhase_ = ScanPhase::FinalizeIndex;
      }
      markChanged();
      return;

    case ScanPhase::FinalizeIndex:
      if (!finalizeIndexWrite()) {
        scanPending_ = false;
        scanPhase_ = ScanPhase::Idle;
        scanSd_ = nullptr;
        listState_ = ListState::Error;
        closeScanFiles();
        markChanged();
        return;
      }
      scanPhase_ = ScanPhase::Finish;
      markChanged();
      return;

    case ScanPhase::Finish:
      scanPending_ = false;
      scanPhase_ = ScanPhase::Idle;
      scanSd_ = nullptr;
      closeScanFiles();
      listState_ = trackCount_ == 0 ? ListState::Empty : ListState::Ready;
      MUSIC_LOG("[SD]", "music scan done count=%lu", static_cast<unsigned long>(trackCount_));
      if (listState_ == ListState::Ready) {
        (void)loadPage(0);
      }
      markChanged();
      return;

    case ScanPhase::Idle:
    default:
      scanPending_ = false;
      scanSd_ = nullptr;
      return;
  }
}

void MusicService::exitMusic(SdCardService& sd) {
  (void)stop();
  audio_.stopSong();
  dac_.powerOff();
  scanPending_ = false;
  scanPhase_ = ScanPhase::Idle;
  scanSd_ = nullptr;
  closeScanFiles();
  sd.end();
  MUSIC_LOG("[SD]", "unmount after music exit");
  listState_ = ListState::Idle;
  page_ = Page{};
  trackCount_ = 0;
  markChanged();
}

bool MusicService::beginIndexWrite() {
  SD_MMC.remove(kTmpIndexPath);
  MUSIC_LOG("[SD]", "music index write begin path=%s", kIndexPath);
  scanIndex_ = SD_MMC.open(kTmpIndexPath, FILE_WRITE);
  if (!scanIndex_) {
    setError("index open failed");
    return false;
  }

  IndexHeader header{};
  memcpy(header.magic, kMagic, sizeof(header.magic));
  header.version = kIndexVersion;
  header.recordSize = sizeof(TrackInfo);
  header.recordCount = 0;
  header.truncated = 0;
  if (scanIndex_.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) !=
      sizeof(header)) {
    closeScanFiles();
    setError("index header failed");
    return false;
  }

  scanRoot_ = SD_MMC.open(kMusicDir);
  if (!scanRoot_ || !scanRoot_.isDirectory()) {
    closeScanFiles();
    setError("music dir missing");
    return false;
  }

  trackCount_ = 0;
  return true;
}

bool MusicService::scanNextIndexBatch() {
  if (!scanRoot_ || !scanIndex_) {
    return true;
  }

  for (uint8_t processed = 0; processed < kScanFilesPerTick; ++processed) {
    File file = scanRoot_.openNextFile();
    if (!file) {
      scanRoot_.close();
      scanRoot_ = File();
      return true;
    }

    if (!file.isDirectory()) {
      TrackInfo track;
      Format format = Format::Unknown;
      const char* path = file.path();
      if (isSupportedExtension(path, format)) {
        snprintf(track.path, sizeof(track.path), "%s", path);
        basenameFromPath(path, track.name, sizeof(track.name));
        track.sizeBytes = static_cast<uint32_t>(file.size());
        track.format = format;
        if (scanIndex_.write(reinterpret_cast<const uint8_t*>(&track), sizeof(track)) !=
            sizeof(track)) {
          file.close();
          closeScanFiles();
          setError("index write failed");
          return false;
        }
        ++trackCount_;
      }
    }
    file.close();
  }
  return true;
}

bool MusicService::finalizeIndexWrite() {
  if (!scanIndex_) {
    setError("index finalize failed");
    return false;
  }

  IndexHeader header{};
  memcpy(header.magic, kMagic, sizeof(header.magic));
  header.version = kIndexVersion;
  header.recordSize = sizeof(TrackInfo);
  header.recordCount = trackCount_;
  header.truncated = 0;
  scanIndex_.seek(0);
  if (scanIndex_.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) !=
      sizeof(header)) {
    closeScanFiles();
    setError("index update failed");
    return false;
  }
  scanIndex_.close();
  scanIndex_ = File();

  SD_MMC.remove(kIndexPath);
  if (!SD_MMC.rename(kTmpIndexPath, kIndexPath)) {
    setError("index rename failed");
    return false;
  }

  MUSIC_LOG("[SD]", "music index write done count=%lu",
            static_cast<unsigned long>(trackCount_));
  return true;
}

void MusicService::closeScanFiles() {
  if (scanRoot_) {
    scanRoot_.close();
    scanRoot_ = File();
  }
  if (scanIndex_) {
    scanIndex_.close();
    scanIndex_ = File();
  }
}

bool MusicService::loadPage(uint16_t pageIndex) {
  page_ = Page{};
  page_.pageIndex = pageIndex;
  const uint32_t start = static_cast<uint32_t>(pageIndex) * kRowsPerPage;
  if (start >= trackCount_) {
    markChanged();
    return false;
  }

  for (uint8_t i = 0; i < kRowsPerPage; ++i) {
    if (start + i >= trackCount_) {
      break;
    }
    if (!readRecord(start + i, page_.tracks[i])) {
      break;
    }
    ++page_.count;
  }
  markChanged();
  return page_.count > 0;
}

bool MusicService::readRecord(uint32_t index, TrackInfo& out) {
  File file = SD_MMC.open(kIndexPath, FILE_READ);
  if (!file) {
    setError("index read failed");
    return false;
  }

  IndexHeader header{};
  if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    file.close();
    setError("index header read failed");
    return false;
  }

  if (memcmp(header.magic, kMagic, sizeof(header.magic)) != 0 ||
      header.version != kIndexVersion || header.recordSize != sizeof(TrackInfo) ||
      index >= header.recordCount) {
    file.close();
    setError("index invalid");
    return false;
  }

  const uint32_t offset = sizeof(IndexHeader) + index * sizeof(TrackInfo);
  if (!file.seek(offset)) {
    file.close();
    setError("index seek failed");
    return false;
  }
  const bool ok = file.read(reinterpret_cast<uint8_t*>(&out), sizeof(out)) == sizeof(out);
  file.close();
  if (!ok) {
    setError("index record read failed");
  }
  return ok;
}

bool MusicService::isSupportedExtension(const char* name, Format& format) const {
  if (endsWithIgnoreCase(name, ".mp3")) {
    format = Format::Mp3;
    return true;
  }
  if (endsWithIgnoreCase(name, ".m4a")) {
    format = Format::M4a;
    return true;
  }
  if (endsWithIgnoreCase(name, ".aac")) {
    format = Format::Aac;
    return true;
  }
  if (endsWithIgnoreCase(name, ".wav")) {
    format = Format::Wav;
    return true;
  }
  if (endsWithIgnoreCase(name, ".flac")) {
    format = Format::Flac;
    return true;
  }
  if (endsWithIgnoreCase(name, ".opus")) {
    format = Format::Opus;
    return true;
  }
  if (endsWithIgnoreCase(name, ".ogg")) {
    format = Format::Ogg;
    return true;
  }
  if (endsWithIgnoreCase(name, ".oga")) {
    format = Format::Oga;
    return true;
  }
  format = Format::Unknown;
  return false;
}

void MusicService::basenameFromPath(const char* path, char* out, size_t outSize) const {
  const char* slash = strrchr(path, '/');
  snprintf(out, outSize, "%s", slash == nullptr ? path : slash + 1);
}

bool MusicService::readFavoriteLine(File& file, char* out, size_t outSize) const {
  if (out == nullptr || outSize == 0) {
    return false;
  }

  size_t len = 0;
  while (file.available()) {
    const char c = static_cast<char>(file.read());
    if (c == '\n') {
      break;
    }
    if (c == '\r') {
      continue;
    }
    if (len + 1 < outSize) {
      out[len++] = c;
    }
  }

  out[len] = '\0';
  return len > 0;
}

bool MusicService::isFavorite(const TrackInfo& track) const {
  if (track.path[0] == '\0') {
    return false;
  }

  File file = SD_MMC.open(kFavoritesPath, FILE_READ);
  if (!file) {
    return false;
  }

  char line[kMaxPathLen] = {0};
  while (readFavoriteLine(file, line, sizeof(line))) {
    if (strncmp(line, track.path, sizeof(line)) == 0) {
      file.close();
      return true;
    }
  }

  file.close();
  return false;
}

bool MusicService::toggleFavorite(const TrackInfo& track) {
  if (track.path[0] == '\0') {
    return false;
  }

  const bool removeTrack = isFavorite(track);
  SD_MMC.remove(kTmpFavoritesPath);
  File out = SD_MMC.open(kTmpFavoritesPath, FILE_WRITE);
  if (!out) {
    setError("favorite write failed");
    return false;
  }

  bool changed = false;
  File in = SD_MMC.open(kFavoritesPath, FILE_READ);
  if (in) {
    char line[kMaxPathLen] = {0};
    while (readFavoriteLine(in, line, sizeof(line))) {
      if (strncmp(line, track.path, sizeof(line)) == 0) {
        changed = true;
        continue;
      }
      out.printf("%s\n", line);
    }
    in.close();
  }

  if (!removeTrack) {
    out.printf("%s\n", track.path);
    changed = true;
  }

  out.close();
  SD_MMC.remove(kFavoritesPath);
  if (!SD_MMC.rename(kTmpFavoritesPath, kFavoritesPath)) {
    setError("favorite rename failed");
    return false;
  }

  if (changed) {
    markChanged();
  }
  return true;
}

bool MusicService::playTrack(uint32_t absoluteIndex) {
  clearAudioDebug();
  if (trackCount_ == 0 || absoluteIndex >= trackCount_) {
    playbackState_ = PlaybackState::Error;
    setError("track index invalid");
    return false;
  }

  if (!readRecord(absoluteIndex, currentTrack_)) {
    playbackState_ = PlaybackState::Error;
    return false;
  }

  resetPlaybackRuntime();
  audio_.stopSong();
  if (!dac_.powerOn()) {
    playbackState_ = PlaybackState::Error;
    setError("audio power failed");
    return false;
  }
  audio_.setPinout(BoardConfig::kPinPcmBck, BoardConfig::kPinPcmLrck, BoardConfig::kPinPcmDin);
  audio_.setVolume(volume_);
  if (!audio_.connecttoFS(SD_MMC, currentTrack_.path)) {
    dac_.powerOff();
    playbackState_ = PlaybackState::Error;
    clearSpectrumBands();
    setError("audio open failed");
    return false;
  }

  currentTrackIndex_ = absoluteIndex;
  openingStartMs_ = millis();
  lastAudioInfoMs_ = openingStartMs_;
  playbackState_ = PlaybackState::Opening;
  MUSIC_LOG("[AUDIO]", "play file=%s", currentTrack_.path);
  markChanged();
  return true;
}

bool MusicService::togglePause(uint32_t nowMs) {
  if (playbackState_ == PlaybackState::Suspended) {
    return resumeSuspended();
  }
  if (playbackState_ != PlaybackState::Playing && playbackState_ != PlaybackState::Paused) {
    return false;
  }
  if (!audio_.pauseResume()) {
    playbackState_ = PlaybackState::Error;
    setError("pause failed");
    markChanged();
    return false;
  }
  if (playbackState_ == PlaybackState::Playing) {
    playbackState_ = PlaybackState::Paused;
    pauseStartMs_ = nowMs;
    clearSpectrumBands();
    MUSIC_LOG("[AUDIO]", "pause");
  } else {
    playbackState_ = PlaybackState::Playing;
    MUSIC_LOG("[AUDIO]", "resume");
  }
  markChanged();
  return true;
}

bool MusicService::stop() {
  if (playbackState_ == PlaybackState::Stopped) {
    return true;
  }
  audio_.stopSong();
  dac_.powerOff();
  MUSIC_LOG("[AUDIO]", "stop reason=user");
  playbackState_ = PlaybackState::Stopped;
  resetPlaybackRuntime();
  markChanged();
  return true;
}

bool MusicService::next() {
  if (trackCount_ == 0) {
    return false;
  }
  return playTrack(nextTrackIndex());
}

bool MusicService::previous() {
  if (trackCount_ == 0) {
    return false;
  }
  return playTrack(previousTrackIndex());
}

void MusicService::setShuffleEnabled(bool enabled) {
  const PlayMode nextMode = enabled ? PlayMode::Shuffle : PlayMode::Sequential;
  if (playMode_ == nextMode) {
    return;
  }
  playMode_ = nextMode;
  markChanged();
}

void MusicService::cycleVolume() {
  static const uint8_t kSteps[] = {4, 8, 12, 16, kMaxVolume};
  uint8_t next = kSteps[0];
  for (uint8_t step : kSteps) {
    if (step > volume_) {
      next = step;
      break;
    }
  }
  volume_ = next;
  audio_.setVolume(volume_);
  markChanged();
}

bool MusicService::adjustVolume(int8_t delta) {
  int16_t next = static_cast<int16_t>(volume_) + delta;
  if (next < 0) {
    next = 0;
  }
  if (next > kMaxVolume) {
    next = kMaxVolume;
  }

  if (next == volume_) {
    return false;
  }

  volume_ = static_cast<uint8_t>(next);
  audio_.setVolume(volume_);
  markChanged();
  return true;
}

void MusicService::suspendForPauseTimeout() {
  suspendedSecond_ = audio_.getAudioCurrentTime();
  audio_.stopSong();
  dac_.powerOff();
  MUSIC_LOG("[AUDIO]", "stop reason=pause_timeout");
  playbackState_ = PlaybackState::Suspended;
  vuLevel_ = 0;
  clearSpectrumBands();
  markChanged();
}

bool MusicService::resumeSuspended() {
  if (currentTrack_.path[0] == '\0') {
    return false;
  }
  if (!dac_.powerOn()) {
    playbackState_ = PlaybackState::Error;
    clearSpectrumBands();
    setError("audio power failed");
    return false;
  }
  audio_.setPinout(BoardConfig::kPinPcmBck, BoardConfig::kPinPcmLrck, BoardConfig::kPinPcmDin);
  audio_.setVolume(volume_);
  if (!audio_.connecttoFS(SD_MMC, currentTrack_.path, static_cast<int32_t>(suspendedSecond_))) {
    dac_.powerOff();
    playbackState_ = PlaybackState::Error;
    clearSpectrumBands();
    setError("resume failed");
    return false;
  }
  playbackState_ = PlaybackState::Opening;
  openingStartMs_ = millis();
  lastAudioInfoMs_ = openingStartMs_;
  streamReady_ = false;
  trackEndPending_ = false;
  vuLevel_ = 0;
  clearSpectrumBands();
  MUSIC_LOG("[AUDIO]", "resume from=%lu", static_cast<unsigned long>(suspendedSecond_));
  markChanged();
  return true;
}

void MusicService::updateDurationCache() {
  const uint32_t duration = audio_.getAudioFileDuration();
  const uint32_t current = audio_.getAudioCurrentTime();
  if (duration != durationSeconds_ || current != currentSeconds_) {
    durationSeconds_ = duration;
    currentSeconds_ = current;
    markChanged();
  }
}

uint32_t MusicService::nextTrackIndex() const {
  if (trackCount_ == 0) {
    return 0;
  }
  if (playMode_ == PlayMode::Shuffle) {
    return shuffledTrackIndex();
  }
  return (static_cast<uint32_t>(currentTrackIndex_) + 1U) % trackCount_;
}

uint32_t MusicService::previousTrackIndex() const {
  if (trackCount_ == 0) {
    return 0;
  }
  if (playMode_ == PlayMode::Shuffle) {
    return shuffledTrackIndex();
  }
  return currentTrackIndex_ == 0 ? trackCount_ - 1U
                                : static_cast<uint32_t>(currentTrackIndex_) - 1U;
}

uint32_t MusicService::shuffledTrackIndex() const {
  if (trackCount_ <= 1U) {
    return 0;
  }

  const uint32_t offset = 1U + (esp_random() % (trackCount_ - 1U));
  return (static_cast<uint32_t>(currentTrackIndex_) + offset) % trackCount_;
}

bool MusicService::advanceAfterTrackEnd() {
  if (trackCount_ == 0) {
    return stop();
  }
  MUSIC_LOG("[AUDIO]", "track end mode=%s",
            playMode_ == PlayMode::Shuffle ? "shuffle" : "sequential");
  return playTrack(nextTrackIndex());
}

void MusicService::setError(const char* error) {
  snprintf(errorText_, sizeof(errorText_), "%s", error);
  MUSIC_LOG("[ERR]", "%s debug=%s", errorText_,
            audioDebugText_[0] != '\0' ? audioDebugText_ : "none");
  markChanged();
}

void MusicService::clearAudioDebug() {
  audioDebugText_[0] = '\0';
}

void MusicService::clearSpectrumBands() {
  memset(spectrumBands_, 0, sizeof(spectrumBands_));
}

void MusicService::resetPlaybackRuntime() {
  durationSeconds_ = 0;
  currentSeconds_ = 0;
  vuLevel_ = 0;
  suspendedSecond_ = 0;
  streamReady_ = false;
  trackEndPending_ = false;
  clearSpectrumBands();
}

void MusicService::handleAudioInfo(Audio::msg_t msg) {
  const char* tag = msg.s != nullptr ? msg.s : "audio";
  const char* text = msg.msg != nullptr ? msg.msg : "";

  MUSIC_LOG("[AUDIOI2S]", "%s: %s", tag, text);
  lastAudioInfoMs_ = millis();

  if (msg.e == Audio::evt_info) {
    if (strncmp(text, "stream ready", 12) == 0) {
      streamReady_ = true;
    }
    if (strncmp(text, "FLAC ", 5) == 0 || strncmp(text, "Duration", 8) == 0 ||
        strncmp(text, "total samples", 13) == 0 || strncmp(text, "Audio-Data-Start", 16) == 0 ||
        strncmp(text, "Audio-Length", 12) == 0 || strncmp(text, "Reading file", 12) == 0 ||
        strncmp(text, "stream ready", 12) == 0 || strncmp(text, "syncword", 8) == 0) {
      snprintf(audioDebugText_, sizeof(audioDebugText_), "%s", text);
      markChanged();
    }
  } else if (msg.e == Audio::evt_eof) {
    trackEndPending_ = playbackState_ == PlaybackState::Playing ||
                       playbackState_ == PlaybackState::Opening;
    snprintf(audioDebugText_, sizeof(audioDebugText_), "%s", text);
    markChanged();
  } else if (msg.e == Audio::evt_log) {
    snprintf(audioDebugText_, sizeof(audioDebugText_), "%s", text);
    markChanged();
  }
}

void MusicService::markChanged() { changed_ = true; }

const MusicService::Page& MusicService::page() const { return page_; }
uint32_t MusicService::trackCount() const { return trackCount_; }
uint16_t MusicService::pageCount() const {
  return trackCount_ == 0 ? 1U : static_cast<uint16_t>((trackCount_ + kRowsPerPage - 1) / kRowsPerPage);
}
MusicService::ListState MusicService::listState() const { return listState_; }
MusicService::PlaybackState MusicService::playbackState() const { return playbackState_; }
MusicService::PlayMode MusicService::playMode() const { return playMode_; }
bool MusicService::shuffleEnabled() const { return playMode_ == PlayMode::Shuffle; }
const MusicService::TrackInfo& MusicService::currentTrack() const { return currentTrack_; }
uint32_t MusicService::currentTrackIndex() const { return currentTrackIndex_; }
uint32_t MusicService::durationSeconds() const { return durationSeconds_; }
uint32_t MusicService::currentSeconds() const { return currentSeconds_; }
uint16_t MusicService::vuLevel() const { return vuLevel_; }
const uint8_t* MusicService::spectrumBands() const { return spectrumBands_; }
bool MusicService::headphonesInserted() const { return dac_.headphonesInserted(); }
uint8_t MusicService::volume() const { return volume_; }
const char* MusicService::errorText() const { return errorText_; }
const char* MusicService::audioDebugText() const { return audioDebugText_; }

bool MusicService::appendRecord(const TrackInfo& track) {
  (void)track;
  return false;
}
