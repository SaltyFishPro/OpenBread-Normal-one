#include "RemoteService.h"

#include "BluetoothService.h"

bool RemoteService::begin() {
  setState(State::Idle);
  triggerCount_ = 0;
  return true;
}

bool RemoteService::triggerCameraShutter(uint32_t nowMs, BluetoothService& bluetooth) {
  if (!bluetooth.isConnected()) {
    setState(State::NotConnected);
    return false;
  }

  if (!bluetooth.triggerCameraShutter(nowMs)) {
    setState(State::Busy);
    return false;
  }

  ++triggerCount_;
  setState(State::Sent);
  return true;
}

RemoteService::State RemoteService::state() const { return state_; }

uint32_t RemoteService::triggerCount() const { return triggerCount_; }

void RemoteService::setState(State state) { state_ = state; }
