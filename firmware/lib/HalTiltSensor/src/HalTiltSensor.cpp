#include <HalTiltSensor.h>

bool HalTiltSensor::writeReg(uint8_t reg, uint8_t val) const {
  Wire.beginTransmission(_i2cAddr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool HalTiltSensor::readReg(uint8_t reg, uint8_t* val) const {
  Wire.beginTransmission(_i2cAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  Wire.requestFrom(_i2cAddr, (uint8_t)1);
  if (Wire.available() < 1) {
    return false;
  }
  *val = Wire.read();
  return true;
}

bool HalTiltSensor::readGyro(float& gx, float& gy, float& gz) const {
  Wire.beginTransmission(_i2cAddr);
  Wire.write(REG_GX_L);  // Start reading at Gyro X Low
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  Wire.requestFrom(_i2cAddr, (uint8_t)6);
  if (Wire.available() < 6) {
    return false;
  }

  auto readInt16 = [&]() -> int16_t {
    const uint8_t lo = Wire.read();
    const uint8_t hi = Wire.read();
    return static_cast<int16_t>((hi << 8) | lo);
  };

  // If Full Scale is ±512 dps, the scale factor is 32768 / 512 = 64 LSB/dps
  constexpr float SCALE = 1.0f / 64.0f;
  gx = readInt16() * SCALE;
  gy = readInt16() * SCALE;
  gz = readInt16() * SCALE;
  return true;
}

void HalTiltSensor::begin(const HalGPIO& gpio) {
  if (!gpio.deviceIsX3()) {
    _available = false;
    return;
  }

  // Try primary address, then alternate
  uint8_t whoami = 0;
  _i2cAddr = I2C_ADDR_QMI8658;
  if (!readReg(QMI8658_WHO_AM_I_REG, &whoami) || whoami != QMI8658_WHO_AM_I_VALUE) {
    _i2cAddr = I2C_ADDR_QMI8658_ALT;
    if (!readReg(QMI8658_WHO_AM_I_REG, &whoami) || whoami != QMI8658_WHO_AM_I_VALUE) {
      Serial.printf("[%lu] [GYR] QMI8658 IMU not found\n", millis());
      _available = false;
      return;
    }
  }

  if (!writeReg(REG_CTRL7, CTRL7_DISABLE_ALL) || !writeReg(REG_CTRL3, CTRL3_FS_512DPS | CTRL3_ODR_28HZ) ||
      !writeReg(REG_CTRL1, CTRL1_BASE | CTRL1_SENSOR_DISABLE)) {
    Serial.printf("[%lu] [GYR] QMI8658 register configuration failed\n", millis());
    _available = false;
    return;
  }

  _available = true;
  _initMs = millis();
  _lastPollMs = millis();
  Serial.printf("[%lu] [GYR] QMI8658 gyro initialized at 0x%02X and put to sleep\n", millis(), _i2cAddr);
}

bool HalTiltSensor::wake() {
  if (!_available) {
    return false;
  }

  // Wait for init to complete before waking
  if ((millis() - _initMs) < SLEEP_STABILIZE_MS) {
    return false;
  }

  if (writeReg(REG_CTRL1, CTRL1_BASE) && writeReg(REG_CTRL7, CTRL7_GYRO_ENABLE)) {
    _lastPollMs = millis();
    _lastTiltMs = millis();
    _wakeMs = millis();
    return true;
  }
  Serial.printf("[%lu] [GYR] Failed to wake QMI8658\n", millis());
  return false;
}

bool HalTiltSensor::deepSleep() {
  if (!_available) {
    return false;
  }

  if ((millis() - _wakeMs) < SLEEP_STABILIZE_MS) {
    return false;
  }

  if (writeReg(REG_CTRL7, CTRL7_DISABLE_ALL) && writeReg(REG_CTRL1, CTRL1_BASE | CTRL1_SENSOR_DISABLE)) {
    // Clear any residual state so it doesn't immediately trigger upon waking
    clearPendingEvents();
    _inTilt = false;
    return true;
  }
  Serial.printf("[%lu] [GYR] Failed to put QMI8658 to sleep\n", millis());
  return false;
}

void HalTiltSensor::update(const bool enabled) {
  if (!_available) {
    return;
  }

  // State machine: wake up or sleep based on the enabled flag
  if (enabled && !_isAwake) {
    _isAwake = wake();
    return;
  } else if (!enabled && _isAwake) {
    _isAwake = !deepSleep();
    return;
  }

  // If disabled, skip the rest of the polling logic and avoid unnecessary I2C traffic
  if (!enabled) {
    return;
  }

  const unsigned long now = millis();
  // Stabilization: discard readings during gyro startup transient
  if ((now - _wakeMs) < WAKE_STABILIZE_MS) {
    return;
  }

  if ((now - _lastPollMs) < POLL_INTERVAL_MS) {
    return;
  }
  _lastPollMs = now;

  float gx, gy, gz;
  if (!readGyro(gx, gy, gz)) {
    return;
  }

  // On the X3 PCB the gyro X axis is the left/right tilt axis in portrait,
  // which is the only orientation this app uses.
  const float tiltAxis = gx;

  if (_inTilt) {
    // Wait for device to return to neutral before allowing next trigger
    if (fabsf(tiltAxis) < NEUTRAL_RATE_DPS) {
      _inTilt = false;
    }
  } else {
    // Check for new tilt gesture (with cooldown)
    if ((now - _lastTiltMs) >= COOLDOWN_MS) {
      if (tiltAxis > RATE_THRESHOLD_DPS) {
        _tiltForwardEvent = true;
        _hadActivity = true;
        _inTilt = true;
        _lastTiltMs = now;
      } else if (tiltAxis < -RATE_THRESHOLD_DPS) {
        _tiltBackEvent = true;
        _hadActivity = true;
        _inTilt = true;
        _lastTiltMs = now;
      }
    }
  }
}

bool HalTiltSensor::wasTiltedForward() {
  const bool val = _tiltForwardEvent;
  _tiltForwardEvent = false;
  return val;
}

bool HalTiltSensor::wasTiltedBack() {
  const bool val = _tiltBackEvent;
  _tiltBackEvent = false;
  return val;
}

bool HalTiltSensor::hadActivity() {
  const bool val = _hadActivity;
  _hadActivity = false;
  return val;
}

void HalTiltSensor::clearPendingEvents() {
  _tiltForwardEvent = false;
  _tiltBackEvent = false;
  _hadActivity = false;
  // Intentionally preserve _inTilt so a held tilt doesn't retrigger on next poll
}
