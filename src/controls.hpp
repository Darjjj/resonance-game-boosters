#pragma once
#include "policy.hpp"
struct SpeedKeys {
    unsigned cycle = 0x70, limit = 0x71, encounter = 0x72, automatic = 0x73, hud = 0x74;
    bool accepts(unsigned key) const {
        return key == cycle || key == limit || key == encounter || key == automatic || key == hud;
    }
    bool valid() const {
        unsigned all[]{cycle,limit,encounter,automatic,hud};
        for (int i = 0; i < 5; ++i) {
            if (!all[i] || all[i] >= 255) return false;
            for (int j = 0; j < i; ++j) if (all[j] == all[i]) return false;
        }
        return true;
    }
    void apply(unsigned key, SpeedPolicy& policy, int& mode, bool& boost, bool& automatic_enabled, bool& visible) const {
        if (key == cycle) policy.cycle();
        else if (key == limit) boost = !boost;
        else if (key == encounter) mode = (mode + 1) % 3;
        else if (key == automatic) automatic_enabled = !automatic_enabled;
        else if (key == hud) visible = !visible;
    }
};
