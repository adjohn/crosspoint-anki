// Host-side test shim for <Arduino.h>. Only what the units under test
// actually touch. Test-only; never compiled on-device.
#pragma once

#include <cstdint>
#include <cstddef>
#include "WString.h"

inline unsigned long millis() { return 0; }
