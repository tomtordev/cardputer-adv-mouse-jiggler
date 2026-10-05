// Mouse Jiggler - Cardputer ADV: USB HID and/or BLE HID mouse with an on-device settings menu.
#include <M5Cardputer.h>
#include "ble_mouse.h"
#include "jiggler.h"
#include "settings.h"
#include "ui.h"
#include "usb_mouse.h"

namespace {

enum class Screen { Status, Menu, EditName };
enum class Dialog { None, ApplyRestart, ForgetPairings, RestartNow };
enum MenuItem : uint8_t {
    kItemLink,
    kItemName,
    kItemPattern,
    kItemInterval,
    kItemRandom,
    kItemScreenOff,
    kItemBrightness,
    kItemAutoStart,
    kItemForget,
    kItemRestart,
    kItemAbout,
    kItemGithub,
    kItemCount
};

struct Key {
    bool any = false;
    char ch  = 0;
    bool enter = false, del = false, tab = false, space = false, esc = false;
    bool up = false, down = false, left = false, right = false;
};

Jiggler jiggler;
Screen screen = Screen::Status;
Dialog dialog = Dialog::None;
uint8_t menuSel = 0, menuScroll = 0;
char nameBuf[kNameMax + 1];

// Values the running USB/BLE stacks were started with; changing them needs a restart.
LinkMode bootLink;
char bootName[kNameMax + 1];

bool screenOn          = true;
uint32_t lastActivity  = 0;
uint32_t lastFrame     = 0;
String toastMsg;
uint32_t toastUntil    = 0;
int batteryLevel       = -1;
bool charging          = false;
uint32_t lastBattery   = 0;
int lastBleBattery     = -1;

bool restartPending() {
    return settings.link != bootLink || strcmp(settings.deviceName, bootName) != 0;
}

void toast(const String& msg, uint32_t ms = 1400) {
    toastMsg   = msg;
    toastUntil = millis() + ms;
}

// ---------------------------------------------------------------- screen power

void applyBrightness() { M5Cardputer.Display.setBrightness(kBrightness[settings.brightnessIdx]); }

void screenSleep() {
    if (!screenOn) return;
    M5Cardputer.Display.setBrightness(0);
    M5Cardputer.Display.sleep();
    screenOn = false;
}

void screenWake() {
    if (screenOn) return;
    M5Cardputer.Display.wakeup();
    applyBrightness();
    screenOn  = true;
    lastFrame = 0;
}

// ---------------------------------------------------------------- keyboard

Key decode(const Keyboard_Class::KeysState& st) {
    Key k;
    k.enter = st.enter;
    k.del   = st.del;
    k.tab   = st.tab;
    k.space = st.space;
    if (!st.word.empty()) k.ch = st.word.back();
    // Cardputer arrow keys are ; . , / and ESC is the ` key (with or without Fn/Shift).
    k.esc   = k.ch == '`' || k.ch == '~';
    k.up    = k.ch == ';' || k.ch == ':';
    k.down  = k.ch == '.' || k.ch == '>';
    k.left  = k.ch == ',' || k.ch == '<';
    k.right = k.ch == '/' || k.ch == '?';
    k.any   = k.enter || k.del || k.tab || k.ch;
    return k;
}

Key pollKey(uint32_t now) {
    static Key held;
    static uint32_t heldSince = 0, lastRepeat = 0;
    auto& kb = M5Cardputer.Keyboard;

    if (kb.isChange()) {
        if (kb.isPressed()) {
            held      = decode(kb.keysState());
            heldSince = lastRepeat = now;
            return held;
        }
        held = Key();
        return Key();
    }
    // Auto-repeat for navigation and erase while held.
    bool repeatable = held.up || held.down || held.left || held.right || held.del;
    if (repeatable && kb.isPressed() && now - heldSince > 420 && now - lastRepeat > 90) {
        lastRepeat = now;
        return held;
    }
    return Key();
}

// ---------------------------------------------------------------- menu model

template <typename T>
T cycle(T v, int dir, uint8_t count) {
    return (T)(((int)v + dir + count) % count);
}

MenuRow menuRow(uint8_t item) {
    switch (item) {
        case kItemLink:       return {"Connection", linkModeName(settings.link), true};
        case kItemName:       return {"Device name", settings.deviceName, false};
        case kItemPattern:    return {"Pattern", patternName(settings.pattern), true};
        case kItemInterval:   return {"Interval", formatSeconds(kIntervalsSec[settings.intervalIdx]), true};
        case kItemRandom:     return {"Randomize", settings.randomize ? "On" : "Off", true};
        case kItemScreenOff: {
            uint16_t s = kScreenTimeoutsSec[settings.screenTimeoutIdx];
            return {"Screen off", s ? "after " + formatSeconds(s) : String("Never"), true};
        }
        case kItemBrightness: {
            String bar;
            for (uint8_t i = 0; i < kBrightnessCount; i++) bar += i <= settings.brightnessIdx ? "|" : ".";
            return {"Brightness", bar, true};
        }
        case kItemAutoStart:  return {"Start on boot", settings.autoStart ? "On" : "Off", true};
        case kItemForget:     return {"Forget BLE pairings", "", false};
        case kItemRestart:    return {"Restart device", restartPending() ? "needed" : "", false};
        case kItemAbout:      return {"Firmware", "v" FW_VERSION, false};
        case kItemGithub:     return {"GitHub", "/tomtordev", false};
        default:              return {"", "", false};
    }
}

void adjustItem(uint8_t item, int dir) {
    switch (item) {
        case kItemLink:    settings.link = cycle(settings.link, dir, (uint8_t)LinkMode::Count); break;
        case kItemPattern: settings.pattern = cycle(settings.pattern, dir, (uint8_t)Pattern::Count); break;
        case kItemInterval:
            settings.intervalIdx = cycle(settings.intervalIdx, dir, kIntervalCount);
            jiggler.reschedule();
            break;
        case kItemRandom:
            settings.randomize = !settings.randomize;
            jiggler.reschedule();
            break;
        case kItemScreenOff: settings.screenTimeoutIdx = cycle(settings.screenTimeoutIdx, dir, kScreenTimeoutCount); break;
        case kItemBrightness:
            settings.brightnessIdx = constrain((int)settings.brightnessIdx + dir, 0, kBrightnessCount - 1);
            applyBrightness();
            break;
        case kItemAutoStart: settings.autoStart = !settings.autoStart; break;
        default: break;
    }
}

void selectItem(uint8_t item) {
    switch (item) {
        case kItemName:
            strlcpy(nameBuf, settings.deviceName, sizeof(nameBuf));
            screen = Screen::EditName;
            break;
        case kItemForget:  dialog = Dialog::ForgetPairings; break;
        case kItemRestart: dialog = Dialog::RestartNow; break;
        default:           break;  // values change only with the left/right keys
    }
}

void leaveMenu() {
    settingsSave();
    screen = Screen::Status;
    if (restartPending()) dialog = Dialog::ApplyRestart;
}

void restartNow() {
    settingsSave();
    ui::splash(settings.link);
    delay(400);
    ESP.restart();
}

// ---------------------------------------------------------------- input handling

void handleDialogKey(const Key& k) {
    bool yes = k.enter;
    bool no  = k.esc || k.del;
    if (!yes && !no) return;
    Dialog d = dialog;
    dialog   = Dialog::None;
    if (!yes) {
        if (d == Dialog::ApplyRestart) toast("Applies on next restart", 1800);
        return;
    }
    switch (d) {
        case Dialog::ApplyRestart:
        case Dialog::RestartNow: restartNow(); break;
        case Dialog::ForgetPairings:
            BleMouse::forgetPairings();
            toast("Pairings cleared");
            break;
        default: break;
    }
}

void handleStatusKey(const Key& k) {
    if (k.enter) {
        jiggler.setRunning(!jiggler.running());
        toast(jiggler.running() ? "Jiggler started" : "Jiggler paused");
    } else if (k.space) {
        jiggler.triggerNow();
        toast("Jiggle!", 700);
    } else if (k.esc) {
        screen = Screen::Menu;
    } else if (k.ch == 'o' || k.ch == 'O') {
        screenSleep();
    } else if (k.left || k.right || k.up || k.down) {
        adjustItem(kItemInterval, (k.right || k.up) ? +1 : -1);
        settingsSave();
        toast("Every " + formatSeconds(kIntervalsSec[settings.intervalIdx]), 900);
    } else if (k.ch == 'p' || k.ch == 'P') {
        adjustItem(kItemPattern, +1);
        settingsSave();
        toast(String("Pattern: ") + patternName(settings.pattern), 900);
    }
}

void handleMenuKey(const Key& k) {
    if (k.esc || k.del) {
        leaveMenu();
    } else if (k.up || k.down) {
        menuSel = cycle(menuSel, k.down ? +1 : -1, kItemCount);
        if (menuSel < menuScroll) menuScroll = menuSel;
        if (menuSel >= menuScroll + ui::kMenuVisibleRows) menuScroll = menuSel - ui::kMenuVisibleRows + 1;
    } else if (k.left || k.right) {
        adjustItem(menuSel, k.right ? +1 : -1);
    } else if (k.enter) {
        selectItem(menuSel);
    }
}

void handleEditorKey(const Key& k) {
    size_t len = strlen(nameBuf);
    if (k.esc) {
        screen = Screen::Menu;
    } else if (k.enter) {
        String s = nameBuf;
        s.trim();
        if (s.length() == 0) {
            toast("Name can't be empty");
            return;
        }
        strlcpy(settings.deviceName, s.c_str(), sizeof(settings.deviceName));
        settingsSave();
        screen = Screen::Menu;
        if (restartPending()) toast("Restart to apply", 1600);
    } else if (k.del) {
        if (len) nameBuf[len - 1] = 0;
    } else if (k.ch >= 32 && k.ch < 127 && len < kNameMax) {
        nameBuf[len]     = k.ch;
        nameBuf[len + 1] = 0;
    }
}

void handleKey(const Key& k) {
    if (dialog != Dialog::None) return handleDialogKey(k);
    switch (screen) {
        case Screen::Status:   handleStatusKey(k); break;
        case Screen::Menu:     handleMenuKey(k); break;
        case Screen::EditName: handleEditorKey(k); break;
    }
}

// ---------------------------------------------------------------- rendering

void render(uint32_t now) {
    switch (screen) {
        case Screen::Status: {
            StatusView v;
            v.name           = bootName;  // what the PC currently sees
            v.link           = bootLink;
            v.usbConnected   = UsbMouse::connected();
            v.bleConnected   = BleMouse::connected();
            v.battery        = batteryLevel;
            v.charging       = charging;
            v.running        = jiggler.running();
            v.moving         = jiggler.moving();
            v.msLeft         = jiggler.msUntilNext(now);
            v.intervalMs     = jiggler.currentIntervalMs();
            v.pattern        = settings.pattern;
            v.randomize      = settings.randomize;
            v.restartPending = restartPending();
            ui::drawStatus(v, now);
            break;
        }
        case Screen::Menu: {
            MenuRow rows[kItemCount];
            for (uint8_t i = 0; i < kItemCount; i++) rows[i] = menuRow(i);
            ui::drawMenu(rows, kItemCount, menuSel, menuScroll);
            break;
        }
        case Screen::EditName: ui::drawEditor(nameBuf, (now / 450) % 2 == 0); break;
    }

    switch (dialog) {
        case Dialog::ApplyRestart:
            ui::drawDialog("Restart now?", "Connection / name changes\nneed a restart.", "restart", "later");
            break;
        case Dialog::ForgetPairings:
            ui::drawDialog("Forget pairings?", "Paired computers must\npair again.", "forget", "cancel");
            break;
        case Dialog::RestartNow: ui::drawDialog("Restart device?", "Settings are saved first.", "restart", "cancel"); break;
        default: break;
    }

    if ((int32_t)(toastUntil - now) > 0 && dialog == Dialog::None) ui::drawToast(toastMsg.c_str());
    ui::push();
}

void updateBattery(uint32_t now) {
    if (lastBattery && now - lastBattery < 5000) return;
    lastBattery  = now;
    batteryLevel = M5Cardputer.Power.getBatteryLevel();
    charging     = M5Cardputer.Power.isCharging() == m5::Power_Class::is_charging;
    if (batteryLevel >= 0 && abs(batteryLevel - lastBleBattery) >= 2) {
        BleMouse::setBatteryLevel(batteryLevel);
        lastBleBattery = batteryLevel;
    }
}

void sendMove(int8_t dx, int8_t dy) {
    if (bootLink != LinkMode::Ble) UsbMouse::move(dx, dy);  // each is a no-op when not connected
    if (bootLink != LinkMode::Usb) BleMouse::move(dx, dy);
}

}  // namespace

void setup() {
    setCpuFrequencyMhz(80);  // plenty for UI + HID, noticeably better battery life
    settingsLoad();
    bootLink = settings.link;
    strlcpy(bootName, settings.deviceName, sizeof(bootName));

    // USB is always started: it carries the upload/serial port even in BLE-only mode.
    UsbMouse::begin(settings.deviceName);

    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    ui::begin();
    applyBrightness();
    ui::splash(settings.link);

    if (settings.link != LinkMode::Usb) BleMouse::begin(settings.deviceName);

    randomSeed(esp_random());
    jiggler.begin(sendMove);
    jiggler.setRunning(settings.autoStart);

    // Play the boot animation for a moment (BLE start-up above already took some of it).
    for (uint32_t start = millis(), e = 0; e < 1200; e = millis() - start) {
        ui::splash(settings.link, e / 1000.0f);
        delay(16);
    }
    lastActivity = millis();
}

void loop() {
    uint32_t now = millis();
    M5Cardputer.update();

    Key k    = pollKey(now);
    bool btn = M5Cardputer.BtnA.wasPressed();
    if (k.any || btn) {
        lastActivity = now;
        if (!screenOn) {
            screenWake();  // the waking key press is swallowed
            k   = Key();
            btn = false;
        }
    }
    if (btn && dialog == Dialog::None) {
        jiggler.setRunning(!jiggler.running());
        toast(jiggler.running() ? "Jiggler started" : "Jiggler paused");
    }
    if (k.any) handleKey(k);

    jiggler.update(now);
    updateBattery(now);

    uint16_t timeout = kScreenTimeoutsSec[settings.screenTimeoutIdx];
    if (screenOn && timeout && now - lastActivity > (uint32_t)timeout * 1000) {
        screenSleep();
    }

    if (screenOn && now - lastFrame >= 40) {
        lastFrame = now;
        render(now);
    }
    delay(screenOn ? 4 : 8);
}
