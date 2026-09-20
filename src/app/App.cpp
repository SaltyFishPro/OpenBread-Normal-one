#include "App.h"

#include "../bsp/UsbSerial.h"

void App::begin() {
  // 串口最早初始化，保证后续模块的开机日志可见。
  UsbSerial::begin();
  uiManager_.begin();
}

void App::tick() { uiManager_.tick(); }
