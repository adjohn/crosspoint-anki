#include "TimeUtils.h"
#include <ctime>
#ifdef ARDUINO
#include <Wire.h>
#endif

namespace TimeUtils {

static int tzOffsetMinutes = 0;

void setTimezoneOffsetMinutes(int minutes) {
    if (minutes < TZ_OFFSET_MIN) minutes = TZ_OFFSET_MIN;
    if (minutes > TZ_OFFSET_MAX) minutes = TZ_OFFSET_MAX;
    tzOffsetMinutes = minutes;
}

int timezoneOffsetMinutes() {
    return tzOffsetMinutes;
}

int32_t epochSecondsToLocalDays(int64_t epochSeconds) {
    const int64_t shifted = epochSeconds + static_cast<int64_t>(tzOffsetMinutes) * 60;
    int64_t days = shifted / 86400;
    if (shifted % 86400 < 0) {
        days -= 1;  // floor division for pre-1970 instants
    }
    return static_cast<int32_t>(days);
}

int bcdToDec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

int32_t daysFromCivil(int year, int month, int day) {
    year -= month <= 2;
    const int32_t era = (year >= 0 ? year : year - 399) / 400;
    const uint32_t yoe = static_cast<uint32_t>(year - era * 400);              // [0, 399]
    const uint32_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;  // [0, 365]
    const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                // [0, 146096]
    return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

int32_t ds3231DateToEpochDays(uint8_t dayReg, uint8_t monthReg, uint8_t yearReg) {
    const int day = bcdToDec(dayReg & 0x3F);
    const int month = bcdToDec(monthReg & 0x1F);
    const int year = 2000 + bcdToDec(yearReg) + ((monthReg & 0x80) ? 100 : 0);
    if (year < 2020 || year > 2099) {
        return -1;
    }
    if (month < 1 || month > 12 || day < 1 || day > 31) {
        return -1;
    }
    return daysFromCivil(year, month, day);
}

int32_t ds3231TimeToSecondsOfDay(uint8_t secReg, uint8_t minReg, uint8_t hourReg) {
    const int sec = bcdToDec(secReg & 0x7F);
    const int min = bcdToDec(minReg & 0x7F);
    int hour;
    if (hourReg & 0x40) {
        // 12-hour mode: bits 4-0 hold 1-12, bit 5 is the PM flag
        hour = bcdToDec(hourReg & 0x1F) % 12;
        if (hourReg & 0x20) {
            hour += 12;
        }
    } else {
        hour = bcdToDec(hourReg & 0x3F);
    }
    if (sec > 59 || min > 59 || hour > 23) {
        return -1;
    }
    return hour * 3600 + min * 60 + sec;
}

#ifdef ARDUINO
// Convention: the DS3231 is treated as storing UTC (CrossPoint sets it from
// NTP), same as the system clock. The full UTC instant is reconstructed from
// the time+date registers and the timezone offset is applied when converting
// to days, so both clock sources roll the day at the same local midnight.
static int32_t readRtcEpochDays() {
    constexpr uint8_t DS3231_ADDR = 0x68;
    constexpr uint8_t DS3231_SEC_REG = 0x00;  // sec, min, hour, dow, day, month(+century), year

    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(DS3231_SEC_REG);
    if (Wire.endTransmission() != 0) {
        return -1;
    }
    if (Wire.requestFrom(DS3231_ADDR, (uint8_t)7) != 7) {
        return -1;
    }
    const uint8_t secReg = Wire.read();
    const uint8_t minReg = Wire.read();
    const uint8_t hourReg = Wire.read();
    Wire.read();  // day-of-week, unused
    const uint8_t dayReg = Wire.read();
    const uint8_t monthReg = Wire.read();
    const uint8_t yearReg = Wire.read();

    const int32_t utcDays = ds3231DateToEpochDays(dayReg, monthReg, yearReg);
    if (utcDays < 0) {
        return -1;
    }
    const int32_t secondsOfDay = ds3231TimeToSecondsOfDay(secReg, minReg, hourReg);
    if (secondsOfDay < 0) {
        return -1;
    }
    return epochSecondsToLocalDays(static_cast<int64_t>(utcDays) * 86400 + secondsOfDay);
}
#endif

int32_t todayEpochDays(bool rtcAvailable) {
    const time_t now = time(nullptr);
    if (static_cast<int64_t>(now) > MIN_VALID_EPOCH) {
        return epochSecondsToLocalDays(static_cast<int64_t>(now));
    }
#ifdef ARDUINO
    if (rtcAvailable) {
        return readRtcEpochDays();
    }
#else
    (void)rtcAvailable;
#endif
    return -1;
}

}
