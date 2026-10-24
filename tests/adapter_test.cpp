#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
static HWND test_foreground = nullptr;
static HWND test_get_foreground() { return test_foreground; }
#define GetForegroundWindow test_get_foreground
#include "../src/plugin.cpp"
#undef GetForegroundWindow
#include <cassert>
#include <initializer_list>

template<class T> static void put(std::uintptr_t address, T value) {
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
}
int main() {
    static bool menu_or_results=false;
    query_indicator_screen=[] { return menu_or_results; };
    bool cinematic = false, detector_known = true;
    static bool* scene = &cinematic;
    static bool* known = &detector_known;
    query_cutscene = [](bool& active) { active = *scene; return *known; };
    query_battle = [](std::uintptr_t, BattleTiming&) { return true; };
    // Private synthetic memory, not a game process. Reserve the address range,
    // commit only the two pages needed for globals.
    image = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 0xA000000,
        MEM_RESERVE, PAGE_READWRITE));
    assert(image);
    for (auto offset : {GWorld, GObjects}) {
        assert(VirtualAlloc(reinterpret_cast<void*>((image + offset) & ~0xFFFULL),
            4096, MEM_COMMIT, PAGE_READWRITE));
    }
    alignas(8) unsigned char world[0x100]{}, level[0x300]{}, settings[0x500]{}, player[0xC00]{};
    alignas(8) unsigned char slots[4 * 0x18]{};
    auto wp = reinterpret_cast<std::uintptr_t>(world);
    auto lp = reinterpret_cast<std::uintptr_t>(level);
    auto sp = reinterpret_cast<std::uintptr_t>(settings);
    static std::uintptr_t pp;
    pp = reinterpret_cast<std::uintptr_t>(player);
    query_map_player = [] { return pp; };
    std::uintptr_t chunk = reinterpret_cast<std::uintptr_t>(slots);
    put(image + GObjects, reinterpret_cast<std::uintptr_t>(&chunk));
    put(image + GObjects + 0x14, 4);
    put(image + GObjects + 0x1C, 1);
    int index = 0;
    for (auto ptr : {wp, lp, sp, pp}) {
        Object obj{};
        obj.index = index;
        obj.outer = index >= 2 ? lp : wp;
        put(ptr, obj);
        put(chunk + index * 0x18, ptr);
        // Zero serial is legitimate until the first weak reference is made.
        put(chunk + index * 0x18 + 0x10, std::uint32_t{0});
        ++index;
    }
    put(image + GWorld, wp);
    put(wp + 0x30, lp);
    put(lp + 0xC0, wp);
    put(lp + 0x2B0, sp);
    put(sp + 0x3E0, 1.0f);
    put(sp + 0x3EC, 0.01f);
    put(sp + 0x3F0, 20.0f);
    window = reinterpret_cast<HWND>(1);
    test_foreground = window;
    Timing t;
    auto select = [&](int multiplier) { policy.select(multiplier); update(); };
    assert(timing(t));
    put(pp + 0xB10, 0.5f);
    EncounterRatio ratio;
    assert(encounter_ratio(wp, ratio) && ratio.value == 0.5f);
    assert(!encounter_ratio(wp + 1, ratio));
    update(VK_F3);
    assert(encounter_mode == 1 && read(pp + 0xB10, t.value) && t.value == 0);
    cinematic = true; update();
    assert(read(pp + 0xB10, t.value) && t.value == 0);
    cinematic = false;
    update(VK_F3);
    assert(encounter_mode == 2 && read(pp + 0xB10, t.value) && t.value == 1.0f);
    update(VK_F3);
    assert(encounter_mode == 0 && read(pp + 0xB10, t.value) && t.value == 0.5f);
    put(chunk + 3 * 0x18, std::uintptr_t{0});
    assert(!encounter_ratio(wp, ratio)); // Freed player rejected.
    put(chunk + 3 * 0x18, pp);
    update(); select(2);
    assert(read(sp + 0x3E0, t.value) && t.value == 2);
    select(4);
    assert(read(sp + 0x3E0, t.value) && t.value == 4);
    put(sp + 0x3E0, 1.0f); // Battle-end timing reset.
    update();
    assert(read(sp + 0x3E0, t.value) && t.value == 4);
    cinematic = true;
    update();
    assert(read(sp + 0x3E0, t.value) && t.value == 1 && policy.multiplier() == 4);
    cinematic = false;
    update();
    assert(read(sp + 0x3E0, t.value) && t.value == 4);
    detector_known = false;
    update();
    assert(read(sp + 0x3E0, t.value) && t.value == 1);
    detector_known = true;
    update();
    assert(read(sp + 0x3E0, t.value) && t.value == 4);
    select(1);
    assert(read(sp + 0x3E0, t.value) && t.value == 1);
    select(2);
    put(sp + 0x3E0, 0.5f);
    select(1); // Reset must not overwrite another writer.
    assert(read(sp + 0x3E0, t.value) && t.value == 0.5f);
    put(sp + 0x3E0, 1.0f);
    select(2);
    test_foreground = nullptr;
    update();
    assert(read(sp + 0x3E0, t.value) && t.value == 1);
    test_foreground = window;
    select(1); // Explicit reset clears the remembered background selection.
    put(sp + 0x3F0, 2.0f);
    select(2); select(4);
    assert(read(sp + 0x3E0, t.value) && t.value == 1);
    put(lp + 0xC0, std::uintptr_t{0});
    assert(!timing(t));
    put(lp + 0xC0, wp);
    put(chunk + 2 * 0x18, std::uintptr_t{0});
    assert(!timing(t)); // Freed object must not be writable.
    put(chunk + 2 * 0x18, sp);

    // Combat uses its persistent base factor, not repeated world-time writes.
    put(sp + 0x3F0, 20.0f);
    put(sp + 0x3E0, 1.0f);
    put(sp + 0x480, 1.0f);
    policy = SpeedPolicy{};
    static BattleTiming combat;
    combat = {true, sp + 0x480, 99, 1, 0.5f};
    query_battle = [](std::uintptr_t, BattleTiming& out) {
        out = combat;
        if (out.active) return read(out.address, out.value);
        return true;
    };
    static int preparations=0;
    prepare_auto_attack=[](std::uintptr_t) { ++preparations; };
    static int auto_calls=0;
    run_auto_attack=[](std::uintptr_t) { ++auto_calls; };
    prepare_auto_complete=[] {};
    static int completions=0;
    run_auto_complete=[](std::uintptr_t,int mode,ULONGLONG) { if(mode==2) ++completions; };
    update(VK_F4); assert(auto_attack_enabled && auto_calls==1 && indicator_status().auto_attack);
    cinematic=true; update(); assert(auto_calls==1 && preparations==2);
    cinematic=false; test_foreground=nullptr; update(); assert(auto_calls==1);
    test_foreground=window;
    update(VK_F4); assert(auto_attack_enabled==2);
    assert(auto_calls==2 && completions==1);
    cinematic=true; update(); assert(completions==1);
    cinematic=false; test_foreground=nullptr; update(); assert(completions==1);
    test_foreground=window;
    update(VK_F4); assert(!auto_attack_enabled);
    static int refills = 0;
    refill_limit_burst = [](std::uintptr_t) { ++refills; };
    update(VK_F2);
    assert(limit_enabled && refills == 1);
    cinematic = true; update();
    assert(refills == 1);
    cinematic = false;
    update(VK_F2);
    assert(!limit_enabled && refills == 1);
    select(4);
    assert(read(sp + 0x480, t.value) && t.value == 4);
    assert(read(sp + 0x3E0, t.value) && t.value == 1);
    assert(combat.effects == 0.5f); // Authored effects are not overwritten.
    assert(indicator_status().selected == 4);
    cinematic = true; update();
    assert(read(sp + 0x480, t.value) && t.value == 1);
    assert(!indicator_allowed());
    cinematic = false; update();
    assert(read(sp + 0x480, t.value) && t.value == 4);
    assert(indicator_allowed());
    menu_or_results=true; update();
    assert(!indicator_allowed() && indicator_status().selected==4);
    menu_or_results=false; update();
    assert(indicator_allowed());
    combat.active = false; update();
    update(VK_F4);
    int before=preparations;
    update();
    assert(preparations==before+1 && auto_calls==2); // Prepare during exploration; never submit an attack.
    update(VK_F4);
    assert(auto_attack_enabled==2);
    update(VK_F4);
    assert(read(sp + 0x3E0, t.value) && t.value == 4);
    select(1);
    assert(read(sp + 0x3E0, t.value) && t.value == 1);

    // Exercise an actual Windows thread hook and message queue without focusing
    // a window or sending keyboard input to the user's desktop.
    window = CreateWindowExW(0, L"STATIC", L"Speed test", 0, 0, 0, 1, 1,
        HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(window);
    test_foreground = window;
    timer_id = 9021;
    message_hook = SetWindowsHookExW(WH_GETMESSAGE, on_message, nullptr, GetCurrentThreadId());
    assert(message_hook);
    assert(SetTimer(window, timer_id, 20, nullptr));
    auto pump = [] {
        auto until = GetTickCount64() + 100;
        while (GetTickCount64() < until) {
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
            Sleep(1);
        }
    };
    policy = SpeedPolicy{};
    pump();
    PostMessageW(window, WM_KEYDOWN, VK_F1, 0);
    pump();
    assert(read(sp + 0x3E0, t.value) && t.value == 2);
    PostMessageW(window, WM_KEYDOWN, VK_F1, 1LL << 30);
    pump();
    assert(read(sp + 0x3E0, t.value) && t.value == 2); // Held key doesn't cycle.
    put(sp + 0x3F0, 20.0f);
    PostMessageW(window, WM_SYSKEYDOWN, VK_F1, 0);
    pump();
    assert(read(sp + 0x3E0, t.value) && t.value == 4);
    PostMessageW(window, WM_KEYDOWN, VK_F1, 0);
    pump();
    assert(read(sp + 0x3E0, t.value) && t.value == 1);
    PostMessageW(window, WM_KEYDOWN, VK_F3, 0);
    pump();
    assert(encounter_mode == 1 && read(pp + 0xB10, t.value) && t.value == 0);
    PostMessageW(window, WM_KEYDOWN, VK_F3, 1LL << 30);
    pump();
    assert(encounter_mode == 1); // Held key must not re-enable encounters.
    PostMessageW(window, WM_KEYDOWN, VK_F3, 0);
    pump();
    assert(encounter_mode == 2 && read(pp + 0xB10, t.value) && t.value == 1.0f);
    update(VK_F3);
    assert(encounter_mode == 0 && read(pp + 0xB10, t.value) && t.value == 0.5f);
    PostMessageW(window, WM_KEYDOWN, VK_F2, 0);
    pump();
    assert(limit_enabled && refills == 1); // Exploration never refills gauges.
    PostMessageW(window, WM_KEYDOWN, VK_F2, 1LL << 30);
    pump(); assert(limit_enabled);
    PostMessageW(window, WM_KEYDOWN, VK_F2, 0);
    pump(); assert(!limit_enabled);
    PostMessageW(window,WM_KEYDOWN,VK_F4,0); pump();
    assert(auto_attack_enabled && indicator_status().auto_attack && auto_calls==2);
    PostMessageW(window,WM_KEYDOWN,VK_F4,1LL<<30); pump(); assert(auto_attack_enabled);
    PostMessageW(window,WM_KEYDOWN,VK_F4,0); pump(); assert(auto_attack_enabled==2);
    PostMessageW(window,WM_KEYDOWN,VK_F4,0); pump(); assert(!auto_attack_enabled);
    static bool game_hud_visible=true;
    update_game_hud=[](bool visible) { game_hud_visible=visible; return true; };
    PostMessageW(window,WM_KEYDOWN,VK_F5,0); pump();
    assert(!show_indicator && !game_hud_visible && !indicator_allowed());
    PostMessageW(window,WM_KEYDOWN,VK_F5,1LL<<30); pump(); assert(!show_indicator);
    PostMessageW(window,WM_KEYDOWN,VK_F5,0); pump();
    assert(show_indicator && game_hud_visible && !indicator_allowed()); // No active boosters.
    // Do not hide only the mod HUD while game-HUD discovery is unavailable.
    update_game_hud=[](bool) { return false; };
    update(VK_F5);
    assert(!show_indicator && hud_visible);
    update_game_hud=[](bool visible) { game_hud_visible=visible; return true; };
    update();
    assert(!hud_visible && !game_hud_visible);
    update(VK_F5);
    assert(show_indicator && hud_visible && game_hud_visible);
    KillTimer(window, timer_id);
    UnhookWindowsHookEx(message_hook);
    DestroyWindow(window);
    VirtualFree(reinterpret_cast<void*>(image), 0, MEM_RELEASE);
    std::puts("Speed adapter tests passed (synthetic memory and Windows message hook)");
}


