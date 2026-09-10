#include "ImuTestService.h"
#ifndef OB_IMU_TEST_LOG_ENABLED
#define OB_IMU_TEST_LOG_ENABLED 1
#endif
#if OB_IMU_TEST_LOG_ENABLED
#define IMU_LOG(f,...) Serial.printf("[SELFTEST][IMU] " f "\r\n",##__VA_ARGS__)
#define IMU_ERROR(f,...) Serial.printf("[ERR][SELFTEST][IMU] " f "\r\n",##__VA_ARGS__)
#else
#define IMU_LOG(f,...) ((void)0)
#define IMU_ERROR(f,...) ((void)0)
#endif

bool ImuTestService::begin() {
  if (!driver_.beginLowPower()) {
    IMU_ERROR("low power init failed");
    return false;
  }
  IMU_LOG("low power configured ctrl7=0x00 sensor_ldo=1");
  return true;
}

bool ImuTestService::start(uint32_t nowMs) {
  if (isBusy()) return false;
  driver_.powerOn(); state_ = State::Powering; nextMs_ = nowMs + 100U; count_ = 0;
  passed_ = false; first_ = {}; latest_ = {}; changed_ = true; return true;
}
void ImuTestService::tick(uint32_t nowMs) {
  if (state_ == State::Powering && static_cast<int32_t>(nowMs - nextMs_) >= 0) {
    if (!driver_.beginBus()) {
      IMU_ERROR("failed stage=bus");
      finish(false);
      return;
    }
    if (!driver_.configure()) {
      IMU_ERROR("failed stage=configure addr=0x%02X", driver_.address());
      finish(false);
      return;
    }
    state_ = State::Sampling; nextMs_ = nowMs + 20U; changed_ = true;
    IMU_LOG("ready addr=0x%02X who=0x%02X ctrl2=0x26 ctrl3=0x56 ctrl7=0x03",
            driver_.address(), driver_.whoAmI());
    return;
  }
  if (state_ != State::Sampling || static_cast<int32_t>(nowMs - nextMs_) < 0) return;
  nextMs_ = nowMs + 20U;
  if (!driver_.readSample(latest_)) { finish(false); return; }
  if (count_ == 0U) first_ = latest_;
  ++count_; changed_ = true;
  if (passed_ || count_ <= 25U) return;
  const int32_t activity = abs(latest_.ax-first_.ax)+abs(latest_.ay-first_.ay)+
      abs(latest_.az-first_.az)+abs(latest_.gx-first_.gx)+abs(latest_.gy-first_.gy)+abs(latest_.gz-first_.gz);
  const bool nonzero = latest_.ax || latest_.ay || latest_.az || latest_.gx || latest_.gy || latest_.gz;
  if (nonzero && activity > 8) { passed_ = true; IMU_LOG("passed samples=%lu continuing=1", static_cast<unsigned long>(count_)); }
}
void ImuTestService::finish(bool passed) {
  driver_.end(); passed_ = passed; state_ = passed ? State::Passed : State::Failed; changed_ = true;
  if (!passed) IMU_ERROR("failed samples=%lu addr=0x%02X who=0x%02X", static_cast<unsigned long>(count_), driver_.address(), driver_.whoAmI());
}
void ImuTestService::cancel() { if (isBusy()) driver_.end(); state_=State::Idle; changed_=true; }
ImuTestService::State ImuTestService::state()const{return state_;} bool ImuTestService::isBusy()const{return state_==State::Powering||state_==State::Sampling;}
bool ImuTestService::consumeChanged(){const bool c=changed_;changed_=false;return c;} bool ImuTestService::hasPassed()const{return passed_;}
uint8_t ImuTestService::address()const{return driver_.address();} uint8_t ImuTestService::whoAmI()const{return driver_.whoAmI();} uint32_t ImuTestService::sampleCount()const{return count_;} const ImuDriver::Sample&ImuTestService::latest()const{return latest_;}
