#include "../src/policy.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
    SpeedPolicy p;
    assert(!p.update(10, 1.0f).write);
    p.cycle();
    auto r = p.update(10, 1.0f);
    assert(r.write && r.value == 2);
    p.cycle();
    r = p.update(10, 2.0f);
    assert(r.write && r.value == 4);
    r = p.update(10, 1.0f); // Battle resets timing in the same world.
    assert(r.write && r.value == 4 && p.multiplier() == 4);
    p.update(0, 0); // Loading preserves the selected speed.
    assert(p.multiplier() == 4);
    r = p.update(11, 1.0f); // New battle/field world.
    assert(r.write && r.value == 4);
    r = p.update(11, 4.0f, false); // In-game cinematic or movie.
    assert(r.write && r.value == 1 && p.multiplier() == 4);
    assert(!p.update(11, 1.0f, false).write);
    r = p.update(11, 1.0f, true);
    assert(r.write && r.value == 4);
    r = p.update(11, 0.25f); // Authored/foreign slow motion wins.
    assert(!r.write && r.conflict && p.multiplier() == 4);
    assert(!p.update(11, 0.25f).write);
    r = p.update(11, 1.0f);
    assert(r.write && r.value == 4); // Resume at normal baseline only.
    p.reset();
    r = p.update(11, 4.0f);
    assert(r.write && r.value == 1);
    assert(!p.update(11, NAN).write);
    assert(!p.update(11, 0).write);
    p.cycle();
    assert(!p.update(11, 1.0f, false).write);
    p.reset(); // Reset during a cutscene cancels the later acceleration.
    assert(!p.update(11, 1.0f, true).write && p.multiplier() == 1);
    p.cycle(); p.update(11, 1.0f);
    p.update(0, 0);
    assert(!p.update(11, 2.0f).write);
    p.reset();
    r = p.update(11, 2.0f);
    assert(r.write && r.value == 1);
    std::puts("Speed policy tests passed");
}
