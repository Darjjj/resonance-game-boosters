#pragma once
#include "game_function.hpp"
#include "battle_resources.hpp"
#include <unordered_map>

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
static ULONGLONG (*clock)() = [] { return GetTickCount64(); };
// Every ProcessEvent call costs about 0.4ms in this game. A unit's parameter
// object, relation and id cannot change during a battle, so they are resolved
// once per unit and only revalidated by object identity on later ticks.
struct Known { std::uintptr_t parameter = 0, parameter_identity = 0; unsigned char relation = 0; int id = -1; };
static std::unordered_map<std::uintptr_t, Known> known;
// A setter that left the value unchanged is not repeated every tick while the
// value stays there. It is retried once a second in case the game only refused
// it momentarily.
struct Ineffective { int value; ULONGLONG until; };
static std::unordered_map<long long, Ineffective> ineffective;
static std::uintptr_t known_world = 0;
static void forget() { known.clear(); ineffective.clear(); }
static bool identity_of(std::uintptr_t pointer, std::uintptr_t& id) {
    Object object{};
    std::uint32_t serial = 0;
    if (!live(pointer, object, serial)) return false;
    id = (static_cast<std::uintptr_t>(serial) << 32) | static_cast<std::uint32_t>(object.index);
    return true;
}
static void refill(std::uintptr_t world) {
    bool ready=false;
    { performance::Scope sample(performance::SuperDiscovery); ready=discover(); }
    if (!ready) return;
    if (world != known_world) { known_world = world; forget(); }
    Units units;
    bool have_units=false;
    { performance::Scope sample(performance::SuperUnits); have_units=query_units(world,units); }
    if (!have_units || units.count < 0 || units.count > 64) return;
    int maximum = 0;
    if (!functions[0].call(0, &maximum) || maximum <= 0 || maximum > 1000000) return;
    auto now = clock();
    for (int i = 0; i < units.count; ++i) {
        std::uintptr_t actor = 0, owner_world = 0, parameter_identity = 0;
        Object object{};
        std::uint32_t serial = 0;
        if (!read(units.data + i * 8, actor) || !live(actor, object, serial) ||
            !read(object.outer + 0xC0, owner_world) || owner_world != world) continue;
        auto identity = (static_cast<std::uintptr_t>(serial) << 32) | static_cast<std::uint32_t>(object.index);
        auto found = known.find(identity);
        if (found != known.end() && (!identity_of(found->second.parameter, parameter_identity) ||
            parameter_identity != found->second.parameter_identity)) {
            known.erase(found); found = known.end();
        }
        if (found == known.end()) {
            Known unit;
            if (!functions[1].call(actor, &unit.parameter) || !identity_of(unit.parameter, unit.parameter_identity) ||
                !functions[2].call(unit.parameter, &unit.relation) ||
                (unit.relation == 1 && (!functions[3].call(unit.parameter, &unit.id) || unit.id < 0))) continue;
            found = known.emplace(identity, unit).first;
        }
        const Known& unit = found->second;
        if (unit.relation != 1) continue;
        int point = -1;
        if (!functions[4].call(unit.parameter, &point) || point < 0) continue;
        // resource: 0 HP, 1 MP, 2 Limit Burst.
        auto wanted = [&](int resource, int current, int cap) {
            long long key = static_cast<long long>(unit.id) * 4 + resource;
            auto skip = ineffective.find(key);
            if (current >= cap) { if (skip != ineffective.end()) ineffective.erase(skip); return false; }
            return skip == ineffective.end() || skip->second.value != current || now >= skip->second.until;
        };
        auto settle = [&](int resource, int before, int after) {
            long long key = static_cast<long long>(unit.id) * 4 + resource;
            if (after == before) ineffective[key] = {before, now + 1000};
            else ineffective.erase(key);
        };
        bool valid = true;
        battle_resources::Points points;
        bool have_points=false;
        { performance::Scope sample(performance::SuperPoints); have_points=battle_resources::query_points(world,unit.parameter,points); }
        for (int resource = 0; have_points && valid && resource < 2; ++resource) {
            int cap=resource ? points.mp_max : points.hp_max;
            int current=resource ? points.mp : points.hp;
            if (!wanted(resource, current, cap)) continue;
            struct { int id, value; } set{unit.id, cap};
            performance::Scope sample(performance::SuperSet);
            if (!functions[10 + resource].call(0, &set)) { valid = false; break; }
            battle_resources::Points after;
            if (battle_resources::query_points(world, unit.parameter, after))
                settle(resource, current, resource ? after.mp : after.hp);
        }
        if (valid && wanted(2, point, maximum)) {
            struct { int id, point; } set{unit.id, maximum};
            performance::Scope sample(performance::SuperSet);
            int after = -1;
            if (!functions[5].call(0, &set)) valid = false;
            else if (functions[4].call(unit.parameter, &after)) settle(2, point, after);
        }
        if (!valid) {
            for (auto& fn : functions) fn.query = {};
            cursor = 0;
            forget();
            return;
        }
    }
}
}
static void (*refill_limit_burst)(std::uintptr_t) = limit_burst::refill;
