#include "settings.h"
#include <Preferences.h>

Settings settings;

const uint16_t kIntervalsSec[] = {5, 15, 30, 45, 60, 120, 180, 300};
const uint8_t kIntervalCount   = sizeof(kIntervalsSec) / sizeof(kIntervalsSec[0]);

const uint16_t kScreenTimeoutsSec[] = {0, 15, 30, 60, 120, 300};
const uint8_t kScreenTimeoutCount   = sizeof(kScreenTimeoutsSec) / sizeof(kScreenTimeoutsSec[0]);

const uint8_t kBrightness[]    = {16, 48, 96, 160, 255};
const uint8_t kBrightnessCount = sizeof(kBrightness) / sizeof(kBrightness[0]);

static constexpr const char* kNs = "jiggler";

void settingsLoad() {
    Preferences p;
    if (!p.begin(kNs, true)) return;  // first boot: namespace missing, keep defaults

    Settings d;
    settings.link = (LinkMode)min<uint8_t>(p.getUChar("link", (uint8_t)d.link), (uint8_t)LinkMode::Count - 1);
    String name   = p.getString("name", d.deviceName);
    name.trim();
    if (name.length() == 0) name = d.deviceName;
    strlcpy(settings.deviceName, name.c_str(), sizeof(settings.deviceName));
    settings.pattern = (Pattern)min<uint8_t>(p.getUChar("pattern", (uint8_t)d.pattern), (uint8_t)Pattern::Count - 1);
    uint16_t sec         = p.getUShort("intSec", kIntervalsSec[d.intervalIdx]);
    settings.intervalIdx = 0;
    for (uint8_t i = 1; i < kIntervalCount; i++) {  // nearest available interval
        if (abs((int)kIntervalsSec[i] - sec) < abs((int)kIntervalsSec[settings.intervalIdx] - sec)) settings.intervalIdx = i;
    }
    settings.randomize        = p.getBool("random", d.randomize);
    settings.screenTimeoutIdx = min<uint8_t>(p.getUChar("scrTo", d.screenTimeoutIdx), kScreenTimeoutCount - 1);
    settings.brightnessIdx    = min<uint8_t>(p.getUChar("bright", d.brightnessIdx), kBrightnessCount - 1);
    settings.autoStart        = p.getBool("auto", d.autoStart);
    p.end();
}

void settingsSave() {
    Preferences p;
    if (!p.begin(kNs, false)) return;
    p.putUChar("link", (uint8_t)settings.link);
    p.putString("name", settings.deviceName);
    p.putUChar("pattern", (uint8_t)settings.pattern);
    p.putUShort("intSec", kIntervalsSec[settings.intervalIdx]);
    p.putBool("random", settings.randomize);
    p.putUChar("scrTo", settings.screenTimeoutIdx);
    p.putUChar("bright", settings.brightnessIdx);
    p.putBool("auto", settings.autoStart);
    p.end();
}

const char* linkModeName(LinkMode m) {
    switch (m) {
        case LinkMode::Usb:  return "USB";
        case LinkMode::Ble:  return "BLE";
        case LinkMode::Both: return "USB+BLE";
        default:             return "?";
    }
}

const char* patternName(Pattern p) {
    switch (p) {
        case Pattern::Invisible: return "Invisible";
        case Pattern::Nudge:     return "Nudge";
        case Pattern::Circle:    return "Circle";
        default:                 return "?";
    }
}

String formatSeconds(uint32_t sec) {
    if (sec < 60) return String(sec) + "s";
    uint32_t m = sec / 60, s = sec % 60;
    if (m < 60) return s ? String(m) + "m" + String(s) + "s" : String(m) + "m";
    uint32_t h = m / 60;
    m %= 60;
    return m ? String(h) + "h" + String(m) + "m" : String(h) + "h";
}
