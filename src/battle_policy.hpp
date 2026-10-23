#pragma once
#include "policy.hpp"

class BattleSpeedPolicy {
    std::uintptr_t identity_ = 0;
    bool owns_ = false, blocked_ = false;
    float last_ = 1;
public:
    SpeedResult update(std::uintptr_t identity, float value, int target, bool explicit_key = false) {
        if (!identity) return {};
        if (identity != identity_) { identity_ = identity; owns_ = blocked_ = false; }
        if (!std::isfinite(value) || value <= 0) return {false, value, true};
        if (explicit_key) blocked_ = false;
        if (owns_ && value != last_ && value != 1) {
            owns_ = false;
            if (!explicit_key) { blocked_ = true; return {false, value, true}; }
        }
        if (value == 1) blocked_ = false;
        if (blocked_ || (!owns_ && value != 1 && value != 2 && value != 4)) return {false, value, true};
        if (!owns_ && target == 1 && !explicit_key) return {};
        last_ = static_cast<float>(target);
        owns_ = target != 1;
        return {value != last_, last_, false};
    }
};
