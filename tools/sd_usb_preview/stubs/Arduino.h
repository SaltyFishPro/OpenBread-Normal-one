#pragma once
#include <cstdint>
#include <vector>
#define OUTPUT 1
#define HIGH 1
#define LOW 0

namespace TestGpio {
struct Event { bool mode; int pin; int value; };
extern std::vector<Event> events;
}
inline void pinMode(int pin, int mode) { TestGpio::events.push_back({true, pin, mode}); }
inline void digitalWrite(int pin, int value) { TestGpio::events.push_back({false, pin, value}); }
