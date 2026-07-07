#pragma once
#include <Arduino.h>
#include <EInkDisplay.h>

class HalDisplay {
 public:
  HalDisplay();
  ~HalDisplay();

  enum RefreshMode {
    FULL_REFRESH,
    HALF_REFRESH,
    FAST_REFRESH
  };

  // Pass isX3=true before any drawing to switch the panel to X3 geometry
  void begin(bool isX3 = false);

  // Legacy X4 compile-time dimensions; use the runtime getters below for
  // per-device geometry
  static constexpr uint16_t DISPLAY_WIDTH = EInkDisplay::DISPLAY_WIDTH;
  static constexpr uint16_t DISPLAY_HEIGHT = EInkDisplay::DISPLAY_HEIGHT;
  static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
  static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;

  // Runtime geometry passthrough. Inline: these sit on the per-pixel hot
  // path in GfxRenderer and collapse to member-variable loads.
  uint16_t getDisplayWidth() const { return einkDisplay.getDisplayWidth(); }
  uint16_t getDisplayHeight() const { return einkDisplay.getDisplayHeight(); }
  uint16_t getDisplayWidthBytes() const { return einkDisplay.getDisplayWidthBytes(); }
  uint32_t getBufferSize() const { return einkDisplay.getBufferSize(); }

  // Hint the X3 policy to run a one-shot full resync on next update
  void requestResync(uint8_t settlePasses = 0) { einkDisplay.requestResync(settlePasses); }

  void clearScreen(uint8_t color = 0xFF) const;
  void drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 bool fromProgmem = false) const;

  void displayBuffer(RefreshMode mode = RefreshMode::FAST_REFRESH, bool turnOffScreen = false);
  void refreshDisplay(RefreshMode mode = RefreshMode::FAST_REFRESH, bool turnOffScreen = false);

  void deepSleep();

  uint8_t* getFrameBuffer() const;

  void copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer);
  void copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer);
  void copyGrayscaleMsbBuffers(const uint8_t* msbBuffer);
  void cleanupGrayscaleBuffers(const uint8_t* bwBuffer);

  void displayGrayBuffer(bool turnOffScreen = false);

 private:
  EInkDisplay einkDisplay;
  bool x3 = false;
};
