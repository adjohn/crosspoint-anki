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
    // compiled out off-device).
    const int32_t expected = static_cast<int32_t>(time(nullptr) / 86400);
    const int32_t got = TimeUtils::todayEpochDays(false);
    CHECK(got == expected || got == expected + 1);  // midnight race tolerance
    CHECK(TimeUtils::todayEpochDays(true) >= expected);
    CHECK(got > TimeUtils::daysFromCivil(2020, 1, 1));
}

int main() {
    testBcdToDec();
    testDaysFromCivil();
    testDs3231Decode();
    testTodayEpochDays();

    std::printf("test_timeutils: %d checks, %d failed\n", testsRun, testsFailed);
    return testsFailed == 0 ? 0 : 1;
}
