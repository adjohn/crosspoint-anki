#pragma once

#include <Arduino.h>
#include <GfxRenderer.h>
#include <cstdint>
#include <vector>

// UTF-8-safe greedy word wrap. Produces byte offsets into the source text
// (no copies), one entry per rendered line. '\n' in the source always starts
// a new line; words wider than maxWidth are split at glyph boundaries.
namespace TextWrap {

struct Line {
    uint32_t start;   // byte offset into the source text
    uint32_t length;  // byte length; 0 renders as a blank line
};

// Fills `lines` (cleared first) with the wrapped layout of `text` for the
// given font and pixel width. Spaces at wrap points and leading spaces on
// wrapped lines are dropped; interior runs of spaces are preserved.
void wrap(const GfxRenderer& renderer, int fontId, const String& text,
          int maxWidth, std::vector<Line>& lines);

}
