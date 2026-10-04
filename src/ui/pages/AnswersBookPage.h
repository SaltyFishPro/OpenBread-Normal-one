#pragma once

#include <stdint.h>

class DisplayMonoTft;

class AnswersBookPage {
public:
  static constexpr uint8_t kHomeIndex = 9;

  bool isSelection(uint8_t homeFocus, uint8_t sectionFocus) const;
  void handleDetailEnter(uint8_t homeFocus, uint8_t sectionFocus);
  bool update(uint32_t nowMs);
  bool handleDetailInput(uint8_t homeFocus, uint8_t sectionFocus, bool okEdge,
                         uint32_t nowMs);
  bool handleDetailBack(uint8_t homeFocus, uint8_t sectionFocus);
  bool renderDetail(uint8_t homeFocus, uint8_t sectionFocus, int16_t yOffset,
                    DisplayMonoTft& display, uint32_t nowMs) const;
  bool needsAnimationFrame(uint8_t homeFocus, uint8_t sectionFocus) const;
  bool isAnimating(uint8_t homeFocus, uint8_t sectionFocus) const;

private:
  enum class View : uint8_t {
    Prompt,
    Thinking,
    RevealAnimation,
    Answer,
  };

  void startThinking(uint32_t nowMs);
  void chooseAnswer();

  View view_ = View::Prompt;
  uint32_t phaseStartMs_ = 0;
  uint16_t answerIndex_ = 0;
  uint16_t lastAnswerIndex_ = 0xFFFFU;
  bool hasAnswer_ = false;

  static constexpr uint32_t kThinkingDurationMs = 5000U;
  static constexpr uint32_t kRevealAnimationMs = 360U;
};
