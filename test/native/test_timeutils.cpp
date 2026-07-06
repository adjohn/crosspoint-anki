// Native unit tests for firmware/src/utils/TimeUtils.{h,cpp}: the pure
// BCD/civil-date helpers plus todayEpochDays() host behavior (the DS3231
// Wire path is ARDUINO-guarded and exercised via ds3231DateToEpochDays).
// Build/run: test/native/run.sh
#include "utils/TimeUtils.h"

#include <cstdio>
#include <ctime>

static int testsRun = 0;
static int testsFailed = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        ++testsRun;                                                            \
        if (!(cond)) {                                                         \
            ++testsFailed;                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
        }                                                                      \
    } while (0)

static void testBcdToDec() {
    CHECK(TimeUtils::bcdToDec(0x00) == 0);
    CHECK(TimeUtils::bcdToDec(0x09) == 9);
    CHECK(TimeUtils::bcdToDec(0x10) == 10);
    CHECK(TimeUtils::bcdToDec(0x26) == 26);
    CHECK(TimeUtils::bcdToDec(0x59) == 59);
    CHECK(TimeUtils::bcdToDec(0x99) == 99);
}

static void testDaysFromCivil() {
    // Reference values cross-checked against Python datetime.date
    CHECK(TimeUtils::daysFromCivil(1970, 1, 1) == 0);
    CHECK(TimeUtils::daysFromCivil(1970, 1, 2) == 1);
    CHECK(TimeUtils::daysFromCivil(1969, 12, 31) == -1);
    CHECK(TimeUtils::daysFromCivil(2000, 3, 1) == 11017);
    CHECK(TimeUtils::daysFromCivil(2020, 1, 1) == 18262);
    CHECK(TimeUtils::daysFromCivil(2024, 2, 29) == 19782);  // leap day
    CHECK(TimeUtils::daysFromCivil(2026, 7, 6) == 20640);
    CHECK(TimeUtils::daysFromCivil(2099, 12, 31) == 47481);

    // Consecutive days differ by exactly 1 across month/year boundaries
    CHECK(TimeUtils::daysFromCivil(2026, 3, 1) - TimeUtils::daysFromCivil(2026, 2, 28) == 1);
    CHECK(TimeUtils::daysFromCivil(2027, 1, 1) - TimeUtils::daysFromCivil(2026, 12, 31) == 1);
    CHECK(TimeUtils::daysFromCivil(2024, 3, 1) - TimeUtils::daysFromCivil(2024, 2, 29) == 1);
}

static void testDs3231Decode() {
    // 2026-07-06: day=0x06, month=0x07, year=0x26 (all BCD)
    CHECK(TimeUtils::ds3231DateToEpochDays(0x06, 0x07, 0x26) == 20640);
    // 2020-01-01
    CHECK(TimeUtils::ds3231DateToEpochDays(0x01, 0x01, 0x20) == 18262);
    // Two-digit BCD fields: 2031-12-25
    CHECK(TimeUtils::ds3231DateToEpochDays(0x25, 0x12, 0x31) ==
          TimeUtils::daysFromCivil(2031, 12, 25));

    // Century bit (month reg bit 7) pushes the year past 2099: rejected
    CHECK(TimeUtils::ds3231DateToEpochDays(0x06, (uint8_t)(0x80 | 0x07), 0x26) == -1);
    // Unset RTC (fresh battery): 2000-01-01 is before 2020 -> rejected
    CHECK(TimeUtils::ds3231DateToEpochDays(0x01, 0x01, 0x00) == -1);
    // 2019 -> rejected (sanity window is 2020..2099)
    CHECK(TimeUtils::ds3231DateToEpochDays(0x01, 0x01, 0x19) == -1);
    // Garbage month/day
    CHECK(TimeUtils::ds3231DateToEpochDays(0x00, 0x07, 0x26) == -1);  // day 0
    CHECK(TimeUtils::ds3231DateToEpochDays(0x06, 0x00, 0x26) == -1);  // month 0
    CHECK(TimeUtils::ds3231DateToEpochDays(0x32, 0x13, 0x26) == -1);  // month 13
}

static void testTodayEpochDays() {
    // On the host the system clock is valid, so todayEpochDays() must agree
    // with time() regardless of the rtcAvailable flag (the DS3231 path is
    // compiled out off-device). Offset is 0 here (default / restored).
    const int32_t expected = static_cast<int32_t>(time(nullptr) / 86400);
    const int32_t got = TimeUtils::todayEpochDays(false);
    CHECK(got == expected || got == expected + 1);  // midnight race tolerance
    CHECK(TimeUtils::todayEpochDays(true) >= expected);
    CHECK(got > TimeUtils::daysFromCivil(2020, 1, 1));
}

static void testTimezoneOffsetClamp() {
    CHECK(TimeUtils::timezoneOffsetMinutes() == 0);  // process default
    TimeUtils::setTimezoneOffsetMinutes(330);        // UTC+5:30
    CHECK(TimeUtils::timezoneOffsetMinutes() == 330);
    TimeUtils::setTimezoneOffsetMinutes(-210);       // UTC-3:30
    CHECK(TimeUtils::timezoneOffsetMinutes() == -210);

    // Exact bounds are representable
    TimeUtils::setTimezoneOffsetMinutes(TimeUtils::TZ_OFFSET_MIN);
    CHECK(TimeUtils::timezoneOffsetMinutes() == -720);
    TimeUtils::setTimezoneOffsetMinutes(TimeUtils::TZ_OFFSET_MAX);
    CHECK(TimeUtils::timezoneOffsetMinutes() == 840);

    // Out-of-range values clamp, never wrap
    TimeUtils::setTimezoneOffsetMinutes(-721);
    CHECK(TimeUtils::timezoneOffsetMinutes() == -720);
    TimeUtils::setTimezoneOffsetMinutes(841);
    CHECK(TimeUtils::timezoneOffsetMinutes() == 840);
    TimeUtils::setTimezoneOffsetMinutes(-1000000);
    CHECK(TimeUtils::timezoneOffsetMinutes() == -720);
    TimeUtils::setTimezoneOffsetMinutes(1000000);
    CHECK(TimeUtils::timezoneOffsetMinutes() == 840);

    TimeUtils::setTimezoneOffsetMinutes(0);
    CHECK(TimeUtils::timezoneOffsetMinutes() == 0);
}

static void testLocalDayBoundary() {
    const int32_t D = TimeUtils::daysFromCivil(2026, 7, 6);  // 20640
    const int64_t base = static_cast<int64_t>(D) * 86400;

    // 23:30 UTC: offset 0 keeps the day, +60 rolls it, +30 rolls it exactly
    // at local midnight, -30 keeps it
    const int64_t s2330 = base + 23 * 3600 + 30 * 60;
    TimeUtils::setTimezoneOffsetMinutes(0);
    CHECK(TimeUtils::epochSecondsToLocalDays(s2330) == D);
    TimeUtils::setTimezoneOffsetMinutes(60);
    CHECK(TimeUtils::epochSecondsToLocalDays(s2330) == D + 1);
    TimeUtils::setTimezoneOffsetMinutes(30);
    CHECK(TimeUtils::epochSecondsToLocalDays(s2330) == D + 1);  // 24:00 local
    CHECK(TimeUtils::epochSecondsToLocalDays(s2330 - 1) == D);  // 23:59:59 local
    TimeUtils::setTimezoneOffsetMinutes(-30);
    CHECK(TimeUtils::epochSecondsToLocalDays(s2330) == D);

    // 00:30 UTC with a negative offset falls back to the previous day
    const int64_t s0030 = base + 30 * 60;
    TimeUtils::setTimezoneOffsetMinutes(-60);
    CHECK(TimeUtils::epochSecondsToLocalDays(s0030) == D - 1);
    TimeUtils::setTimezoneOffsetMinutes(-30);
    CHECK(TimeUtils::epochSecondsToLocalDays(s0030) == D);  // exactly local 00:00

    // India (UTC+5:30): the day rolls at 18:30 UTC
    TimeUtils::setTimezoneOffsetMinutes(330);
    CHECK(TimeUtils::epochSecondsToLocalDays(base + 18 * 3600 + 30 * 60) == D + 1);
    CHECK(TimeUtils::epochSecondsToLocalDays(base + 18 * 3600 + 29 * 60 + 59) == D);

    // Extreme clamped offsets: UTC+14 rolls at 10:00 UTC, UTC-12 rolls back
    // for anything before 12:00 UTC
    TimeUtils::setTimezoneOffsetMinutes(840);
    CHECK(TimeUtils::epochSecondsToLocalDays(base + 10 * 3600) == D + 1);
    CHECK(TimeUtils::epochSecondsToLocalDays(base + 10 * 3600 - 1) == D);
    TimeUtils::setTimezoneOffsetMinutes(-720);
    CHECK(TimeUtils::epochSecondsToLocalDays(base + 12 * 3600) == D);
    CHECK(TimeUtils::epochSecondsToLocalDays(base + 12 * 3600 - 1) == D - 1);

    // Floor division for pre-1970 instants (defensive; never hit on-device)
    TimeUtils::setTimezoneOffsetMinutes(0);
    CHECK(TimeUtils::epochSecondsToLocalDays(-1) == -1);
    CHECK(TimeUtils::epochSecondsToLocalDays(0) == 0);
    TimeUtils::setTimezoneOffsetMinutes(60);
    CHECK(TimeUtils::epochSecondsToLocalDays(-3600) == 0);   // shifts to 0
    CHECK(TimeUtils::epochSecondsToLocalDays(-3601) == -1);

    // todayEpochDays applies the same offset as epochSecondsToLocalDays
    TimeUtils::setTimezoneOffsetMinutes(840);
    const int32_t viaHelper =
        TimeUtils::epochSecondsToLocalDays(static_cast<int64_t>(time(nullptr)));
    const int32_t viaToday = TimeUtils::todayEpochDays(false);
    CHECK(viaToday == viaHelper || viaToday == viaHelper + 1);  // midnight race
    TimeUtils::setTimezoneOffsetMinutes(0);
}

static void testDs3231TimeDecode() {
    // 24-hour mode
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x00, 0x00, 0x00) == 0);
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x59, 0x59, 0x23) == 86399);
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x30, 0x15, 0x07) ==
          7 * 3600 + 15 * 60 + 30);
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x00, 0x30, 0x23) ==
          23 * 3600 + 30 * 60);

    // 12-hour mode (bit 6 set; bit 5 = PM): 12AM->0, 12PM->12, wraps correct
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x00, 0x00, 0x40 | 0x12) == 0);           // 12:00 AM
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x00, 0x00, 0x40 | 0x20 | 0x12) == 12 * 3600);  // 12:00 PM
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x00, 0x00, 0x40 | 0x01) == 3600);        // 1:00 AM
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x00, 0x00, 0x40 | 0x20 | 0x11) == 23 * 3600);  // 11:00 PM
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x45, 0x30, 0x40 | 0x20 | 0x06) ==
          18 * 3600 + 30 * 60 + 45);  // 6:30:45 PM

    // Implausible fields rejected
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x60, 0x00, 0x00) == -1);  // sec 60
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x00, 0x60, 0x00) == -1);  // min 60
    CHECK(TimeUtils::ds3231TimeToSecondsOfDay(0x00, 0x00, 0x24) == -1);  // hour 24
}

// Host mirror of the ARDUINO-only readRtcEpochDays() in TimeUtils.cpp: the
// DS3231 registers are treated as a UTC instant and the timezone offset is
// applied via epochSecondsToLocalDays — the documented convention. Keep in
// sync with TimeUtils.cpp.
static int32_t rtcMirror(uint8_t secReg, uint8_t minReg, uint8_t hourReg,
                         uint8_t dayReg, uint8_t monthReg, uint8_t yearReg) {
    const int32_t utcDays = TimeUtils::ds3231DateToEpochDays(dayReg, monthReg, yearReg);
    if (utcDays < 0) return -1;
    const int32_t secondsOfDay = TimeUtils::ds3231TimeToSecondsOfDay(secReg, minReg, hourReg);
    if (secondsOfDay < 0) return -1;
    return TimeUtils::epochSecondsToLocalDays(
        static_cast<int64_t>(utcDays) * 86400 + secondsOfDay);
}

static void testRtcPathConsistency() {
    const int32_t D = TimeUtils::daysFromCivil(2026, 7, 6);

    // 2026-07-06 23:30:00 UTC on the RTC
    TimeUtils::setTimezoneOffsetMinutes(0);
    CHECK(rtcMirror(0x00, 0x30, 0x23, 0x06, 0x07, 0x26) == D);
    TimeUtils::setTimezoneOffsetMinutes(60);
    CHECK(rtcMirror(0x00, 0x30, 0x23, 0x06, 0x07, 0x26) == D + 1);  // rolls
    TimeUtils::setTimezoneOffsetMinutes(-720);
    CHECK(rtcMirror(0x00, 0x30, 0x23, 0x06, 0x07, 0x26) == D);      // 11:30 local

    // 00:15 UTC with a negative offset is still "yesterday" locally — the
    // RTC path must agree with the system-clock path, not truncate to date
    TimeUtils::setTimezoneOffsetMinutes(-60);
    CHECK(rtcMirror(0x00, 0x15, 0x00, 0x06, 0x07, 0x26) == D - 1);

    // 12h-mode registers land on the same local day as their 24h equivalent
    TimeUtils::setTimezoneOffsetMinutes(60);
    CHECK(rtcMirror(0x00, 0x30, 0x40 | 0x20 | 0x11, 0x06, 0x07, 0x26) ==
          rtcMirror(0x00, 0x30, 0x23, 0x06, 0x07, 0x26));

    // Bad date or bad time each poison the whole read
    CHECK(rtcMirror(0x00, 0x30, 0x23, 0x01, 0x01, 0x00) == -1);  // year 2000
    CHECK(rtcMirror(0x00, 0x60, 0x23, 0x06, 0x07, 0x26) == -1);  // min 60

    TimeUtils::setTimezoneOffsetMinutes(0);
}

int main() {
    testBcdToDec();
    testDaysFromCivil();
    testDs3231Decode();
    testTodayEpochDays();
    testTimezoneOffsetClamp();
    testLocalDayBoundary();
    testDs3231TimeDecode();
    testRtcPathConsistency();

    std::printf("test_timeutils: %d checks, %d failed\n", testsRun, testsFailed);
    return testsFailed == 0 ? 0 : 1;
}
