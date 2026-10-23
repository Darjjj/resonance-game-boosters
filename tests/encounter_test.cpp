#include "../src/encounter_policy.hpp"
#include <cassert>
#include <limits>
int main() {
    EncounterPolicy p;
    assert(!p.update(1, 0.5f, false).write);
    auto r = p.update(1, 0.5f, true);
    assert(r.write && r.value == 0);
    assert(!p.update(1, 0, true).write);
    r = p.update(2, 1, true); // New map/player, separate baseline.
    assert(r.write && r.value == 0);
    r = p.update(1, 0, false);
    assert(r.write && r.value == 0.5f);
    r = p.update(2, 0, false);
    assert(r.write && r.value == 1);
    assert(!p.update(3, 0, true).write); // Authored no-encounter region.
    assert(!p.update(3, 0, false).write);
    p.update(4, 1, true);
    assert(!p.update(4, 0.25f, false).write); // Preserve another writer.
    p.update(5, 1, true);
    p.update(5, 0.25f, true); // Remember newly authored ratio while disabled.
    assert(p.update(5, 0, false).value == 0.25f);
    assert(!p.update(0, 1, true).write);
    assert(!p.update(6, std::numeric_limits<float>::quiet_NaN(), true).write);
    assert(!p.update(7, -1, true).write);
    EncounterPolicy cycle;
    assert(cycle.update(10, 0.5f, 1).value == 0);
    r = cycle.update(10, 0, 2);
    assert(r.write && r.value == 1.0f); // Twice the area's baseline.
    assert(!cycle.update(10, 1.0f, 2).write);
    r = cycle.update(10, 1.0f, 0);
    assert(r.write && r.value == 0.5f);
    assert(!cycle.update(11, 0, 2).write); // Encounter-free areas stay free.
    assert(cycle.update(12, 1, 2).value == 2);
    assert(cycle.update(12, 0.25f, 2).value == 0.5f); // New authored ratio.
    assert(cycle.update(12, 0.5f, 0).value == 0.25f);
}
