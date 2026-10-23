#pragma once
#include "encounter_policy.hpp"
struct EncounterRatio {
    std::uintptr_t address = 0, identity = 0;
    float value = 1;
};
// Pinned-build native getter performs GetPlayerCharacter + CPP_MapUnit_Playable
// class check. Called only on the game's window thread, never the worker.
static std::uintptr_t native_map_player() {
    using Getter = std::uintptr_t (*)();
    return reinterpret_cast<Getter>(image + 0x505A0E0)();
}
static std::uintptr_t (*query_map_player)() = native_map_player;
static bool encounter_ratio(std::uintptr_t world, EncounterRatio& ratio) {
    auto player = query_map_player();
    Object object{}, level{};
    std::uint32_t serial = 0, level_serial = 0;
    std::uintptr_t owning_world = 0;
    if (!live(player, object, serial) || !live(object.outer, level, level_serial) ||
        !read(object.outer + 0xC0, owning_world) || owning_world != world) return false;
    ratio.address = player + 0xB10;
    ratio.identity = (static_cast<std::uintptr_t>(serial) << 32) |
        static_cast<std::uint32_t>(object.index);
    return read(ratio.address, ratio.value) && std::isfinite(ratio.value) && ratio.value >= 0;
}
static EncounterPolicy encounter_policy;
static void update_encounters(std::uintptr_t world, int mode) {
    if (mode == 0 && !encounter_policy.pending()) return;
    EncounterRatio ratio;
    if (!encounter_ratio(world, ratio)) return;
    auto result = encounter_policy.update(ratio.identity, ratio.value, mode);
    if (!result.write) return;
    float current = 0;
    SIZE_T written = 0;
    if (read(ratio.address, current) && current == ratio.value &&
        WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(ratio.address),
            &result.value, sizeof(float), &written) && written == sizeof(float))
        log(mode == 1 ? "Random encounters suppressed." : mode == 2 ? "Double encounter multiplier applied." : "Default encounter multiplier restored.");
    else log("Random encounter write cancelled or failed.");
}
