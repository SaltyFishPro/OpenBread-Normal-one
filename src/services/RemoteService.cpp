#include "RemoteService.h"

#include "BluetoothService.h"

bool RemoteService::begin() {
  setState(State::Idle);
  triggerCount_ = 0;
  pagerState_ = PagerState::Idle;
  pagerUpCount_ = 0;
  pagerDownCount_ = 0;
  pagerPauseCount_ = 0;
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

bool RemoteService::triggerPagerUp(uint32_t nowMs, BluetoothService& bluetooth) {
  if (!bluetooth.isConnected()) {
    pagerState_ = PagerState::NotConnected;
    return false;
  }
  if (!bluetooth.triggerPageUp(nowMs)) {
    pagerState_ = PagerState::Busy;
    return false;
  }
  ++pagerUpCount_;
  pagerState_ = PagerState::UpSent;
  return true;
}

bool RemoteService::triggerPagerDown(uint32_t nowMs, BluetoothService& bluetooth) {
  if (!bluetooth.isConnected()) {
    pagerState_ = PagerState::NotConnected;
    return false;
  }
  if (!bluetooth.triggerPageDown(nowMs)) {
    pagerState_ = PagerState::Busy;
    return false;
  }
  ++pagerDownCount_;
  pagerState_ = PagerState::DownSent;
  return true;
}

bool RemoteService::triggerPagerPause(uint32_t nowMs, BluetoothService& bluetooth) {
  if (!bluetooth.isConnected()) {
    pagerState_ = PagerState::NotConnected;
    return false;
  }
  if (!bluetooth.triggerPlayPause(nowMs)) {
    pagerState_ = PagerState::Busy;
    return false;
  }
  ++pagerPauseCount_;
  pagerState_ = PagerState::Paused;
  return true;
}

RemoteService::PagerState RemoteService::pagerState() const { return pagerState_; }

uint32_t RemoteService::pagerUpCount() const { return pagerUpCount_; }

uint32_t RemoteService::pagerDownCount() const { return pagerDownCount_; }

uint32_t RemoteService::pagerPauseCount() const { return pagerPauseCount_; }

void RemoteService::setState(State state) { state_ = state; }
