#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "src/services/RacingSteeringService.h"

namespace {
int16_t sampleAy = 0;
bool failRead = false;
uint32_t stopCount = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "FAIL: %s\n", message);
  std::exit(1);
}
}  // namespace

void ImuDriver::powerOn() {}
bool ImuDriver::beginBus() { return true; }
bool ImuDriver::configureAccelerometer() { return true; }
bool ImuDriver::readAcceleration(int16_t& ax, int16_t& ay, int16_t& az) {
  if (failRead) return false;
  ax = 0;
  ay = sampleAy;
  az = 4096;
  return true;
}
void ImuDriver::end() { ++stopCount; }

int main() {
  RacingSteeringService service;
  check(service.start(0), "IMU did not start");
  for (uint32_t time = 80; time <= 220; time += 20) service.tick(time);
  check(service.steering() == 0.0f, "neutral calibration drifted");

  sampleAy = 100;
  for (uint32_t time = 240; time <= 400; time += 20) service.tick(time);
  check(service.steering() == 0.0f, "dead zone failed");

  sampleAy = 2000;
  for (uint32_t time = 420; time <= 700; time += 20) service.tick(time);
  check(service.steering() > 0.9f, "right tilt did not steer right");

  sampleAy = -2000;
  for (uint32_t time = 720; time <= 1040; time += 20) service.tick(time);
  check(service.steering() < -0.9f, "left tilt did not steer left");

  failRead = true;
  for (uint32_t time = 1060; time <= 1100; time += 20) service.tick(time);
  check(service.steering() == 0.0f && stopCount == 1, "read failure did not release IMU");
  std::puts("PASS: neutral, dead zone, left/right tilt, read failure fallback");
  return 0;
}
