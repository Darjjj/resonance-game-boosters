#include "../src/controls.hpp"
#include <cassert>
int main() {
    SpeedKeys keys;
    SpeedPolicy speed;
    int mode = 0;
    bool boost = false, automatic = false, visible = true;
    assert(keys.valid());
    for (unsigned key = 0x70; key <= 0x74; ++key) assert(keys.accepts(key));
    assert(!keys.accepts(0x75));
    auto press = [&](unsigned key) { keys.apply(key,speed,mode,boost,automatic,visible); };
    press(0x70); assert(speed.multiplier() == 2);
    press(0x70); assert(speed.multiplier() == 4);
    press(0x70); assert(speed.multiplier() == 1);
    press(0x71); assert(boost && !automatic && visible && mode == 0);
    press(0x71); assert(!boost);
    press(0x72); assert(mode == 1);
    press(0x72); assert(mode == 2);
    press(0x72); assert(mode == 0);
    press(0x73); assert(automatic && !boost && visible);
    press(0x73); assert(!automatic);
    press(0x74); assert(!visible && !automatic);
    press(0x74); assert(visible);
    keys.hud = keys.automatic; assert(!keys.valid());
    keys.hud = 0; assert(!keys.valid());
}
