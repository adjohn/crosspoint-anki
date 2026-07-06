#include "TimeUtils.h"
#include <ctime>
#ifdef ARDUINO
#include <Wire.h>
#endif

namespace TimeUtils {

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

#ifdef ARDUINO
static int32_t readRtcEpochDays() {
    constexpr uint8_t DS3231_ADDR = 0x68;
    constexpr uint8_t DS3231_DAY_REG = 0x04;  // day, month(+century), year

    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(DS3231_DAY_REG);
    if (Wire.endTransmission() != 0) {
        return -1;
    }
    if (Wire.requestFrom(DS3231_ADDR, (uint8_t)3) != 3) {
        return -1;
    }
    const uint8_t dayReg = Wire.read();
    const uint8_t monthReg = Wire.read();
    const uint8_t yearReg = Wire.read();
    return ds3231DateToEpochDays(dayReg, monthReg, yearReg);
}
#endif

int32_t todayEpochDays(bool rtcAvailable) {
    const time_t now = time(nullptr);
    if (static_cast<int64_t>(now) > MIN_VALID_EPOCH) {
        return static_cast<int32_t>(now / 86400);
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
