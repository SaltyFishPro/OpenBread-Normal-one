#pragma once

#include <Arduino.h>

namespace DateUtils {

bool isLeapYear(uint16_t year);
uint8_t daysInMonth(uint16_t year, uint8_t month);
void daysToDate(uint32_t daysFromEpoch, uint16_t epochYear, uint8_t epochWeekday,
                uint16_t& year, uint8_t& month, uint8_t& day, uint8_t& weekday);

}  // namespace DateUtils
