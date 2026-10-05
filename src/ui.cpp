#include "ui.h"
#include <M5Cardputer.h>

namespace {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr uint16_t kBg       = rgb(10, 14, 20);
constexpr uint16_t kPanel    = rgb(22, 28, 38);
constexpr uint16_t kPanel2   = rgb(36, 44, 58);
constexpr uint16_t kText     = rgb(232, 238, 244);
constexpr uint16_t kMuted    = rgb(124, 136, 154);
constexpr uint16_t kAccent   = rgb(46, 230, 166);
constexpr uint16_t kAccentLo = rgb(16, 70, 54);
constexpr uint16_t kWarn     = rgb(255, 184, 77);
constexpr uint16_t kBlue     = rgb(88, 166, 255);
constexpr uint16_t kRed      = rgb(255, 96, 96);

constexpr int W = 240, H = 135;
constexpr int kHeaderH = 20, kFooterY = 121;

M5Canvas canvas(&M5Cardputer.Display);

// The 1.14" panel's pixels are taller than wide (0.1038 x 0.1101 mm), so anything meant to look
// round is drawn with its vertical radius scaled by this factor.
constexpr float kYScale = 0.1038f / 0.1101f;

uint16_t blend(uint16_t a, uint16_t b, float t) {
    if (t <= 0) return a;
    if (t >= 1) return b;
    int ar = a >> 11, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    int br = b >> 11, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    return ((int)(ar + (br - ar) * t) << 11) | ((int)(ag + (bg - ag) * t) << 5) | (int)(ab + (bb - ab) * t);
}

// Pre-computed anti-aliased ring pixels (built once per ring size).
struct RingPx {
    int8_t dx, dy;
    uint8_t coverage;  // 0-255
    uint16_t angle;    // 0-65535 = 0-360 degrees, clockwise from 12 o'clock
};
std::vector<RingPx> ringPx;
int ringR0 = -1, ringR1 = -1;

void buildRing(int r0, int r1) {
    ringPx.clear();
    int ryMax = (int)ceilf((r1 + 1) * kYScale) + 1;
    for (int dy = -ryMax; dy <= ryMax; dy++) {
        float fy = dy / kYScale;  // vertical distance in horizontal-pixel units
        for (int dx = -(r1 + 1); dx <= r1 + 1; dx++) {
            float d   = sqrtf(dx * dx + fy * fy);
            float cov = min(constrain(r1 + 0.5f - d, 0.0f, 1.0f), constrain(d - (r0 - 0.5f), 0.0f, 1.0f));
            if (cov <= 0) continue;
            float a = atan2f(dx, -fy);
            if (a < 0) a += 2 * PI;
            ringPx.push_back({(int8_t)dx, (int8_t)dy, (uint8_t)lroundf(cov * 255), (uint16_t)(a / (2 * PI) * 65535)});
        }
    }
    ringR0 = r0;
    ringR1 = r1;
}

void drawRing(int cx, int cy, int r0, int r1, float progress) {
    if (r0 != ringR0 || r1 != ringR1) buildRing(r0, r1);
    const float pa = progress * 2 * PI, rm = (r0 + r1) / 2.0f;
    for (const auto& q : ringPx) {
        float a     = q.angle * (2 * PI / 65535);
        float fill  = progress > 0 ? constrain((pa - a) * rm + 0.5f, 0.0f, 1.0f) : 0;  // soft leading edge
        uint16_t c  = blend(kPanel2, kAccent, fill);
        canvas.drawPixel(cx + q.dx, cy + q.dy, blend(kBg, c, q.coverage / 255.0f));
    }
}

// Keycap tokens drawn as arrow icons (compared by pointer).
const char* const kKeyUpDown    = "UD";
const char* const kKeyLeftRight = "LR";

const lgfx::IFont* const kFontTiny  = &fonts::DejaVu9;
const lgfx::IFont* const kFontBody  = &fonts::DejaVu12;
const lgfx::IFont* const kFontTitle = &fonts::FreeSansBold9pt7b;
const lgfx::IFont* const kFontBig   = &fonts::FreeSansBold12pt7b;
const lgfx::IFont* const kFontHuge  = &fonts::FreeSansBold18pt7b;

void text(const String& s, int x, int y, const lgfx::IFont* font, uint16_t color, textdatum_t datum) {
    canvas.setFont(font);
    canvas.setTextColor(color);
    canvas.setTextDatum(datum);
    canvas.drawString(s, x, y);
}

String fit(const String& s, int maxW, const lgfx::IFont* font) {
    canvas.setFont(font);
    if (canvas.textWidth(s) <= maxW) return s;
    String t = s;
    while (t.length() && canvas.textWidth(t + "..") > maxW) t.remove(t.length() - 1);
    return t + "..";
}

bool isArrowKey(const char* key) { return key == kKeyUpDown || key == kKeyLeftRight; }

int keyWidth(const char* key) {
    if (isArrowKey(key)) return 20;
    canvas.setFont(kFontTiny);
    return canvas.textWidth(key) + 6;
}

// Small rounded "keycap" followed by a label. Returns the x after the pair.
int keyHint(int x, int y, const char* key, const char* label) {
    int kw = keyWidth(key);
    canvas.fillSmoothRoundRect(x, y - 6, kw, 12, 3, kPanel2);
    int cx = x + kw / 2;
    if (key == kKeyUpDown) {
        canvas.fillTriangle(cx - 7, y + 2, cx - 1, y + 2, cx - 4, y - 3, kText);
        canvas.fillTriangle(cx + 1, y - 2, cx + 7, y - 2, cx + 4, y + 3, kText);
    } else if (key == kKeyLeftRight) {
        canvas.fillTriangle(cx - 6, y, cx - 2, y - 3, cx - 2, y + 3, kText);
        canvas.fillTriangle(cx + 6, y, cx + 2, y - 3, cx + 2, y + 3, kText);
    } else {
        text(key, cx, y, kFontTiny, kText, middle_center);
    }
    x += kw + 3;
    text(label, x, y, kFontTiny, kMuted, middle_left);
    return x + canvas.textWidth(label) + 8;
}

int hintsWidth(const char* const* pairs, int n) {
    canvas.setFont(kFontTiny);
    int w = 0;
    for (int i = 0; i < n; i++) {
        w += keyWidth(pairs[i * 2]) + 3 + 8;
        canvas.setFont(kFontTiny);
        w += canvas.textWidth(pairs[i * 2 + 1]);
    }
    return w - 8;
}

void footer(const char* const* pairs, int n) {
    canvas.fillRect(0, kFooterY, W, H - kFooterY, kPanel);
    int x = (W - hintsWidth(pairs, n)) / 2;
    int y = kFooterY + (H - kFooterY) / 2;
    for (int i = 0; i < n; i++) x = keyHint(x, y, pairs[i * 2], pairs[i * 2 + 1]);
}

void header(const String& title, const lgfx::IFont* font = kFontBody) {
    canvas.fillRect(0, 0, W, kHeaderH, kPanel);
    text(title, 7, kHeaderH / 2, font, kText, middle_left);
}

void pill(int x, int y, const char* label, bool on, bool blink) {
    canvas.setFont(kFontTiny);
    int w = canvas.textWidth(label) + 10;
    if (on) {
        canvas.fillSmoothRoundRect(x, y, w, 13, 6, kAccent);
        text(label, x + w / 2, y + 7, kFontTiny, kBg, middle_center);
    } else {
        canvas.fillSmoothRoundRect(x, y, w, 13, 6, kPanel2);
        if (blink) canvas.drawRoundRect(x, y, w, 13, 6, kBlue);
        text(label, x + w / 2, y + 7, kFontTiny, blink ? kBlue : kMuted, middle_center);
    }
}

int pillWidth(const char* label) {
    canvas.setFont(kFontTiny);
    return canvas.textWidth(label) + 10;
}

void battery(int xRight, int yMid, int level, bool charging) {
    const int bw = 20, bh = 10;
    int x = xRight - bw - 2, y = yMid - bh / 2;
    canvas.drawRoundRect(x, y, bw, bh, 2, kMuted);
    canvas.fillRect(x + bw, y + 3, 2, 4, kMuted);
    if (level >= 0) {
        uint16_t c = level <= 15 ? kRed : (level <= 35 ? kWarn : kAccent);
        int fw = (bw - 4) * constrain(level, 0, 100) / 100;
        if (fw > 0) canvas.fillRect(x + 2, y + 2, fw, bh - 4, c);
        text(String(level) + "%", x - 3, yMid, kFontTiny, kText, middle_right);
    }
    if (charging) {
        int cx = x + bw / 2, cy = yMid;
        canvas.fillTriangle(cx + 1, cy - 6, cx - 4, cy + 1, cx, cy + 1, kText);
        canvas.fillTriangle(cx - 1, cy + 6, cx + 4, cy - 1, cx, cy - 1, kText);
    }
}

String clock(uint32_t sec) {
    char buf[16];
    uint32_t h = sec / 3600, m = (sec / 60) % 60, s = sec % 60;
    if (h) snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    else snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)m, (unsigned long)s);
    return buf;
}

void mouseIcon(int cx, int cy, int s, uint16_t body, uint16_t detail) {
    int w = s, h = s * 3 / 2;
    canvas.fillSmoothRoundRect(cx - w / 2, cy - h / 2, w, h, w / 2, body);
    canvas.drawFastVLine(cx, cy - h / 2, h * 2 / 5, detail);
    canvas.drawFastHLine(cx - w / 2, cy - h / 2 + h * 2 / 5, w, detail);
    canvas.fillSmoothRoundRect(cx - 2, cy - h / 2 + 5, 4, 8, 2, detail);
}

}  // namespace

namespace ui {

void begin() {
    M5Cardputer.Display.setRotation(1);
    canvas.setColorDepth(16);
    canvas.createSprite(W, H);
}

void push() { canvas.pushSprite(0, 0); }

void splash(LinkMode link, float t) {
    canvas.fillScreen(kBg);

    // Mouse wiggles side to side (with a slight bob) while motion lines trail behind it.
    const int cx = 50, cy = 67, tx = 104;
    const float w  = 2 * PI * 1.6f * t;
    float ox       = 9.0f * sinf(w);
    float vx       = cosf(w);  // horizontal velocity, -1..1
    int mx = cx + lroundf(ox), my = cy + lroundf(2.5f * sinf(2 * w) * kYScale);
    if (fabsf(vx) > 0.25f) {
        int dir  = vx > 0 ? -1 : 1;  // lines appear on the trailing side
        int edge = mx + dir * 18;
        for (int i = -1; i <= 1; i++) {
            int len = lroundf((i == 0 ? 10 : 6) * fabsf(vx));
            int y   = my + lroundf(i * 9 * kYScale);
            uint16_t c = i == 0 ? kAccent : kAccentLo;
            canvas.drawWideLine(edge, y, edge + dir * len, y, 1.2f, c);
        }
    }
    mouseIcon(mx, my, 26, kAccent, kBg);

    text("Mouse", tx, 38, kFontTitle, kText, middle_left);
    text("Jiggler", tx, 58, kFontTitle, kAccent, middle_left);
    text("Cardputer ADV", tx, 78, kFontTiny, kMuted, middle_left);
    text(String(linkModeName(link)) + "  v" FW_VERSION, tx, 93, kFontTiny, kMuted, middle_left);
    push();
}

void drawStatus(const StatusView& v, uint32_t now) {
    canvas.fillScreen(kBg);

    // Header: name, link pills, battery.
    canvas.fillRect(0, 0, W, kHeaderH, kPanel);
    bool blink = (now / 600) % 2;
    int x = 236 - 56;  // battery + percentage take ~48 px on the right, plus breathing room
    if (v.link != LinkMode::Usb) {
        x -= pillWidth("BLE") + 4;
        pill(x, 3, "BLE", v.bleConnected, !v.bleConnected && blink);
    }
    if (v.link != LinkMode::Ble) {
        x -= pillWidth("USB") + 4;
        pill(x, 3, "USB", v.usbConnected, false);
    }
    text(fit(v.name, x - 12, kFontBody), 7, kHeaderH / 2, kFontBody, kText, middle_left);
    battery(236, kHeaderH / 2, v.battery, v.charging);

    // Countdown ring: anti-aliased track and progress arc.
    const int cx = 52, cy = 71, r1 = 37, r0 = 30;
    float progress = 0;
    if (v.running && v.intervalMs) progress = constrain(1.0f - (float)v.msLeft / v.intervalMs, 0.0f, 1.0f);
    drawRing(cx, cy, r0, r1, progress);
    if (v.moving) {
        mouseIcon(cx, cy, 20, kAccent, kBg);
    } else if (v.running) {
        // Baseline-anchored so the digits (not the font's line box) sit on the ring centre.
        uint32_t s = (v.msLeft + 999) / 1000;
        if (s < 60) {
            text(String(s), cx, cy + 12, kFontHuge, kText, baseline_center);
            text("sec", cx, cy + 20, kFontTiny, kMuted, middle_center);
        } else {
            text(clock(s), cx, cy + 8, kFontBig, kText, baseline_center);
            text("min", cx, cy + 18, kFontTiny, kMuted, middle_center);
        }
    } else {
        canvas.fillSmoothRoundRect(cx - 10, cy - 12, 7, 24, 2, kWarn);
        canvas.fillSmoothRoundRect(cx + 3, cy - 12, 7, 24, 2, kWarn);
    }

    // Status + details, block vertically centred on the ring.
    const int lx = 104, rx = 234, ty = cy - 18;
    if (v.running) {
        canvas.fillSmoothCircle(lx + 5, ty, 4, kAccent);
        text("ACTIVE", lx + 15, ty + 1, kFontBig, kAccent, middle_left);
    } else {
        text("PAUSED", lx, ty + 1, kFontBig, kWarn, middle_left);
    }

    struct { const char* k; String val; } rows[] = {
        {"Pattern", patternName(v.pattern)},
        {"Every", String(v.randomize ? "~" : "") + formatSeconds(kIntervalsSec[settings.intervalIdx])},
    };
    int y = cy + 6;
    for (auto& r : rows) {
        text(r.k, lx, y, kFontBody, kMuted, middle_left);
        text(r.val, rx, y, kFontBody, kText, middle_right);
        y += 16;
    }
    if (v.restartPending) {
        canvas.fillSmoothCircle(lx + 5, y, 5, kWarn);
        text("!", lx + 5, y, kFontTiny, kBg, middle_center);
        text("Restart to apply", lx + 15, y, kFontTiny, kWarn, middle_left);
    }

    static const char* const hints[] = {"ENT", "run", "SPC", "now", "ESC", "menu", "O", "off"};
    footer(hints, 4);
}

void drawMenu(const MenuRow* rows, uint8_t count, uint8_t selected, uint8_t scroll) {
    canvas.fillScreen(kBg);
    header("Settings", kFontTitle);

    const int rowH = 19, top = kHeaderH + 3;
    for (uint8_t i = 0; i < kMenuVisibleRows && scroll + i < count; i++) {
        uint8_t idx    = scroll + i;
        const auto& r  = rows[idx];
        int y          = top + i * rowH;
        bool sel       = idx == selected;
        int mid        = y + rowH / 2 - 1;
        if (sel) {
            canvas.fillSmoothRoundRect(4, y, 226, rowH - 2, 4, kPanel2);
            canvas.fillRect(4, y + 3, 3, rowH - 8, kAccent);
        }
        text(r.label, 13, mid, kFontBody, sel ? kText : kMuted, middle_left);
        constexpr int kArrowW = 4, kArrowGap = 5;
        int vx = 225;  // right edge of the value (or of the right arrow)
        if (sel && r.adjustable) {
            canvas.fillTriangle(vx - kArrowW, mid - 4, vx - kArrowW, mid + 4, vx, mid, kAccent);
            vx -= kArrowW + kArrowGap;
        }
        canvas.setFont(kFontBody);
        String val = fit(r.value, 110, kFontBody);
        text(val, vx, mid, kFontBody, sel ? kAccent : kMuted, middle_right);
        if (sel && r.adjustable) {
            int lx = vx - canvas.textWidth(val) - kArrowGap;
            canvas.fillTriangle(lx, mid - 4, lx, mid + 4, lx - kArrowW, mid, kAccent);
        }
    }

    // Scrollbar.
    if (count > kMenuVisibleRows) {
        int trackY = top, trackH = kMenuVisibleRows * rowH - 2;
        int thumbH = max(10, trackH * kMenuVisibleRows / count);
        int thumbY = trackY + (trackH - thumbH) * scroll / (count - kMenuVisibleRows);
        canvas.fillRect(235, trackY, 2, trackH, kPanel);
        canvas.fillRect(235, thumbY, 2, thumbH, kMuted);
    }

    static const char* const hints[] = {kKeyUpDown, "move", kKeyLeftRight, "change", "ENT", "ok", "ESC", "back"};
    footer(hints, 4);
}

void drawEditor(const char* value, bool cursorOn) {
    canvas.fillScreen(kBg);
    header("Device name", kFontTitle);

    const int bx = 10, by = 38, bw = 220, bh = 28;
    canvas.fillSmoothRoundRect(bx, by, bw, bh, 6, kPanel2);
    canvas.drawRoundRect(bx, by, bw, bh, 6, kAccent);
    canvas.setFont(kFontBody);
    int tw = canvas.textWidth(value);
    text(value, bx + 10, by + bh / 2, kFontBody, kText, middle_left);
    if (cursorOn) canvas.fillRect(bx + 11 + tw, by + 7, 2, bh - 14, kAccent);

    size_t len = strlen(value);
    text(String(len) + "/" + String(kNameMax), bx + bw, by + bh + 10, kFontTiny, len >= kNameMax ? kWarn : kMuted, middle_right);
    text("Shown on the PC for USB and Bluetooth.", bx, by + bh + 26, kFontTiny, kMuted, middle_left);
    text("Applies after a restart.", bx, by + bh + 38, kFontTiny, kMuted, middle_left);

    static const char* const hints[] = {"ENT", "save", "DEL", "erase", "ESC", "cancel"};
    footer(hints, 3);
}

void drawDialog(const char* title, const char* message, const char* yes, const char* no) {
    const int x = 6, y = 24, w = 228, h = 90;
    canvas.fillSmoothRoundRect(x - 2, y - 2, w + 4, h + 4, 10, kBg);
    canvas.fillSmoothRoundRect(x, y, w, h, 8, kPanel);
    canvas.drawRoundRect(x, y, w, h, 8, kAccent);
    text(title, x + w / 2, y + 16, kFontTitle, kText, middle_center);

    // Up to two lines separated by '\n'.
    String msg = message;
    int nl     = msg.indexOf('\n');
    if (nl < 0) {
        text(msg, x + w / 2, y + 42, kFontBody, kMuted, middle_center);
    } else {
        text(msg.substring(0, nl), x + w / 2, y + 36, kFontBody, kMuted, middle_center);
        text(msg.substring(nl + 1), x + w / 2, y + 51, kFontBody, kMuted, middle_center);
    }

    canvas.setFont(kFontTiny);
    const char* pairs[] = {"ENT", yes, "ESC", no};
    int n      = no ? 2 : 1;
    int hx     = x + (w - hintsWidth(pairs, n)) / 2;
    for (int i = 0; i < n; i++) hx = keyHint(hx, y + h - 14, pairs[i * 2], pairs[i * 2 + 1]);
}

void drawToast(const char* message) {
    canvas.setFont(kFontBody);
    int w = canvas.textWidth(message) + 20;
    int x = (W - w) / 2, y = kFooterY - 22;
    canvas.fillSmoothRoundRect(x, y, w, 18, 9, kAccent);
    text(message, W / 2, y + 9, kFontBody, kBg, middle_center);
}

}  // namespace ui
