#pragma once
#include <Arduino.h>
#include "settings.h"

struct StatusView {
    const char* name;
    LinkMode link;
    bool usbConnected;
    bool bleConnected;
    int battery;  // 0-100, <0 unknown
    bool charging;
    bool running;
    bool moving;
    uint32_t msLeft;
    uint32_t intervalMs;
    Pattern pattern;
    bool randomize;
    bool restartPending;
};

struct MenuRow {
    const char* label;
    String value;
    bool adjustable;  // shows < > arrows when selected
};

namespace ui {
void begin();
void splash(LinkMode link, float t = 0);  // t = seconds into the boot animation
void drawStatus(const StatusView& v, uint32_t now);
void drawMenu(const MenuRow* rows, uint8_t count, uint8_t selected, uint8_t scroll);
void drawEditor(const char* text, bool cursorOn);
void drawDialog(const char* title, const char* message, const char* yes, const char* no);
void drawToast(const char* message);
void push();

constexpr uint8_t kMenuVisibleRows = 5;
}  // namespace ui
