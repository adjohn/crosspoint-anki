// Native unit tests for in-session relearning:
//  - RelearnQueue / Relearn::staysDueToday (firmware/src/scheduling/RelearnQueue.h)
//    tested directly (pure String+vector logic), plus
//  - MIRROR tests of how ReviewActivity.cpp applies them (processRating's
//    due-date branch, first-rating counting, and the relearn pass loop with
//    its serve-nothing guard). The mirrors are copies of the device logic,
//    clearly marked; keep them in sync with ReviewActivity.cpp.
// Build/run: test/native/run.sh
#include "scheduling/RelearnQueue.h"
#include "scheduling/SM2.h"

#include <cstdio>
#include <cstdint>
#include <vector>

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

static void testStaysDueToday() {
    // Exactly the branch processRating uses for due = today vs today+interval
    CHECK(Relearn::staysDueToday(SM2::AGAIN));
    CHECK(!Relearn::staysDueToday(SM2::HARD));
    CHECK(!Relearn::staysDueToday(SM2::GOOD));
    CHECK(!Relearn::staysDueToday(SM2::EASY));
}

static void testEnqueueDedup() {
    RelearnQueue q;
    CHECK(q.empty());
    CHECK(q.size() == 0);
    CHECK(!q.contains("a"));

    q.onRated("a", SM2::AGAIN);
    CHECK(!q.empty());
    CHECK(q.size() == 1);
    CHECK(q.contains("a"));

    // Duplicate Again ratings never double-enqueue
    q.onRated("a", SM2::AGAIN);
    q.onRated("a", SM2::AGAIN);
    CHECK(q.size() == 1);
    CHECK(q.contains("a"));

    q.onRated("b", SM2::AGAIN);
    CHECK(q.size() == 2);
    CHECK(q.contains("a") && q.contains("b"));
}

static void testDequeueOnPassingRatings() {
    // Each of Hard/Good/Easy removes the card; Again re-adds it
    const SM2::Quality passing[3] = {SM2::HARD, SM2::GOOD, SM2::EASY};
    for (SM2::Quality quality : passing) {
        RelearnQueue q;
        q.onRated("a", SM2::AGAIN);
        q.onRated("b", SM2::AGAIN);
        q.onRated("a", quality);
        CHECK(!q.contains("a"));
        CHECK(q.contains("b"));
        CHECK(q.size() == 1);
    }

    // Dequeue of an id that was never enqueued is a no-op
    RelearnQueue q;
    q.onRated("a", SM2::AGAIN);
    q.onRated("ghost", SM2::GOOD);
    q.onRated("ghost", SM2::HARD);
    q.onRated("ghost", SM2::EASY);
    CHECK(q.size() == 1);
    CHECK(q.contains("a"));

    // Again -> Good -> Again cycles cleanly
    q.onRated("a", SM2::GOOD);
    CHECK(q.empty());
    q.onRated("a", SM2::AGAIN);
    CHECK(q.size() == 1 && q.contains("a"));
}

static String idFor(int i) {
    return String("card-") + String(i);
}

static void testCapacityCap() {
    RelearnQueue q;
    CHECK(RelearnQueue::MAX_IDS == 256);

    for (int i = 0; i < 256; i++) {
        q.onRated(idFor(i), SM2::AGAIN);
    }
    CHECK(q.size() == 256);
    CHECK(q.contains(idFor(0)) && q.contains(idFor(255)));

    // The 257th distinct Again id is silently dropped
    q.onRated(idFor(256), SM2::AGAIN);
    CHECK(q.size() == 256);
    CHECK(!q.contains(idFor(256)));

    // Re-rating an already-queued card Again at the cap is still a no-op
    q.onRated(idFor(0), SM2::AGAIN);
    CHECK(q.size() == 256);

    // Freeing a slot lets the dropped card in on its next Again rating
    q.onRated(idFor(10), SM2::GOOD);
    CHECK(q.size() == 255);
    CHECK(!q.contains(idFor(10)));
    q.onRated(idFor(256), SM2::AGAIN);
    CHECK(q.size() == 256);
    CHECK(q.contains(idFor(256)));
}

// ---------------------------------------------------------------------------
// MIRROR of ReviewActivity::processRating() (ReviewActivity.cpp): ease ->
// repetitions -> interval, then the relearn due-date branch, queue fold, and
// first-rating counting. Keep in sync with the device code.
// ---------------------------------------------------------------------------

struct CardState {
    float ease = 2.5f;
    int interval = 0;
    int repetitions = 0;
    int32_t due = 0;
};

struct SessionMirror {
    RelearnQueue relearnQueue;
    int reviewedCount = 0;
    int32_t today = 20640;  // 2026-07-06

    void processRating(CardState& cp, const String& id, SM2::Quality quality,
                       bool inRelearnPass) {
        const bool firstRating = !inRelearnPass;

        cp.ease = SM2::updateEaseFactor(cp.ease, quality);
        if (quality >= SM2::GOOD) {
            cp.repetitions++;
        } else {
            cp.repetitions = 0;
        }
        cp.interval = SM2::calculateInterval(cp.repetitions, cp.interval, cp.ease);

        if (today >= 0) {
            cp.due = Relearn::staysDueToday(quality) ? today : today + cp.interval;
        } else {
            cp.due = 0;
        }

        relearnQueue.onRated(id, quality);

        if (firstRating) {
            reviewedCount++;
        }
    }
};

static void testDueDateBranchMirror() {
    SessionMirror s;

    // Again: SM-2 updates run as before (ease drop, reps reset, interval 1)
    // but due stays TODAY, and the card is queued for this session
    CardState a;
    a.ease = 2.5f; a.interval = 12; a.repetitions = 3;
    s.processRating(a, "a", SM2::AGAIN, false);
    CHECK(a.due == s.today);
    CHECK(a.interval == 1);
    CHECK(a.repetitions == 0);
    CHECK(s.relearnQueue.contains("a"));

    // Hard: also a lapse, but scheduled OUT to tomorrow and not queued
    CardState h;
    h.ease = 2.5f; h.interval = 12; h.repetitions = 3;
    s.processRating(h, "h", SM2::HARD, false);
    CHECK(h.due == s.today + 1);
    CHECK(h.interval == 1);
    CHECK(!s.relearnQueue.contains("h"));

    // Good/Easy: due moves out by the new interval
    CardState g;
    s.processRating(g, "g", SM2::GOOD, false);
    CHECK(g.due == s.today + 1);
    CHECK(!s.relearnQueue.contains("g"));

    // No trustworthy clock: due stays 0 even for Again, card still queued
    SessionMirror noClock;
    noClock.today = -1;
    CardState n;
    noClock.processRating(n, "n", SM2::AGAIN, false);
    CHECK(n.due == 0);
    CHECK(noClock.relearnQueue.contains("n"));

    // Relearn re-rating with Good clears the queue and schedules out
    s.processRating(a, "a", SM2::GOOD, true);
    CHECK(a.due == s.today + 1);
    CHECK(!s.relearnQueue.contains("a"));
}

// ---------------------------------------------------------------------------
// MIRROR of ReviewActivity::advanceToNextCard()'s relearn pass loop: after
// the main due-pass, the deck file is rescanned serving only queued ids, in
// file order, until the queue empties; a full pass that serves nothing while
// the queue is non-empty (ids missing from the file) ends the session.
// Keep in sync with ReviewActivity.cpp.
// ---------------------------------------------------------------------------

struct PassResult {
    std::vector<String> served;      // every card served, in order
    int passes = 0;
    bool endedByGuard = false;       // serve-nothing guard tripped
    int reviewedCount = 0;
};

// rateFn decides the quality for a serving (index = how many times this card
// has been served across the whole session, 0-based).
template <typename RateFn>
static PassResult runSession(const std::vector<String>& deck, RateFn rateFn) {
    SessionMirror s;
    std::vector<CardState> states(deck.size());
    std::vector<int> timesServed(deck.size(), 0);
    PassResult r;

    auto indexOf = [&](const String& id) {
        for (size_t i = 0; i < deck.size(); i++) {
            if (deck[i] == id) return static_cast<int>(i);
        }
        return -1;
    };

    // Main pass: every card is due (fresh deck), served in file order
    for (size_t i = 0; i < deck.size(); i++) {
        r.served.push_back(deck[i]);
        s.processRating(states[i], deck[i], rateFn(deck[i], timesServed[i]++), false);
    }

    // Relearn passes, with the same guard structure as advanceToNextCard()
    const int MAX_PASSES = 64;  // test-only safety net, never hit by design
    while (!s.relearnQueue.empty() && r.passes < MAX_PASSES) {
        bool relearnServedThisPass = false;
        r.passes++;
        for (size_t i = 0; i < deck.size(); i++) {
            if (s.relearnQueue.contains(deck[i])) {
                relearnServedThisPass = true;
                r.served.push_back(deck[i]);
                const int idx = indexOf(deck[i]);
                s.processRating(states[idx], deck[i],
                                rateFn(deck[i], timesServed[idx]++), true);
            }
        }
        if (!relearnServedThisPass) {
            r.endedByGuard = true;  // queued ids vanished from the file: bail
            break;
        }
    }

    r.reviewedCount = s.reviewedCount;
    return r;
}

static void testRelearnPassOrderingMirror() {
    // c1 and c3 fail once, c2 passes; the relearn pass serves c1 then c3
    // (file order), and reviewedCount stays unique-cards
    const std::vector<String> deck = {"c1", "c2", "c3"};
    PassResult r = runSession(deck, [](const String& id, int served) {
        if ((id == "c1" || id == "c3") && served == 0) return SM2::AGAIN;
        return SM2::GOOD;
    });
    const std::vector<String> expected = {"c1", "c2", "c3", "c1", "c3"};
    CHECK(r.served == expected);
    CHECK(r.passes == 1);
    CHECK(!r.endedByGuard);
    CHECK(r.reviewedCount == 3);  // relearn re-ratings don't inflate it
}

static void testRepeatedAgainRepeatsAcrossPassesMirror() {
    // c2 fails twice: it appears in two consecutive relearn passes (alone in
    // the second), then Hard dequeues it — Hard ends relearning for a card
    const std::vector<String> deck = {"c1", "c2"};
    PassResult r = runSession(deck, [](const String& id, int served) {
        if (id == "c2" && served < 2) return SM2::AGAIN;
        if (id == "c2" && served == 2) return SM2::HARD;
        return SM2::GOOD;
    });
    const std::vector<String> expected = {"c1", "c2", "c2", "c2"};
    CHECK(r.served == expected);
    CHECK(r.passes == 2);
    CHECK(!r.endedByGuard);
    CHECK(r.reviewedCount == 2);
}

static void testAllGoodSkipsRelearnMirror() {
    const std::vector<String> deck = {"c1", "c2", "c3"};
    PassResult r = runSession(deck, [](const String&, int) { return SM2::GOOD; });
    CHECK(r.served.size() == 3);
    CHECK(r.passes == 0);
    CHECK(r.reviewedCount == 3);
}

static void testServeNothingGuardMirror() {
    // A queued id that is missing from the deck file must not spin forever:
    // the pass serves zero cards and the guard ends the session
    SessionMirror s;
    s.relearnQueue.onRated("ghost", SM2::AGAIN);
    const std::vector<String> deck = {"c1"};

    int passes = 0;
    bool endedByGuard = false;
    while (!s.relearnQueue.empty() && passes < 10) {
        bool served = false;
        passes++;
        for (const String& id : deck) {
            if (s.relearnQueue.contains(id)) served = true;
        }
        if (!served) {
            endedByGuard = true;
            break;
        }
    }
    CHECK(endedByGuard);
    CHECK(passes == 1);
    CHECK(!s.relearnQueue.empty());  // queue untouched; cards stay due today
}

int main() {
    testStaysDueToday();
    testEnqueueDedup();
    testDequeueOnPassingRatings();
    testCapacityCap();
    testDueDateBranchMirror();
    testRelearnPassOrderingMirror();
    testRepeatedAgainRepeatsAcrossPassesMirror();
    testAllGoodSkipsRelearnMirror();
    testServeNothingGuardMirror();

    std::printf("test_relearn: %d checks, %d failed\n", testsRun, testsFailed);
    return testsFailed == 0 ? 0 : 1;
}
