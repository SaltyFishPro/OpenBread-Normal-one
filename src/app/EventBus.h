#pragma once

enum class AppEventType {
  None,
  Input,
  Timer,
  RtcAlarm,
  ImuGesture,
  SdMounted,
  AudioState,
  Navigation
};

struct AppEvent {
  AppEventType type = AppEventType::None;
  int32_t arg0 = 0;
  int32_t arg1 = 0;
};