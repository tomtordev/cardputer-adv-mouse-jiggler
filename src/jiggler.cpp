#include "jiggler.h"
#include "settings.h"
#include <math.h>

static constexpr uint32_t kStepMs = 12;

void Jiggler::begin(MoveFn fn) {
    move_ = std::move(fn);
    scheduleNext(millis());
}

void Jiggler::setRunning(bool run) {
    if (run == running_) return;
    running_ = run;  // an in-flight sequence still finishes so the cursor ends where it started
    if (run) scheduleNext(millis());
}

void Jiggler::triggerNow() {
    if (moving()) return;
    buildSequence();
    stepAt_ = millis();
}

void Jiggler::reschedule() { scheduleNext(millis()); }

uint32_t Jiggler::msUntilNext(uint32_t now) const {
    return (int32_t)(nextAt_ - now) > 0 ? nextAt_ - now : 0;
}

void Jiggler::scheduleNext(uint32_t now) {
    uint32_t base = (uint32_t)kIntervalsSec[settings.intervalIdx] * 1000;
    if (settings.randomize) {
        // +/-30 % so the movement doesn't look machine-regular.
        int32_t spread = base * 3 / 10;
        base += random(-spread, spread + 1);
    }
    intervalMs_ = max<uint32_t>(base, 2000);
    nextAt_     = now + intervalMs_;
}

void Jiggler::update(uint32_t now) {
    if (moving()) {
        if ((int32_t)(now - stepAt_) < 0) return;
        const Step& s = steps_[pos_++];
        if (move_) move_(s.dx, s.dy);
        stepAt_ = now + kStepMs;
        if (!moving()) scheduleNext(now);
        return;
    }
    if (running_ && (int32_t)(now - nextAt_) >= 0) {
        buildSequence();
        stepAt_ = now;
    }
}

// Converts absolute integer positions (starting at 0,0) into relative steps.
void Jiggler::pushPath(const int16_t* xs, const int16_t* ys, uint8_t n) {
    int16_t px = 0, py = 0;
    for (uint8_t i = 0; i < n && count_ < kMaxSteps; i++) {
        int16_t dx = xs[i] - px, dy = ys[i] - py;
        if (dx == 0 && dy == 0) continue;
        steps_[count_++] = {(int8_t)dx, (int8_t)dy};
        px = xs[i];
        py = ys[i];
    }
}

void Jiggler::buildSequence() {
    count_ = pos_ = 0;
    lastDir_   = -lastDir_;

    switch (settings.pattern) {
        case Pattern::Invisible: {
            // 1 px out and straight back: resets the OS idle timer without visible drift.
            steps_[count_++] = {lastDir_, 0};
            steps_[count_++] = {(int8_t)-lastDir_, 0};
            break;
        }
        case Pattern::Nudge: {
            constexpr uint8_t kHalf = 5;
            float angle = random(0, 360) * DEG_TO_RAD;
            float dist  = random(4, 10);
            int16_t xs[kHalf * 2], ys[kHalf * 2];
            for (uint8_t i = 0; i < kHalf; i++) {
                float t = (i + 1) / (float)kHalf;
                xs[i] = lroundf(cosf(angle) * dist * t);
                ys[i] = lroundf(sinf(angle) * dist * t);
            }
            for (uint8_t i = 0; i < kHalf; i++) {
                xs[kHalf + i] = i + 1 < kHalf ? xs[kHalf - 2 - i] : 0;
                ys[kHalf + i] = i + 1 < kHalf ? ys[kHalf - 2 - i] : 0;
            }
            pushPath(xs, ys, kHalf * 2);
            break;
        }
        case Pattern::Circle: {
            // Circle through the start point, centred R px to the left.
            constexpr uint8_t kN = 36;
            constexpr float kR  = 12.0f;
            int16_t xs[kN], ys[kN];
            for (uint8_t i = 0; i < kN; i++) {
                float a = (i + 1) * 2.0f * PI / kN;
                xs[i]   = lroundf(kR * cosf(a) - kR);
                ys[i]   = lroundf(kR * sinf(a) * lastDir_);
            }
            pushPath(xs, ys, kN);
            break;
        }
        default: break;
    }
}
