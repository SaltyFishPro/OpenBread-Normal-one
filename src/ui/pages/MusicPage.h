#pragma once

#include <stdint.h>

#include "../../services/MusicService.h"
#include "HomePage.h"

class DisplayMonoTft;
class SdCardService;

class MusicPage {
public:
  static constexpr uint8_t kHomeIndex = 1;
  static constexpr uint8_t kMusicListItemIndex = 0;

  bool isMusicListSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  uint8_t detailPageCount(uint8_t homeFocus, uint8_t sectionFocus) const;
  void resetState(uint8_t homeFocus, uint8_t sectionFocus);
  void handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus, MusicService& music,
                         SdCardService& sd);
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool leftEdge,
                         bool rightEdge, bool upEdge, bool downEdge, bool okEdge,
                         uint32_t nowMs, MusicService& music);
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus, MusicService& music,
                        SdCardService& sd);
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display, HomePage::Language language,
                    const MusicService& music, uint32_t nowMs);
  bool renderDetailNavOnly(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                           DisplayMonoTft& display, HomePage::Language language,
                           uint32_t nowMs);
  bool renderDetailListOnly(uint8_t homeFocus, uint8_t sectionFocus, DisplayMonoTft& display,
                            HomePage::Language language, const MusicService& music,
                            uint32_t nowMs);
  bool needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const;
  bool needsNavAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const;
  bool needsListAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus, uint32_t nowMs) const;
  uint32_t detailFrameIntervalMs(uint8_t homeFocus, uint8_t sectionFocus) const;

private:
  enum class PlayerViewState : uint8_t {
    List,
    Entering,
    Player,
    Leaving,
  };

  enum class PlayerControl : uint8_t {
    PlayPause,
    PlayMode,
    Previous,
    Next,
  };

  void startSelectionAnimation(uint8_t nextIndex, uint32_t nowMs);
  void startPlayerTransition(bool entering, uint32_t nowMs);
  void setPlayerUiPlaying(bool playing, uint32_t nowMs);
  void movePlayerControl(int8_t delta);
  void moveListFocus(int8_t delta, uint32_t nowMs, MusicService& music);
  void refreshFavoriteState(MusicService& music);
  bool playSelectedTrack(MusicService& music);
  uint32_t selectedAbsoluteIndex() const;
  const MusicService::TrackInfo* focusedTrack(const MusicService& music) const;
  float selectionProgress(uint32_t nowMs) const;
  float playerTransitionProgress(uint32_t nowMs) const;
  bool isSelectionAnimating(uint32_t nowMs) const;
  bool isListFocusAnimating(uint32_t nowMs) const;
  bool isPlayerTransitionAnimating(uint32_t nowMs) const;
  bool isVolumePopupAnimating(uint32_t nowMs) const;
  void resetStateUnchecked();

  uint8_t selectedIndex_ = 0;
  bool favoriteEnabled_ = false;
  bool selectionAnimating_ = false;
  uint32_t selectionAnimStartMs_ = 0;
  uint16_t pageIndex_ = 0;
  uint8_t rowIndex_ = 0;
  bool listFocusAnimating_ = false;
  uint8_t listFocusFromRow_ = 0;
  uint8_t listFocusToRow_ = 0;
  uint32_t listFocusAnimStartMs_ = 0;
  bool volumePopupOpen_ = false;
  uint32_t volumePopupStartMs_ = 0;
  PlayerViewState playerViewState_ = PlayerViewState::List;
  uint8_t playerControlIndex_ = 0;
  bool playerUiPlaying_ = false;
  bool playerUiShuffle_ = false;
  float playerTransitionStartProgress_ = 0.0f;
  float playerTransitionTargetProgress_ = 0.0f;
  uint32_t playerTransitionStartMs_ = 0;
  uint32_t playerTransitionDurationMs_ = 0;
};
