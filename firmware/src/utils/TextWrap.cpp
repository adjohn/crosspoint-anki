#include "TextWrap.h"

#include <Utf8.h>
#include <string>

namespace {

// Byte length of the UTF-8 sequence starting at s (always >= 1, so malformed
// input still advances). Uses the same decoder as the renderer.
uint32_t sequenceLength(const char* s) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(s);
    utf8NextCodepoint(&p);
    const uint32_t n = p - reinterpret_cast<const uint8_t*>(s);
    return n > 0 ? n : 1;
}

}

void TextWrap::wrap(const GfxRenderer& renderer, int fontId, const String& text,
                    int maxWidth, std::vector<Line>& lines) {
    lines.clear();
    if (maxWidth < 1) maxWidth = 1;

    const char* s = text.c_str();
    const uint32_t len = text.length();
    const int spaceWidth = renderer.getSpaceWidth(fontId);
    std::string buf;  // reusable measurement buffer

    uint32_t pos = 0;
    while (true) {
        // Paragraph = [pos, paraEnd) up to the next '\n' or end of text
        uint32_t paraEnd = pos;
        while (paraEnd < len && s[paraEnd] != '\n') paraEnd++;

        uint32_t lineStart = pos;
        uint32_t lineEnd = pos;  // exclusive; == lineStart means empty line
        int lineWidth = 0;

        uint32_t i = pos;
        while (i < paraEnd) {
            // Gap of spaces before the next word (trailing spaces are dropped)
            uint32_t gap = 0;
            while (i < paraEnd && s[i] == ' ') { i++; gap++; }
            if (i >= paraEnd) break;

            const uint32_t wordStart = i;
            while (i < paraEnd && s[i] != ' ') i++;
            const uint32_t wordEnd = i;
            buf.assign(s + wordStart, wordEnd - wordStart);
            const int wordWidth = renderer.getTextWidth(fontId, buf.c_str());

            if (lineEnd > lineStart) {
                // Try to append to the current line, gap spaces included
                const int candidate = lineWidth + static_cast<int>(gap) * spaceWidth + wordWidth;
                if (candidate <= maxWidth) {
                    lineEnd = wordEnd;
                    lineWidth = candidate;
                    continue;
                }
                lines.push_back({lineStart, lineEnd - lineStart});
                lineStart = wordStart;
                lineEnd = wordStart;
                lineWidth = 0;
            }

            if (wordWidth <= maxWidth) {
                lineStart = wordStart;  // line is empty here; drop leading spaces
                lineEnd = wordEnd;
                lineWidth = wordWidth;
                continue;
            }

            // Unbreakable over-long word: split at glyph boundaries. Every
            // fragment except the last becomes a full line of its own.
            uint32_t fragStart = wordStart;
            while (fragStart < wordEnd) {
                uint32_t fragEnd = fragStart;
                int fragWidth = 0;
                while (fragEnd < wordEnd) {
                    const uint32_t n = sequenceLength(s + fragEnd);
                    buf.assign(s + fragStart, fragEnd + n - fragStart);
                    const int grown = renderer.getTextWidth(fontId, buf.c_str());
                    if (grown > maxWidth && fragEnd > fragStart) break;
                    fragEnd += n;
                    fragWidth = grown;
                    if (fragWidth > maxWidth) break;  // single glyph wider than the line
                }
                if (fragEnd < wordEnd) {
                    lines.push_back({fragStart, fragEnd - fragStart});
                    fragStart = fragEnd;
                } else {
                    // Last fragment stays open so following words can join it
                    lineStart = fragStart;
                    lineEnd = wordEnd;
                    lineWidth = fragWidth;
                    fragStart = wordEnd;
                }
            }
        }

        // Flush the paragraph's final line (empty paragraph -> blank line)
        lines.push_back({lineStart, lineEnd - lineStart});

        if (paraEnd >= len) break;
        pos = paraEnd + 1;
    }

    // Trailing newlines would otherwise pad the page count with invisible lines
    while (!lines.empty() && lines.back().length == 0) {
        lines.pop_back();
    }
    if (lines.empty()) {
        lines.push_back({0, 0});
    }
}
