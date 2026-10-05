#pragma once
#include <Arduino.h>

enum class LinkMode : uint8_t { Usb = 0, Ble = 1, Both = 2, Count };
enum class Pattern : uint8_t { Invisible = 0, Nudge = 1, Circle = 2, Count };

constexpr size_t kNameMax = 20;

struct Settings {
    LinkMode link           = LinkMode::Both;
    char deviceName[kNameMax + 1] = "Mouse";
    Pattern pattern         = Pattern::Circle;
    uint8_t intervalIdx     = 2;  // 30 s
    bool randomize          = true;
    uint8_t screenTimeoutIdx = 2; // 30 s
    uint8_t brightnessIdx   = 2;
    bool autoStart          = false;
};

extern Settings settings;

// Option tables shared by the UI and the jiggler.
extern const uint16_t kIntervalsSec[];
extern const uint8_t kIntervalCount;
extern const uint16_t kScreenTimeoutsSec[];  // 0 = never
extern const uint8_t kScreenTimeoutCount;
extern const uint8_t kBrightness[];
extern const uint8_t kBrightnessCount;

void settingsLoad();
void settingsSave();

const char* linkModeName(LinkMode m);
const char* patternName(Pattern p);
String formatSeconds(uint32_t sec);  // "30s", "2m", "1m30s"
