#pragma once
#include <Arduino.h>

// USB HID mouse (plus a CDC serial port so uploads keep working without the BOOT button).
namespace UsbMouse {
void begin(const char* productName);
bool connected();  // enumerated by a host and not suspended
void move(int8_t dx, int8_t dy);
}  // namespace UsbMouse
