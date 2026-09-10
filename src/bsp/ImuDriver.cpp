#include "ImuDriver.h"
#include <Arduino.h>
#include <Wire.h>
#include "IicBus.h"
#include "BoardConfig.h"
#ifndef OB_IMU_DRIVER_LOG_ENABLED
#define OB_IMU_DRIVER_LOG_ENABLED 1
#endif

namespace {
constexpr uint8_t kAddresses[] = {0x6A, 0x6B};
constexpr uint8_t kWho = 0x00;
constexpr uint8_t kCtrl1 = 0x02;
constexpr uint8_t kCtrl2 = 0x03;
constexpr uint8_t kCtrl3 = 0x04;
constexpr uint8_t kCtrl7 = 0x08;
constexpr uint8_t kData = 0x35;
constexpr uint8_t kWhoAmIValue = 0x05;
constexpr uint8_t kCtrl7Disabled = 0x00;
constexpr uint8_t kCtrl1Config = 0x60;
constexpr uint8_t kCtrl2Config = 0x26;
constexpr uint8_t kCtrl3Config = 0x56;
constexpr uint8_t kCtrl7Config = 0x03;
}

void ImuDriver::powerOn() {
  pinMode(BoardConfig::kPinSensorLdoEn, OUTPUT);
  digitalWrite(BoardConfig::kPinSensorLdoEn, HIGH);
}

bool ImuDriver::beginBus() {
  address_ = 0;
  whoAmI_ = 0;
  if (!IicBus::begin(400000U, true)) {
    return false;
  }

  for (const uint8_t address : kAddresses) {
    uint8_t whoAmI = 0;
    address_ = address;
    if (readRegister(kWho, whoAmI) && whoAmI == kWhoAmIValue) {
      whoAmI_ = whoAmI;
      return true;
    }
  }

  address_ = 0;
  return false;
}

bool ImuDriver::beginLowPower() {
  powerOn();
  if (!beginBus()) {
#if OB_IMU_DRIVER_LOG_ENABLED
    Serial.printf("[ERR][IMU] low power init failed stage=bus sensor_ldo=1\r\n");
#endif
    return false;
  }
  if (!writeRegister(kCtrl7, kCtrl7Disabled)) {
#if OB_IMU_DRIVER_LOG_ENABLED
    Serial.printf("[ERR][IMU] low power init failed stage=ctrl7 addr=0x%02X\r\n", address_);
#endif
    return false;
  }
#if OB_IMU_DRIVER_LOG_ENABLED
  Serial.printf("[IMU] low power addr=0x%02X who=0x%02X ctrl7=0x%02X sensor_ldo=1\r\n",
                address_, whoAmI_, kCtrl7Disabled);
#endif
  return true;
}

bool ImuDriver::configure() {
  return writeRegister(kCtrl1, kCtrl1Config) &&
         writeRegister(kCtrl2, kCtrl2Config) &&
         writeRegister(kCtrl3, kCtrl3Config) &&
         writeRegister(kCtrl7, kCtrl7Config);
}

bool ImuDriver::readSample(Sample&s){uint8_t r[12]={0};if(!readRegisters(kData,r,12))return false;auto v=[&](uint8_t i){return static_cast<int16_t>((static_cast<uint16_t>(r[i+1])<<8)|r[i]);};s.ax=v(0);s.ay=v(2);s.az=v(4);s.gx=v(6);s.gy=v(8);s.gz=v(10);return true;}

void ImuDriver::end() {
  if (address_ != 0U) {
    if (!writeRegister(kCtrl7, kCtrl7Disabled)) {
#if OB_IMU_DRIVER_LOG_ENABLED
      Serial.printf("[ERR][IMU] disable measurement failed addr=0x%02X\r\n", address_);
#endif
    }
  }
  powerOn();
#if OB_IMU_DRIVER_LOG_ENABLED
  Serial.printf("[IMU] measurement disabled ctrl7=0x%02X sensor_ldo=1\r\n", kCtrl7Disabled);
#endif
}

uint8_t ImuDriver::address()const{return address_;}uint8_t ImuDriver::whoAmI()const{return whoAmI_;}
bool ImuDriver::readRegister(uint8_t r,uint8_t&v){return readRegisters(r,&v,1);}bool ImuDriver::writeRegister(uint8_t r,uint8_t v){Wire.beginTransmission(address_);Wire.write(r);Wire.write(v);return Wire.endTransmission()==0;}
bool ImuDriver::readRegisters(uint8_t r,uint8_t*d,uint8_t n){Wire.beginTransmission(address_);Wire.write(r);if(Wire.endTransmission(false)!=0)return false;if(Wire.requestFrom(static_cast<int>(address_),static_cast<int>(n))!=n)return false;for(uint8_t i=0;i<n;++i){if(!Wire.available())return false;d[i]=static_cast<uint8_t>(Wire.read());}return true;}
