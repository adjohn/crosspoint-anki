#pragma once

#include <HalGPIO.h>

class HalTiltSensor;

class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };

  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };

  explicit MappedInputManager(HalGPIO& gpio) : gpio(gpio) {}

  bool wasPressed(Button button) const;
  bool wasReleased(Button button) const;
  bool isPressed(Button button) const;
  bool wasAnyPressed() const;
  bool wasAnyReleased() const;
  unsigned long getHeldTime() const;
  Labels mapLabels(const char* back, const char* confirm, const char* previous, const char* next) const;
  // Returns the raw front button index that was pressed this frame (or -1 if none).
  int getPressedFrontButton() const;

  // Tilt gesture input (X3 only; a null/absent sensor makes these no-ops).
  void setTiltSensor(HalTiltSensor* sensor);
  bool tiltAvailable() const;
  // Gate for tilt events; disabling discards any pending gestures.
  void setTiltEnabled(bool enabled);
  bool isTiltEnabled() const;
  // Consume-on-read: read at most once per frame per direction.
  bool wasTiltedForward() const;
  bool wasTiltedBack() const;
  // Discard pending tilt gestures (activity transitions, tilt-ignoring screens).
  void clearTiltEvents() const;

 private:
  HalGPIO& gpio;
  HalTiltSensor* tiltSensor = nullptr;
  bool tiltEnabled = true;

  bool mapButton(Button button, bool (HalGPIO::*fn)(uint8_t) const) const;
};
