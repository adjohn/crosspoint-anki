#pragma once
#include <cstdint>

// Wall-clock date access with graceful degradation. The X4 has no RTC, so
// the only clock is the ESP32 system time (set by CrossPoint via NTP and
// surviving esp_restart, but not deep-sleep power-off or cold boot). The X3
// additionally has a DS3231 RTC on the I2C bus that HalGPIO::begin() brings
// up. When neither source is trustworthy, callers get -1 and must degrade.
namespace TimeUtils {
    // Any epoch second before 2020-01-01T00:00:00Z is treated as "unset"
    constexpr int64_t MIN_VALID_EPOCH = 1577836800LL;

    // Timezone offset bounds (UTC-12:00 .. UTC+14:00), in minutes
    constexpr int TZ_OFFSET_MIN = -720;
    constexpr int TZ_OFFSET_MAX = 840;

    // Process-wide timezone offset used to place the local day boundary.
    // Loaded from NVS at boot, updated from the main menu or a deck upload.
    // Values are clamped to [TZ_OFFSET_MIN, TZ_OFFSET_MAX]; default 0 (UTC).
    void setTimezoneOffsetMinutes(int minutes);
    int timezoneOffsetMinutes();

    // Pure helpers (host-testable, no hardware access)
    int bcdToDec(uint8_t bcd);
    // Howard Hinnant's days_from_civil: proleptic Gregorian date -> days
    // since 1970-01-01
    int32_t daysFromCivil(int year, int month, int day);
    // Decode DS3231 date registers 0x04 (day), 0x05 (month + century bit 7),
    // 0x06 (year 00-99, base 2000) to epoch days. Returns -1 when the
    // decoded date is implausible (outside 2020..2099 or invalid fields).
    int32_t ds3231DateToEpochDays(uint8_t dayReg, uint8_t monthReg, uint8_t yearReg);
    // Decode DS3231 time registers 0x00 (seconds), 0x01 (minutes), 0x02
    // (hours; handles 12h mode via bits 6/5) to seconds of day, or -1 when
    // the fields are implausible.
    int32_t ds3231TimeToSecondsOfDay(uint8_t secReg, uint8_t minReg, uint8_t hourReg);
    // UTC epoch seconds -> local epoch days: floor((s + offset*60) / 86400)
    int32_t epochSecondsToLocalDays(int64_t epochSeconds);

    // Local days since 1970-01-01 for "today" (UTC epoch time shifted by the
    // configured timezone offset), or -1 when no trustworthy clock exists.
    // rtcAvailable: pass gpio.deviceIsX3() — whether the DS3231 can be
    // consulted when system time is unset.
    int32_t todayEpochDays(bool rtcAvailable);
}
