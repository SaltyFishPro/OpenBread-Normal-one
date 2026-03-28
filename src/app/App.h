#pragma once

#include "../ui/UiManager.h"

class App {
public:
  void begin();
  void tick();

private:
  UiManager uiManager_;
};
