#pragma once

#include <stdint.h>

class DisplayMonoTft;

// Shared black title bar used by the detail pages. Previously copy-pasted in
// TimeCalibrationPage, GamesPage, RemotePage and SettingsPage.
namespace DetailHeader {

constexpr int16_t kHeight = 28;

void render(DisplayMonoTft& display, const char* title, int16_t yOffset);

}  // namespace DetailHeader
