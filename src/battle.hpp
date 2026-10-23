#pragma once
#include "battle_policy.hpp"

struct BattleTiming {
    bool active = false;
    std::uintptr_t address = 0, identity = 0;
    float value = 1, effects = 1;
};

// Installed-build disassembly: native UWorld battle-subsystem lookup at
// +4F3BA80; CPP_BtlManager.m_pGameSpeedManager at +60. Combat multiplies
// base(+2AC) * sequence(+2B0) * command(+2B4) * hit-stop(+2B8) before
// setting world time (+4F66A04..+4F66A33). +4F6263A writes the user-speed
// array's selected value to +2AC; +4F46080 resets these factors.
static bool native_battle_timing(std::uintptr_t world, BattleTiming& out) {
    using Lookup = std::uintptr_t(*)(void*);
    auto subsystem = reinterpret_cast<Lookup>(image + 0x4F3BA80)(reinterpret_cast<void*>(world));
    if (!subsystem) return true;
    Object obj{};
    std::uint32_t serial = 0;
    std::uintptr_t manager = 0, level_world = 0;
    if (!live(subsystem, obj, serial) || obj.outer != world || !read(subsystem + 0x60, manager)) return false;
    if (!manager) return true;
    if (!live(manager, obj, serial) || !read(obj.outer + 0xC0, level_world) || level_world != world) return false;
    out.active = true;
    out.address = manager + 0x2AC;
    out.identity = (static_cast<std::uintptr_t>(serial) << 32) | static_cast<std::uint32_t>(obj.index);
    float sequence = 1, command = 1, hit = 1;
    if (!read(out.address, out.value) || !read(manager + 0x2B0, sequence) ||
        !read(manager + 0x2B4, command) || !read(manager + 0x2B8, hit)) return false;
    out.effects = sequence * command * hit;
    return std::isfinite(out.value) && out.value > 0 && out.value <= 20 &&
        std::isfinite(out.effects) && out.effects >= 0 && out.effects <= 1000;
}
static bool (*query_battle)(std::uintptr_t, BattleTiming&) = native_battle_timing;
