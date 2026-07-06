// Native unit tests for TextWrap::wrap (firmware/src/utils/TextWrap.{h,cpp}),
// the UTF-8-safe greedy word wrap behind long-card pagination. Compiled
// against the mock GfxRenderer (test/native/mock) where every codepoint is
// exactly 10 px wide, so expected break points are deterministic.
// Build/run: test/native/run.sh
#include "utils/TextWrap.h"

#include <cstdio>
#include <string>
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

static GfxRenderer g;
constexpr int FONT = 2;   // any id; the mock ignores it
constexpr int CP = GfxRenderer::CP_WIDTH;  // 10 px per codepoint

// Wrap and return the rendered lines as std::strings for easy comparison.
static std::vector<std::string> wrapText(const char* text, int maxWidth) {
    std::vector<TextWrap::Line> lines;
    const String s(text);
    TextWrap::wrap(g, FONT, s, maxWidth, lines);

    std::vector<std::string> out;
    for (const TextWrap::Line& ln : lines) {
        out.push_back(std::string(text + ln.start, ln.length));
    }
    return out;
}

static bool isContinuationByte(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

// Structural invariants that must hold for ANY input: offsets in-bounds and
// non-decreasing, no line boundary mid-UTF-8-sequence, no line wider than
// maxWidth unless it is a single glyph, output never empty. wellFormed=false
// (malformed UTF-8 inputs) skips the mid-sequence check: stray continuation
// bytes decode as standalone replacement glyphs, so lines may start on them.
static void checkInvariants(const char* text, int maxWidth, bool wellFormed = true) {
    std::vector<TextWrap::Line> lines;
    const String s(text);
    TextWrap::wrap(g, FONT, s, maxWidth, lines);
    const uint32_t len = s.length();

    CHECK(!lines.empty());
    uint32_t prevEnd = 0;
    bool ok = true;
    for (size_t i = 0; i < lines.size(); i++) {
        const TextWrap::Line& ln = lines[i];
        if (ln.start > len || ln.start + ln.length > len) ok = false;
        if (i > 0 && ln.start < prevEnd) ok = false;  // no overlap/reorder
        prevEnd = ln.start + ln.length;

        // Boundaries never split a UTF-8 sequence
        if (wellFormed) {
            if (ln.start < len && isContinuationByte(text[ln.start])) ok = false;
            if (prevEnd < len && ln.length > 0 && isContinuationByte(text[prevEnd])) ok = false;
        }

        // Width budget (a single glyph may legitimately exceed a tiny width)
        std::string line(text + ln.start, ln.length);
        const int w = g.getTextWidth(FONT, line.c_str());
        if (w > maxWidth && w > CP) ok = false;
    }
    CHECK(ok);
}

static void testBasicWrap() {
    // Everything fits on one line
    CHECK(wrapText("hello", 100) == std::vector<std::string>({"hello"}));
    CHECK(wrapText("aa bb", 50) == std::vector<std::string>({"aa bb"}));

    // Greedy fill: exactly-full lines, space at the wrap point dropped
    CHECK(wrapText("aa bb cc dd", 50) ==
          std::vector<std::string>({"aa bb", "cc dd"}));
    CHECK(wrapText("aaa bbb", 30) == std::vector<std::string>({"aaa", "bbb"}));

    // One px short of fitting wraps; one word per line
    CHECK(wrapText("aa bb", 49) == std::vector<std::string>({"aa", "bb"}));

    // Greedy (not balanced): first line takes all it can
    CHECK(wrapText("a bb ccc d", 80) ==
          std::vector<std::string>({"a bb ccc", "d"}));
    CHECK(wrapText("a bb ccc d", 70) ==
          std::vector<std::string>({"a bb", "ccc d"}));

    // Empty text still yields one blank line
    CHECK(wrapText("", 100) == std::vector<std::string>({""}));
}

static void testSpaceHandling() {
    // Interior runs of spaces are preserved and their width counted
    CHECK(wrapText("a  b", 100) == std::vector<std::string>({"a  b"}));
    CHECK(wrapText("a  b", 40) == std::vector<std::string>({"a  b"}));  // 4 cp exactly
    CHECK(wrapText("a  b", 30) == std::vector<std::string>({"a", "b"}));  // gap busts it

    // Leading spaces are dropped, on the first line and after \n
    CHECK(wrapText("  abc", 100) == std::vector<std::string>({"abc"}));
    CHECK(wrapText("x\n  y", 100) == std::vector<std::string>({"x", "y"}));

    // Trailing spaces are dropped (never wrap a line just for them)
    CHECK(wrapText("abc   ", 30) == std::vector<std::string>({"abc"}));

    // Space-only text collapses to a single blank line
    CHECK(wrapText("   ", 100) == std::vector<std::string>({""}));
}

static void testNewlines() {
    // \n always starts a new line, even when both parts would fit together
    CHECK(wrapText("one\ntwo", 200) == std::vector<std::string>({"one", "two"}));

    // Empty paragraph -> blank line preserved mid-text
    CHECK(wrapText("one\n\ntwo", 200) ==
          std::vector<std::string>({"one", "", "two"}));

    // Trailing newlines never pad the page count
    CHECK(wrapText("one\n", 200) == std::vector<std::string>({"one"}));
    CHECK(wrapText("one\n\n\n", 200) == std::vector<std::string>({"one"}));
    CHECK(wrapText("\n\n", 200) == std::vector<std::string>({""}));

    // Wrapping still applies within each paragraph
    CHECK(wrapText("aa bb cc\ndd", 50) ==
          std::vector<std::string>({"aa bb", "cc", "dd"}));

    // Leading newline -> blank first line (deliberate vertical space)
    CHECK(wrapText("\nabc", 200) == std::vector<std::string>({"", "abc"}));
}

static void testLongWordSplit() {
    // Unbreakable word split at glyph boundaries, 3 cp per 30 px line
    CHECK(wrapText("abcdefg", 30) ==
          std::vector<std::string>({"abc", "def", "g"}));

    // Exact multiple: no empty trailing fragment
    CHECK(wrapText("abcdef", 30) == std::vector<std::string>({"abc", "def"}));

    // The last fragment stays open so a following word can join it
    CHECK(wrapText("abcd e", 30) == std::vector<std::string>({"abc", "d e"}));
    // ...but only if it fits
    CHECK(wrapText("abcd ef", 30) ==
          std::vector<std::string>({"abc", "d", "ef"}));

    // A long word after a partial line: line flushes first, then splits
    CHECK(wrapText("aa bcdefgh", 30) ==
          std::vector<std::string>({"aa", "bcd", "efg", "h"}));

    // Width smaller than one glyph: one glyph per line (can't do better)
    CHECK(wrapText("ab", 5) == std::vector<std::string>({"a", "b"}));
    checkInvariants("ab", 5);
    checkInvariants("abcdefg", 30);
}

static void testUtf8Boundaries() {
    // 2-byte codepoints (e acute): wrap counts codepoints, not bytes
    CHECK(wrapText("\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9", 20) ==
          std::vector<std::string>({"\xC3\xA9\xC3\xA9", "\xC3\xA9\xC3\xA9",
                                    "\xC3\xA9"}));

    // 3-byte CJK, no spaces: split at glyph boundaries only
    CHECK(wrapText("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x82\xAB\xE3\x83\xBC\xE3\x83\x89", 30) ==
          std::vector<std::string>({"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E",
                                    "\xE3\x82\xAB\xE3\x83\xBC\xE3\x83\x89"}));

    // Mixed-width words wrap by codepoint count
    CHECK(wrapText("h\xC3\xA9llo w\xC3\xB6rld", 50) ==
          std::vector<std::string>({"h\xC3\xA9llo", "w\xC3\xB6rld"}));

    // 4-byte emoji split cleanly (2 cp per 20 px line)
    CHECK(wrapText("\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80", 20) ==
          std::vector<std::string>({"\xF0\x9F\x98\x80\xF0\x9F\x98\x80",
                                    "\xF0\x9F\x98\x80"}));

    checkInvariants("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x82\xAB\xE3\x83\xBC\xE3\x83\x89", 30);
    checkInvariants("h\xC3\xA9llo w\xC3\xB6rld caf\xC3\xA9", 40);
    checkInvariants("\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80", 25);
}

static void testMalformedUtf8DoesNotHang() {
    // Lone continuation bytes and truncated sequences must terminate and
    // keep offsets in-bounds (widths may be odd; only structure matters)
    checkInvariants("\x80\x80\x80", 20, false);
    checkInvariants("abc\xC3", 20, false);      // truncated 2-byte seq at end
    checkInvariants("a\xE6\x97 b", 30, false);  // truncated 3-byte seq mid-text
}

static void testCardLikeText() {
    // A realistic multi-paragraph card back at X4 geometry (800-20 = 780 px,
    // 78 cp per line with the mock's uniform widths)
    const char* text =
        "The mitochondria is the powerhouse of the cell.\n\n"
        "It generates most of the cell's supply of adenosine triphosphate "
        "(ATP), used as a source of chemical energy.";
    std::vector<std::string> lines = wrapText(text, 780);
    CHECK(lines.size() == 4);
    CHECK(lines[0] == "The mitochondria is the powerhouse of the cell.");
    CHECK(lines[1] == "");
    CHECK(lines[2].size() <= 78);
    checkInvariants(text, 780);

    // Same text at a narrow width still satisfies every invariant
    checkInvariants(text, 100);
}

int main() {
    testBasicWrap();
    testSpaceHandling();
    testNewlines();
    testLongWordSplit();
    testUtf8Boundaries();
    testMalformedUtf8DoesNotHang();
    testCardLikeText();

    std::printf("test_textwrap: %d checks, %d failed\n", testsRun, testsFailed);
    return testsFailed == 0 ? 0 : 1;
}
