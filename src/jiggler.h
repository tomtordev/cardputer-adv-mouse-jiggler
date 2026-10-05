#pragma once
#include <Arduino.h>
#include <functional>

// Non-blocking jiggle scheduler. Every pattern returns the cursor to where it started.
class Jiggler {
public:
    using MoveFn = std::function<void(int8_t dx, int8_t dy)>;

    void begin(MoveFn fn);
    void update(uint32_t now);

    void setRunning(bool run);
    bool running() const { return running_; }
    void triggerNow();
    void reschedule();  // apply a changed interval

    bool moving() const { return pos_ < count_; }
    uint32_t msUntilNext(uint32_t now) const;
    uint32_t currentIntervalMs() const { return intervalMs_; }

private:
    struct Step { int8_t dx, dy; };
    static constexpr uint8_t kMaxSteps = 64;

    void scheduleNext(uint32_t now);
    void buildSequence();
    void pushPath(const int16_t* xs, const int16_t* ys, uint8_t n);

    MoveFn move_;
    bool running_       = false;
    Step steps_[kMaxSteps];
    uint8_t count_      = 0;
    uint8_t pos_        = 0;
    uint32_t nextAt_    = 0;
    uint32_t stepAt_    = 0;
    uint32_t intervalMs_ = 30000;
    int8_t lastDir_     = 1;
};
