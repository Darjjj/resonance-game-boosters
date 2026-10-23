#pragma once
#include "policy.hpp"
#include <unordered_map>

class EncounterPolicy {
    struct Owned { float original, applied; };
    // Never retain raw UObject pointers across callbacks/map transitions.
    std::unordered_map<std::uintptr_t, Owned> originals_;
public:
    bool pending() const { return !originals_.empty(); }
    // 0=default, 1=no random encounters, 2=twice the authored ratio.
    SpeedResult update(std::uintptr_t identity, float observed, int mode) {
        if (!identity || !std::isfinite(observed) || observed < 0 || mode < 0 || mode > 2) return {};
        auto found = originals_.find(identity);
        if (mode == 0) {
            if (found == originals_.end()) return {};
            if (observed == found->second.applied && observed != found->second.original)
                return {true, found->second.original, false};
            originals_.erase(found); // Restoration observed, or another writer took over.
            return {};
        }
        float baseline = found != originals_.end() && observed == found->second.applied ?
            found->second.original : observed;
        float target = mode == 1 ? 0 : baseline * 2;
        if (!std::isfinite(target)) return {};
        if (target == observed) return {};
        originals_[identity] = {baseline, target};
        return {true, target, false};
    }
};
