#include "../src/battle_policy.hpp"
#include <cassert>
int main() {
    BattleSpeedPolicy p;
    auto r = p.update(10, 1, 4);
    assert(r.write && r.value == 4);
    assert(!p.update(10, 4, 4).write);
    r = p.update(10, 4, 1); // Cutscene / battle toggle off.
    assert(r.write && r.value == 1);
    r = p.update(10, 1, 4);
    assert(r.write && r.value == 4);
    r = p.update(11, 2, 4); // Native fast-speed preset already selected.
    assert(r.write && r.value == 4);
    r = p.update(11, 2, 4); // Another writer changed our factor.
    assert(!r.write && r.conflict);
    assert(!p.update(11, 2, 4).write);
    r = p.update(11, 2, 4, true); // An explicit speed key reasserts the player's choice.
    assert(r.write && r.value == 4);
    assert(p.update(11, 4, 1, true).value == 1);
    assert(!p.update(12, 0.5f, 4).write);
}
