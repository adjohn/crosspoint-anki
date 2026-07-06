// Native unit tests for the SM-2 scheduler (firmware/src/scheduling/SM2.{h,cpp})
// plus a mirror of how ReviewActivity::processRating() applies it.
// Build/run: test/native/run.sh
#include "scheduling/SM2.h"

#include <cmath>
#include <cstdio>

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

// Mirrors the scheduling core of ReviewActivity::processRating()
// (firmware/src/activities/ReviewActivity.cpp lines 175-190) so the
// as-shipped update sequence is executable on the host.
struct CardState {
    float ease = 2.5f;
    int interval = 0;
    int repetitions = 0;
};

static void applyRating(CardState& cp, SM2::Quality quality) {
    float newEase = SM2::updateEaseFactor(cp.ease, quality);
    int newInterval = SM2::calculateInterval(cp.repetitions, newEase);
    cp.ease = newEase;
    cp.interval = newInterval;
    if (quality >= SM2::GOOD) {
        cp.repetitions++;
    } else {
        cp.repetitions = 0;
    }
}

static void testCalculateInterval() {
    // SM-2 spec: I(1)=1, I(2)=6, I(n)=I(n-1)*EF
    CHECK(SM2::calculateInterval(0, 2.5f) == 0);
    CHECK(SM2::calculateInterval(-3, 2.5f) == 0);
    CHECK(SM2::calculateInterval(1, 2.5f) == 1);
    CHECK(SM2::calculateInterval(1, 1.3f) == 1);   // EF-independent
    CHECK(SM2::calculateInterval(2, 2.5f) == 6);
    CHECK(SM2::calculateInterval(2, 1.3f) == 6);   // EF-independent
    CHECK(SM2::calculateInterval(3, 2.5f) == 15);  // round(6*2.5)
    CHECK(SM2::calculateInterval(4, 2.5f) == 38);  // round(6*2.5^2) = round(37.5)
    CHECK(SM2::calculateInterval(5, 2.5f) == 94);  // round(6*2.5^3) = round(93.75)
    CHECK(SM2::calculateInterval(3, 1.3f) == 8);   // round(7.8)
    CHECK(SM2::calculateInterval(4, 1.3f) == 10);  // round(10.14)

    // Growth is monotonic non-decreasing across repetitions at the EF floor
    int prev = 0;
    for (int r = 1; r <= 12; ++r) {
        int cur = SM2::calculateInterval(r, 1.3f);
        CHECK(cur >= prev);
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
    // NOTE: processRating() computes the interval from the PRE-increment
    // repetition count, so a new card gets interval 0 even on Good/Easy.
    // These assertions document the shipped behavior (flagged in review).
    {
        CardState cp;
        applyRating(cp, SM2::AGAIN);
        CHECK(cp.repetitions == 0);
        CHECK(cp.interval == 0);
        CHECK(approx(cp.ease, 1.7f));
    }
    {
        CardState cp;
        applyRating(cp, SM2::HARD);   // q=2 < GOOD: counts as a lapse
        CHECK(cp.repetitions == 0);
        CHECK(cp.interval == 0);
        CHECK(approx(cp.ease, 2.18f));
    }
    {
        CardState cp;
        applyRating(cp, SM2::GOOD);
        CHECK(cp.repetitions == 1);
        CHECK(cp.interval == 0);      // pre-increment reps: 0, not 1 day
        CHECK(approx(cp.ease, 2.36f));
    }
    {
        CardState cp;
        applyRating(cp, SM2::EASY);
        CHECK(cp.repetitions == 1);
        CHECK(cp.interval == 0);      // pre-increment reps
        CHECK(approx(cp.ease, 2.6f));
    }
}

static void testIntervalGrowthOverRepeatedGood() {
    CardState cp;
    for (int i = 0; i < 8; ++i) {
        float expectedEase = SM2::updateEaseFactor(cp.ease, SM2::GOOD);
        int expectedInterval = SM2::calculateInterval(cp.repetitions, expectedEase);
        int expectedReps = cp.repetitions + 1;

        applyRating(cp, SM2::GOOD);

        CHECK(approx(cp.ease, expectedEase));
        CHECK(cp.interval == expectedInterval);
        CHECK(cp.repetitions == expectedReps);
    }
    // Shipped quirk (flagged in review): calculateInterval() re-applies the
    // CURRENT ease to every repetition instead of the SM-2 incremental
    // I(n)=I(n-1)*EF, and Good lowers ease by 0.14 each time, so the interval
    // can SHRINK under repeated Good once ease decays: 0,1,6,12,19,27,32,30.
    {
        CardState q;
        const int expected[8] = {0, 1, 6, 12, 19, 27, 32, 30};
        for (int i = 0; i < 8; ++i) {
            applyRating(q, SM2::GOOD);
            CHECK(q.interval == expected[i]);
        }
        CHECK(expected[7] < expected[6]);  // documents the non-monotonic step
    }
    // Shipped sequence from a fresh card: 0, 1, 6, then EF-scaled growth
    CardState fresh;
    applyRating(fresh, SM2::GOOD);
    CHECK(fresh.interval == 0);
    applyRating(fresh, SM2::GOOD);
    CHECK(fresh.interval == 1);
    applyRating(fresh, SM2::GOOD);
    CHECK(fresh.interval == 6);
    applyRating(fresh, SM2::GOOD);
    CHECK(fresh.interval == 12);  // round(6 * 1.94)
}

static void testAgainResetsRepetitions() {
    CardState cp;
    applyRating(cp, SM2::GOOD);
    applyRating(cp, SM2::GOOD);
    applyRating(cp, SM2::GOOD);
    CHECK(cp.repetitions == 3);

    applyRating(cp, SM2::AGAIN);
    CHECK(cp.repetitions == 0);
    // Documented shipped quirk: the interval is computed from the
    // pre-reset repetition count, so a lapse still yields a long interval.
    CHECK(cp.interval == SM2::calculateInterval(3, cp.ease));
    CHECK(cp.interval > 6);

    // After the reset the ladder restarts from the bottom
    applyRating(cp, SM2::GOOD);
    CHECK(cp.repetitions == 1);
    CHECK(cp.interval == 0);
}

int main() {
    testCalculateInterval();
    testUpdateEaseFactor();
    testFirstReviewPerGrade();
    testIntervalGrowthOverRepeatedGood();
    testAgainResetsRepetitions();

    std::printf("test_sm2: %d checks, %d failed\n", testsRun, testsFailed);
    return testsFailed == 0 ? 0 : 1;
}
