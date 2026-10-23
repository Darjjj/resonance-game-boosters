#pragma once
#include "game_function.hpp"
#include "battle_resources.hpp"

namespace limit_burst {
using Function = game_api::Function;
static std::array<Function, 12> functions{{
    {L"CPP_BFL_Unit", L"GetLimitBurstPointMaxGauge", true},
    {L"CPP_BtlUnit", L"GetUnitParameter", false},
    {L"CPP_UnitParameter", L"GetRelation", false},
    {L"CPP_UnitParameter", L"GetUnitId", false},
    {L"CPP_UnitParameter", L"GetLimitBurstPoint", false},
    {L"CPP_BFL_Battle", L"SetBattleUnitLimitBurstPoint", true},
    {L"CPP_UnitParameter", L"GetMaxHitPoint", false},
    {L"CPP_UnitParameter", L"GetMaxMagicPoint", false},
    {L"CPP_UnitParameter", L"GetHitPoint", false},
    {L"CPP_UnitParameter", L"GetMagicPoint", false},
    {L"CPP_BFL_Unit", L"SetPlayableUnitHitPoint", true},
    {L"CPP_BFL_Unit", L"SetPlayableUnitMagicPoint", true}
}};
static int cursor = 0;
static bool discover() { return game_api::discover(functions,cursor); }
struct Units { std::uintptr_t data = 0; int count = 0, capacity = 0; };
static bool native_units(std::uintptr_t world, Units& out) {
    using Lookup = std::uintptr_t(*)(void*);
    auto subsystem = reinterpret_cast<Lookup>(image + 0x4F3BA80)(reinterpret_cast<void*>(world));
    Object object{};
    std::uint32_t serial = 0;
    std::uintptr_t manager = 0;
    return live(subsystem, object, serial) && object.outer == world &&
        read(subsystem + 0x48, manager) && live(manager, object, serial) && object.outer == subsystem &&
        read(manager + 0x28, out) && out.count >= 0 && out.count <= 64 && out.capacity >= out.count;
}
static bool (*query_units)(std::uintptr_t, Units&) = native_units;
static void refill(std::uintptr_t world) {
    bool ready=false;
    { performance::Scope sample(performance::SuperDiscovery); ready=discover(); }
    if (!ready) return;
    Units units;
    bool have_units=false;
    { performance::Scope sample(performance::SuperUnits); have_units=query_units(world,units); }
    if (!have_units || units.count < 0 || units.count > 64) return;
    int maximum = 0;
    if (!functions[0].call(0, &maximum) || maximum <= 0 || maximum > 1000000) return;
    for (int i = 0; i < units.count; ++i) {
        std::uintptr_t actor = 0, owner_world = 0, parameter = 0;
        Object object{};
        std::uint32_t serial = 0;
        if (!read(units.data + i * 8, actor) || !live(actor, object, serial) ||
            !read(object.outer + 0xC0, owner_world) || owner_world != world ||
            !functions[1].call(actor, &parameter)) continue;
        unsigned char relation = 0;
        int id = -1, point = -1;
        if (!functions[2].call(parameter, &relation) || relation != 1 ||
            !functions[3].call(parameter, &id) || id < 0 ||
            !functions[4].call(parameter, &point) || point < 0) continue;
        bool valid = true;
        battle_resources::Points points;
        bool have_points=false;
        { performance::Scope sample(performance::SuperPoints); have_points=battle_resources::query_points(world,parameter,points); }
        for (int resource = 0; have_points && resource < 2; ++resource) {
            int cap=resource ? points.mp_max : points.hp_max;
            int current=resource ? points.mp : points.hp;
            if (current < cap) {
                struct { int id, value; } set{id, cap};
                performance::Scope sample(performance::SuperSet);
                valid &= functions[10 + resource].call(0, &set);
            }
        }
        struct { int id, point; } set{id, maximum};
        if (point < maximum) {
            performance::Scope sample(performance::SuperSet); valid &= functions[5].call(0, &set);
        }
        if (!valid) {
            for (auto& fn : functions) fn.query = {};
            cursor = 0;
            return;
        }
    }
}
}
static void (*refill_limit_burst)(std::uintptr_t) = limit_burst::refill;
