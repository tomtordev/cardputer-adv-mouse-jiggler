#pragma once
#include <Arduino.h>

// BLE HID-over-GATT mouse (works with Windows, macOS, Linux, Android, iOS).
namespace BleMouse {
void begin(const char* deviceName);
bool started();
bool connected();
void move(int8_t dx, int8_t dy);
void setBatteryLevel(uint8_t percent);
void forgetPairings();  // deletes all bonds and drops current connections
}  // namespace BleMouse
