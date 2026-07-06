// Native unit tests for JSONL card parsing (firmware/src/data/Card.h) and
// progress persistence (firmware/src/data/Progress.h), run against the real
// vendored ArduinoJson with a host String shim.
// Build/run: test/native/run.sh
#include "data/Card.h"
#include "data/Progress.h"

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
    float d = a - b;
    return (d < 0 ? -d : d) < eps;
}

static void testParseFullLine() {
    Card c = Card::fromJson("{\"id\":\"c1\",\"front\":\"Q?\",\"back\":\"A!\",\"tags\":[\"geo\",\"cap\"]}");
    CHECK(c.id == "c1");
    CHECK(c.front == "Q?");
    CHECK(c.back == "A!");
    CHECK(c.tags.size() == 2);
    CHECK(c.tags[0] == "geo");
    CHECK(c.tags[1] == "cap");
}

static void testParseNoTags() {
    Card c = Card::fromJson("{\"id\":\"c2\",\"front\":\"f\",\"back\":\"b\"}");
    CHECK(c.id == "c2");
    CHECK(c.tags.empty());

    Card d = Card::fromJson("{\"id\":\"c3\",\"front\":\"f\",\"back\":\"b\",\"tags\":[]}");
    CHECK(d.tags.empty());
}

static void testParseEscapesAndUnicode() {
    Card c = Card::fromJson("{\"id\":\"u1\",\"front\":\"say \\\"hi\\\"\",\"back\":\"caf\\u00e9 → done\",\"tags\":[]}");
    CHECK(c.front == "say \"hi\"");
    CHECK(c.back == "caf\xc3\xa9 \xe2\x86\x92 done");
}

static void testMalformedLine() {
    Card c = Card::fromJson("{\"id\":\"broken\",\"front\":");
    CHECK(c.id == "");
    CHECK(c.front == "");
    CHECK(c.back == "");
    CHECK(c.tags.empty());

    Card d = Card::fromJson("");
    CHECK(d.id == "");
}

static void testMissingFieldBehavior() {
    // Regression: absent keys must parse to empty strings, not the literal
    // text "null" (the old as<String>() behavior on a null variant).
    Card c = Card::fromJson("{\"id\":\"only-id\"}");
    CHECK(c.id == "only-id");
    CHECK(c.front == "");
    CHECK(c.back == "");

    Card d = Card::fromJson("{\"front\":\"f\",\"back\":\"b\"}");
    CHECK(d.id == "");

    // Explicit JSON null behaves like a missing key
    Card e = Card::fromJson("{\"id\":null,\"front\":null,\"back\":\"b\"}");
    CHECK(e.id == "");
    CHECK(e.front == "");
    CHECK(e.back == "b");
}

static void testCardRoundTrip() {
    Card c;
    c.id = "rt1";
    c.front = "front text";
    c.back = "back \"quoted\" text";
    c.tags.push_back(String("t1"));
    c.tags.push_back(String("t2"));

    Card d = Card::fromJson(c.toJson());
    CHECK(d.id == c.id);
    CHECK(d.front == c.front);
    CHECK(d.back == c.back);
    CHECK(d.tags.size() == 2);
    CHECK(d.tags[0] == "t1");
    CHECK(d.tags[1] == "t2");
}

static void testProgressDefaults() {
    DeckProgress p;
    CHECK(p.lastReview == 0);
    CardProgress& cp = p.getCardProgress(String("new-card"));
    CHECK(approx(cp.ease, 2.5f));
    CHECK(cp.interval == 0);
    CHECK(cp.repetitions == 0);
    CHECK(cp.due == 0);          // 0 = always due
    CHECK(p.cards.size() == 1);  // operator[] default-inserts
}

static void testProgressRoundTrip() {
    DeckProgress p;
    p.deckId = "deck-1";
    p.lastReview = 20640;  // 2026-07-06 in epoch days
    CardProgress a;
    a.ease = 2.36f;
    a.interval = 6;
    a.repetitions = 3;
    a.due = 20646;
    p.cards[String("card-a")] = a;
    CardProgress b;
    b.ease = 1.3f;
    b.interval = 0;
    b.repetitions = 0;
    b.due = 0;
    p.cards[String("card-b")] = b;

    DeckProgress q = DeckProgress::fromJson(p.toJson());
    CHECK(q.deckId == "deck-1");
    CHECK(q.lastReview == 20640);
    CHECK(q.cards.size() == 2);
    CHECK(approx(q.cards[String("card-a")].ease, 2.36f));
    CHECK(q.cards[String("card-a")].interval == 6);
    CHECK(q.cards[String("card-a")].repetitions == 3);
    CHECK(q.cards[String("card-a")].due == 20646);
    CHECK(approx(q.cards[String("card-b")].ease, 1.3f));
    CHECK(q.cards[String("card-b")].repetitions == 0);
    CHECK(q.cards[String("card-b")].due == 0);
}

static void testProgressLenientMigration() {
    // Old-format files stored due/lastReview as strings and may omit fields
    // entirely; both must migrate to 0 ("due immediately" / "never
    // reviewed") without corrupting the numeric fields.
    DeckProgress p = DeckProgress::fromJson(String(
        "{\"deckId\":\"old\",\"lastReview\":\"2026-07-06\",\"cards\":{"
        "\"a\":{\"ease\":2.36,\"interval\":6,\"repetitions\":3,\"due\":\"2026-07-12\"},"
        "\"b\":{\"ease\":1.3,\"interval\":0,\"repetitions\":0,\"due\":\"\"},"
        "\"c\":{}"
        "}}"));
    CHECK(p.deckId == "old");
    CHECK(p.lastReview == 0);
    CHECK(p.cards.size() == 3);
    CHECK(p.cards[String("a")].due == 0);
    CHECK(p.cards[String("a")].interval == 6);
    CHECK(p.cards[String("a")].repetitions == 3);
    CHECK(approx(p.cards[String("a")].ease, 2.36f));
    CHECK(p.cards[String("b")].due == 0);
    CHECK(approx(p.cards[String("c")].ease, 2.5f));  // missing -> defaults
    CHECK(p.cards[String("c")].interval == 0);
    CHECK(p.cards[String("c")].due == 0);
}

static void testProgressMalformed() {
    DeckProgress p = DeckProgress::fromJson(String("not json at all"));
    CHECK(p.deckId == "");
    CHECK(p.lastReview == 0);
    CHECK(p.cards.empty());
}

static void testCountDue() {
    const int32_t today = 20640;

    DeckProgress p;
    CardProgress cp;

    cp.due = today - 3;                    // overdue
    p.cards[String("overdue")] = cp;
    cp.due = today;                        // due today
    p.cards[String("today")] = cp;
    cp.due = 0;                            // always due
    p.cards[String("always")] = cp;
    cp.due = today + 1;                    // due tomorrow
    p.cards[String("tomorrow")] = cp;
    cp.due = today + 30;                   // far future
    p.cards[String("future")] = cp;

    // 6 cards in the deck, 5 tracked: 1 new + overdue + today + always = 4
    CHECK(p.countDue(today, 6) == 4);
    // All tracked, none new
    CHECK(p.countDue(today, 5) == 3);
    // Stale progress entries beyond cardCount never go negative on new cards
    CHECK(p.countDue(today, 2) == 3);
    // No clock: everything counts as due
    CHECK(p.countDue(-1, 6) == 6);
    // Empty progress: all cards are new and due
    DeckProgress empty;
    CHECK(empty.countDue(today, 42) == 42);
    CHECK(empty.countDue(-1, 42) == 42);
    CHECK(empty.countDue(today, 0) == 0);
}

int main() {
    testParseFullLine();
    testParseNoTags();
    testParseEscapesAndUnicode();
    testMalformedLine();
    testMissingFieldBehavior();
    testCardRoundTrip();
    testProgressDefaults();
    testProgressRoundTrip();
    testProgressLenientMigration();
    testProgressMalformed();
    testCountDue();

    std::printf("test_card_json: %d checks, %d failed\n", testsRun, testsFailed);
    return testsFailed == 0 ? 0 : 1;
}
