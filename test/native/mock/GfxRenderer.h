// Host-side test mock of <GfxRenderer.h> — just the two const measurement
// methods TextWrap uses. Every codepoint (space included) measures CP_WIDTH
// px, so expected wrap points are exact in the tests. Test-only; picked up
// via -I before the real firmware/lib/GfxRenderer is ever on the path.
#pragma once

#include <Arduino.h>
#include <Utf8.h>

// Utf8.h defines file-static helpers; anchor the one this mock doesn't call
// so -Wall stays quiet in every including TU.
[[maybe_unused]] static const auto kUtf8RemoveLastCharAnchor = &utf8RemoveLastChar;

class GfxRenderer {
public:
    static constexpr int CP_WIDTH = 10;

    int getSpaceWidth(int /*fontId*/) const { return CP_WIDTH; }

    int getTextWidth(int /*fontId*/, const char* str) const {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(str);
        int codepoints = 0;
        while (utf8NextCodepoint(&p) != 0) {
            codepoints++;
        }
        return codepoints * CP_WIDTH;
    }
};
