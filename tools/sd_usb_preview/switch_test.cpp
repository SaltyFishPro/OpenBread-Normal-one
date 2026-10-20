#include <Arduino.h>
#include <cstdio>
#include <stdexcept>

#include "src/bsp/SdUsbSwitch.h"

std::vector<TestGpio::Event> TestGpio::events;

int main() {
  try {
    SdUsbSwitch route;
    route.begin();
    if (route.enabled()) throw std::runtime_error("Bridge enabled at boot");
    // Repeated button/redraw events must not produce extra pin writes.
    route.setEnabled(false);
    for (int i = 0; i < 3; ++i) {
      route.setEnabled(true);
      route.setEnabled(true);
      if (!route.enabled()) throw std::runtime_error("Enter did not enable bridge");
      route.setEnabled(false);
      route.setEnabled(false);
      if (route.enabled()) throw std::runtime_error("Exit did not disable bridge");
    }
    if (TestGpio::events.size() != 8U) throw std::runtime_error("Unexpected GPIO side effects");
    for (size_t i = 0; i < TestGpio::events.size(); ++i) {
      const auto& event = TestGpio::events[i];
      const int expected = i == 0U ? OUTPUT : (i >= 2U && i % 2U == 0U ? HIGH : LOW);
      if (event.pin != 16 || event.mode != (i == 0U) || event.value != expected)
        throw std::runtime_error("Wrong pin, boot level or enter/exit sequence");
    }
    route.setEnabled(true);
    route.begin();
    if (route.enabled() || TestGpio::events.back().value != LOW)
      throw std::runtime_error("Reinitialization did not restore LOW");
    std::puts("PASS: GPIO16 boot LOW, repeated entry HIGH/exit LOW, idempotency and reinitialization");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
