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
    // Documents shipped behavior: as<String>() on an absent key serializes
    // the null variant, so missing fields come back as the text "null"
    // rather than an empty string (flagged in review).
    Card c = Card::fromJson("{\"id\":\"only-id\"}");
    CHECK(c.id == "only-id");
    CHECK(c.front == "null");
    CHECK(c.back == "null");
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
    CardProgress& cp = p.getCardProgress(String("new-card"));
    CHECK(approx(cp.ease, 2.5f));
    CHECK(cp.interval == 0);
    CHECK(cp.repetitions == 0);
    CHECK(cp.due == "");
    CHECK(p.cards.size() == 1);  // operator[] default-inserts
}

static void testProgressRoundTrip() {
    DeckProgress p;
    p.deckId = "deck-1";
    p.lastReview = "2026-07-06";
    CardProgress a;
    a.ease = 2.36f;
    a.interval = 6;
    a.repetitions = 3;
    a.due = "2026-07-12";
    p.cards[String("card-a")] = a;
    CardProgress b;
    b.ease = 1.3f;
    b.interval = 0;
    b.repetitions = 0;
    b.due = "";
    p.cards[String("card-b")] = b;

    DeckProgress q = DeckProgress::fromJson(p.toJson());
    CHECK(q.deckId == "deck-1");
    CHECK(q.lastReview == "2026-07-06");
    CHECK(q.cards.size() == 2);
    CHECK(approx(q.cards[String("card-a")].ease, 2.36f));
    CHECK(q.cards[String("card-a")].interval == 6);
    CHECK(q.cards[String("card-a")].repetitions == 3);
    CHECK(q.cards[String("card-a")].due == "2026-07-12");
    CHECK(approx(q.cards[String("card-b")].ease, 1.3f));
    CHECK(q.cards[String("card-b")].repetitions == 0);
}

static void testProgressMalformed() {
    DeckProgress p = DeckProgress::fromJson(String("not json at all"));
    CHECK(p.deckId == "");
    CHECK(p.cards.empty());
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
    testProgressMalformed();

    std::printf("test_card_json: %d checks, %d failed\n", testsRun, testsFailed);
    return testsFailed == 0 ? 0 : 1;
}
