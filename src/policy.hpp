#pragma once
#include <cstdint>
#include <cmath>

struct SpeedResult { bool write = false; float value = 1; bool conflict = false; };
class SpeedPolicy {
    std::uintptr_t world_ = 0;
    int multiplier_ = 1;
    bool owns_ = false;
    float last_ = 1;
public:
    void select(int value) { if (value == 1 || value == 2 || value == 4) multiplier_ = value; }
    void cycle() { multiplier_ = multiplier_ == 1 ? 2 : multiplier_ == 2 ? 4 : 1; }
    void reset() { multiplier_ = 1; }
    int multiplier() const { return multiplier_; }
    SpeedResult update(std::uintptr_t world, float observed, bool accelerate = true) {
        // Selection survives loading and world changes. Ownership belongs only
        // to a validated current identity, never to a retained raw pointer.
        if (!world) return {};
        if (world != world_) {
            world_ = world;
            owns_ = false;
        }
        if (!std::isfinite(observed) || observed <= 0 ||
            (owns_ && observed != last_ && observed != 1.0f) ||
            (!owns_ && observed != 1.0f)) {
            owns_ = false;
            return {false, observed, true};
        }
        float target = accelerate ? static_cast<float>(multiplier_) : 1.0f;
        if (!owns_ && target == 1) return {};
        bool write = target != observed;
        last_ = target;
        owns_ = target != 1;
        return {write, target, false};
    }
};
