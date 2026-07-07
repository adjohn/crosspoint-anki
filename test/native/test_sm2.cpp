// Native unit tests for the SM-2 scheduler (firmware/src/scheduling/SM2.{h,cpp})
// plus a mirror of how ReviewActivity::processRating() applies it,
// including due-date arithmetic.
// Build/run: test/native/run.sh
#include "scheduling/SM2.h"

#include <cmath>
#include <cstdio>
#include <cstdint>

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

static bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}

// Mirrors the scheduling core of ReviewActivity::processRating(): ease first,
// then repetitions, then the interval from the post-update repetition count
// and the previous interval, then the due date.
struct CardState {
    float ease = 2.5f;
    int interval = 0;
    int repetitions = 0;
    int32_t due = 0;  // epoch days; 0 = always due
};

static void applyRating(CardState& cp, SM2::Quality quality, int32_t today = -1) {
    cp.ease = SM2::updateEaseFactor(cp.ease, quality);
    if (quality >= SM2::GOOD) {
        cp.repetitions++;
    } else {
        cp.repetitions = 0;
    }
    cp.interval = SM2::calculateInterval(cp.repetitions, cp.interval, cp.ease);

    if (today >= 0) {
        cp.due = today + cp.interval;
    } else {
        cp.due = 0;
    }
}

static void testCalculateInterval() {
    // SM-2 spec: I(1)=1, I(2)=6, I(n)=round(I(n-1)*EF); lapse restarts at 1
    CHECK(SM2::calculateInterval(0, 0, 2.5f) == 1);    // lapse
    CHECK(SM2::calculateInterval(0, 40, 2.5f) == 1);   // lapse ignores prev
    CHECK(SM2::calculateInterval(-3, 12, 2.5f) == 1);
    CHECK(SM2::calculateInterval(1, 0, 2.5f) == 1);
    CHECK(SM2::calculateInterval(1, 0, 1.3f) == 1);    // EF-independent
    CHECK(SM2::calculateInterval(2, 1, 2.5f) == 6);
    CHECK(SM2::calculateInterval(2, 1, 1.3f) == 6);    // EF-independent
    CHECK(SM2::calculateInterval(3, 6, 2.5f) == 15);   // round(6*2.5)
    CHECK(SM2::calculateInterval(4, 15, 2.5f) == 38);  // round(37.5)
    CHECK(SM2::calculateInterval(3, 6, 1.3f) == 8);    // round(7.8)
    CHECK(SM2::calculateInterval(4, 8, 1.3f) == 10);   // round(10.4)

    // The next interval only depends on the PREVIOUS interval, not on
    // recomputing the whole ladder from the current ease
    CHECK(SM2::calculateInterval(7, 30, 1.3f) == 39);  // round(39.0)

    // Monotonic growth guard: never shrinks, even from a migrated interval 0
    CHECK(SM2::calculateInterval(5, 0, 1.3f) == 1);
    CHECK(SM2::calculateInterval(3, 1, 1.3f) == 2);    // round(1.3)=1 bumped to 2
    int prev = 6;
    for (int r = 3; r <= 14; ++r) {
        int cur = SM2::calculateInterval(r, prev, 1.3f);
        CHECK(cur > prev);
        prev = cur;
    }
}

static void testUpdateEaseFactor() {
    // EF' = EF + (0.1 - (5-q)*(0.08 + (5-q)*0.02))
    CHECK(approx(SM2::updateEaseFactor(2.5f, SM2::EASY), 2.6f));    // q=5: +0.10
    CHECK(approx(SM2::updateEaseFactor(2.5f, SM2::GOOD), 2.36f));   // q=3: -0.14
    CHECK(approx(SM2::updateEaseFactor(2.5f, SM2::HARD), 2.18f));   // q=2: -0.32
    CHECK(approx(SM2::updateEaseFactor(2.5f, SM2::AGAIN), 1.7f));   // q=0: -0.80

    // Floor at 1.3
    CHECK(approx(SM2::updateEaseFactor(1.3f, SM2::AGAIN), 1.3f));
    CHECK(approx(SM2::updateEaseFactor(1.3f, SM2::GOOD), 1.3f));    // 1.16 clamped
    CHECK(approx(SM2::updateEaseFactor(1.35f, SM2::HARD), 1.3f));   // 1.03 clamped
    CHECK(approx(SM2::updateEaseFactor(1.3f, SM2::EASY), 1.4f));    // floor doesn't stick

    // Repeated failures never push EF below the floor
    float ef = 2.5f;
    for (int i = 0; i < 10; ++i) {
        ef = SM2::updateEaseFactor(ef, SM2::AGAIN);
        CHECK(ef >= 1.3f - 1e-6f);
    }
    CHECK(approx(ef, 1.3f));
}

static void testFirstReviewPerGrade() {
    // First review of a brand-new card (reps=0), one case per grade.
    {
        CardState cp;
        applyRating(cp, SM2::AGAIN);
        CHECK(cp.repetitions == 0);
        CHECK(cp.interval == 1);       // lapse: retry tomorrow
        CHECK(approx(cp.ease, 1.7f));
    }
    {
        CardState cp;
        applyRating(cp, SM2::HARD);    // q=2 < GOOD: counts as a lapse
        CHECK(cp.repetitions == 0);
        CHECK(cp.interval == 1);
        CHECK(approx(cp.ease, 2.18f));
    }
    {
        CardState cp;
        applyRating(cp, SM2::GOOD);
        CHECK(cp.repetitions == 1);
        CHECK(cp.interval == 1);       // first success: 1 day
        CHECK(approx(cp.ease, 2.36f));
    }
    {
        CardState cp;
        applyRating(cp, SM2::EASY);
        CHECK(cp.repetitions == 1);
        CHECK(cp.interval == 1);
        CHECK(approx(cp.ease, 2.6f));
    }
}

static void testIntervalGrowthOverRepeatedGood() {
    // Canonical incremental ladder: 1, 6, then round(prev*EF), with Good
    // decaying ease 2.36, 2.22, 2.08, 1.94, 1.80, 1.66, 1.52, 1.38
    CardState cp;
    const int expected[8] = {1, 6, 12, 23, 41, 68, 103, 142};
    int prev = 0;
    for (int i = 0; i < 8; ++i) {
        applyRating(cp, SM2::GOOD);
        CHECK(cp.repetitions == i + 1);
        CHECK(cp.interval == expected[i]);
        CHECK(cp.interval > prev);  // strictly monotonic under repeated Good
        prev = cp.interval;
    }
}

static void testAgainResetsRepetitions() {
    CardState cp;
    applyRating(cp, SM2::GOOD);
    applyRating(cp, SM2::GOOD);
    applyRating(cp, SM2::GOOD);
    CHECK(cp.repetitions == 3);
    CHECK(cp.interval == 12);

    // Lapse: repetitions and interval both reset
    applyRating(cp, SM2::AGAIN);
    CHECK(cp.repetitions == 0);
    CHECK(cp.interval == 1);

    // After the reset the ladder restarts from the bottom
    applyRating(cp, SM2::GOOD);
    CHECK(cp.repetitions == 1);
    CHECK(cp.interval == 1);
    applyRating(cp, SM2::GOOD);
    CHECK(cp.interval == 6);
}

static void testDueDateArithmetic() {
    const int32_t today = 20640;  // 2026-07-06

    // New card rated Good: due tomorrow
    CardState cp;
    applyRating(cp, SM2::GOOD, today);
    CHECK(cp.due == today + 1);

    // Second Good (as if 1 day later): due 6 days out
    applyRating(cp, SM2::GOOD, today + 1);
    CHECK(cp.interval == 6);
    CHECK(cp.due == today + 1 + 6);

    // Lapse: due tomorrow again
    applyRating(cp, SM2::AGAIN, today + 7);
    CHECK(cp.interval == 1);
    CHECK(cp.due == today + 8);

    // No trustworthy clock: due stays 0 ("always due"), no date arithmetic
    CardState noClock;
    applyRating(noClock, SM2::GOOD, -1);
    CHECK(noClock.due == 0);
    CHECK(noClock.interval == 1);

    // A card scheduled while the clock worked, rated again with no clock,
    // falls back to always-due rather than keeping a bogus date
    applyRating(cp, SM2::GOOD, -1);
    CHECK(cp.due == 0);
}

int main() {
    testCalculateInterval();
    testUpdateEaseFactor();
    testFirstReviewPerGrade();
    testIntervalGrowthOverRepeatedGood();
    testAgainResetsRepetitions();
    testDueDateArithmetic();

    std::printf("test_sm2: %d checks, %d failed\n", testsRun, testsFailed);
    return testsFailed == 0 ? 0 : 1;
}
