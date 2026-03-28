#include "DateUtils.h"

namespace DateUtils {

bool isLeapYear(uint16_t year) {
  if ((year % 400U) == 0U) return true;
  if ((year % 100U) == 0U) return false;
  return (year % 4U) == 0U;
}

uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2) {
    return static_cast<uint8_t>(isLeapYear(year) ? 29 : 28);
  }
  return kDays[month - 1];
}

void daysToDate(uint32_t daysFromEpoch, uint16_t epochYear, uint8_t epochWeekday,
                uint16_t& year, uint8_t& month, uint8_t& day, uint8_t& weekday) {
  weekday = static_cast<uint8_t>((epochWeekday + daysFromEpoch) % 7U);

  year = epochYear;
  uint32_t remainingDays = daysFromEpoch;
  while (true) {
    const uint16_t yearDays = static_cast<uint16_t>(isLeapYear(year) ? 366U : 365U);
    if (remainingDays < yearDays) {
      break;
    }
    remainingDays -= yearDays;
    ++year;
  }

  month = 1;
  while (month <= 12) {
    const uint8_t monthDays = daysInMonth(year, month);
    if (remainingDays < monthDays) {
      break;
    }
    remainingDays -= monthDays;
    ++month;
  }

  day = static_cast<uint8_t>(remainingDays + 1U);
}

}  // namespace DateUtils
