#pragma once
#include <cstdint>
enum class SpeedMode { Gameplay, Battle, Cutscene, Unavailable, Background, OtherTiming };
struct IndicatorStatus {
    int selected;
    int encounters;
    bool boost, auto_attack;
};
