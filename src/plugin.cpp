#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cmath>
#include <atomic>
#include "policy.hpp"
#include "controls.hpp"

// Layout reference: PolarWizard/final-fantasy-resonance-fix,
// commit 0f5bc2c4bab2b1680c59d8d1c2135253ae14c0df. No SDK code linked.
static constexpr std::uintptr_t GWorld = 0x9AA0540, GObjects = 0x9D84DD0;
static constexpr char ExpectedHash[] =
    "b8c588e46ebadbe37feca7bdf2da0ae63a8538099d14a53ec0c2241b086e5ffa";
static HMODULE module;
static std::atomic<HWND> window{nullptr};
static HHOOK message_hook;
static UINT_PTR timer_id;
static FILE* logfile;
static wchar_t config_path[MAX_PATH];
static std::uintptr_t image;
static SpeedPolicy policy;
static SpeedKeys keys;
static std::atomic<int> encounter_mode{0};
static std::atomic<bool> limit_enabled{false};
static std::atomic<int> auto_attack_enabled{0};

static void log(const char* text) {
    if (logfile) { std::fprintf(logfile, "%llu %s\n", GetTickCount64(), text); std::fflush(logfile); }
}
#include "profiling.hpp"

template<class T> static bool read(std::uintptr_t address, T& out) {
    SIZE_T count = 0;
    return address > 0x10000 && ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<void*>(address), &out, sizeof(T), &count) && count == sizeof(T);
}

struct Object {
    std::uintptr_t vtable;
    std::uint32_t flags;
    std::int32_t index;
    std::uintptr_t klass;
    std::uint64_t name;
    std::uintptr_t outer;
};
static_assert(sizeof(Object) == 0x28);

// Validate the registry slot, not only whether the address is readable. Never
// retain raw game-object pointers across message callbacks or map transitions.
static bool live(std::uintptr_t ptr, Object& obj, std::uint32_t& serial, bool allow_default = false) {
    if (!read(ptr, obj) || obj.index < 0 || (obj.flags & (allow_default ? 0x18000 : 0x18010))) return false;
    std::int32_t count = 0, chunks_count = 0;
    std::uintptr_t chunks = 0, chunk = 0, registered = 0;
    if (!read(image + GObjects + 0x14, count) || count <= obj.index || count > 10000000 ||
        !read(image + GObjects + 0x1C, chunks_count) || chunks_count <= obj.index / 65536 ||
        !read(image + GObjects, chunks) ||
        !read(chunks + (obj.index / 65536) * 8, chunk)) return false;
    auto slot = chunk + (obj.index % 65536) * 0x18;
    return read(slot, registered) && registered == ptr &&
        read(slot + 0x10, serial);
}

#include "cutscene.hpp"
static bool (*query_cutscene)(bool&) = reflected_cutscene;
#include "battle.hpp"
#include "encounter.hpp"
#include "limit_burst.hpp"
#include "auto_attack.hpp"
#include "game_hud.hpp"
#include "screen_visibility.hpp"
#include "auto_complete.hpp"
#include "overlay.hpp"
#include "native_indicator.hpp"
static BattleSpeedPolicy battle_policy;

struct Timing {
    std::uintptr_t address = 0, identity = 0, world = 0;
    float value = 1, minimum = 0, maximum = 0;
};

static bool timing(Timing& t) {
    std::uintptr_t world = 0, level = 0, owner = 0, settings = 0;
    Object obj{};
    std::uint32_t serial = 0;
    if (!read(image + GWorld, world) || !live(world, obj, serial) ||
        !read(world + 0x30, level) || !live(level, obj, serial) ||
        !read(level + 0xC0, owner) || owner != world ||
        !read(level + 0x2B0, settings) || !live(settings, obj, serial) || obj.outer != level)
        return false;
    t.address = settings + 0x3E0;
    t.world = world;
    t.identity = (static_cast<std::uintptr_t>(serial) << 32) |
        static_cast<std::uint32_t>(obj.index);
    return read(t.address, t.value) && read(settings + 0x3EC, t.minimum) &&
        read(settings + 0x3F0, t.maximum) && std::isfinite(t.value) &&
        std::isfinite(t.minimum) && std::isfinite(t.maximum) &&
        t.minimum >= 0 && t.minimum <= 1 && t.maximum >= 1 && t.maximum <= 1000;
}

static bool apply_speed(std::uintptr_t address, float observed, const SpeedResult& result, const char* kind) {
    if (!result.write) return true;
    SIZE_T written = 0;
    float current = 0;
    if (!read(address, current) || current != observed ||
        !WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address),
            &result.value, sizeof(float), &written) || written != sizeof(float)) {
        log("Speed write cancelled or failed; control released."); return false;
    }
    char line[128];
    std::snprintf(line, sizeof(line), "Turbo Mode: applied %.0fx %s speed on window thread %lu.",
        result.value, kind, GetCurrentThreadId());
    log(line);
    return true;
}

static void update(UINT key = 0) {
    performance::Scope total(performance::Update);
    static std::uintptr_t reported_world = 0;
    bool focused = GetForegroundWindow() == window;
    if (focused && key) {
        int mode = encounter_mode.load();
        bool burst = limit_enabled.load();
        int automatic = auto_attack_enabled.load(); bool visible = show_indicator.load();
        keys.apply(key, policy, mode, burst, automatic, visible);
        if (automatic!=auto_attack_enabled.load()) log(automatic==2 ? "F4 Auto-Battle and Complete." : automatic==1 ? "F4 Auto-Battle." : "F4 Auto-Battle: OFF.");
        if (visible!=show_indicator.load()) log(visible ? "F5 HUD Hide/Show: SHOWN (game and mod)." : "F5 HUD Hide/Show: HIDDEN (game and mod).");
        auto_attack_enabled.store(automatic); show_indicator.store(visible);
        if (burst != limit_enabled.load()) log(burst ? "F2 Supercharge: ON (HP/MP/Limit Burst)." : "F2 Supercharge: OFF.");
        limit_enabled.store(burst);
        if (mode != encounter_mode.load())
            log(mode == 0 ? "F3 Encounter Rate: DEFAULT." : mode == 1 ? "F3 Encounter Rate: NONE." : "F3 Encounter Rate: DOUBLE.");
        encounter_mode.store(mode);
    }
    { performance::Scope sample(performance::Hud);
      bool visible=show_indicator.load();
      if(update_game_hud(visible)) {
          hud_visible.store(visible);
          if(!visible && indicator.load()) ShowWindowAsync(indicator.load(),SW_HIDE);
      }
    }
    screen_hides_indicator.store(query_indicator_screen());
    Timing t;
    bool valid_timing=false;
    { performance::Scope sample(performance::World); valid_timing=timing(t); }
    if (!valid_timing) {
        auto_attack::reset();
        auto_complete::reset();
        publish_status(policy.multiplier(), 1, SpeedMode::Unavailable, encounter_mode.load(), limit_enabled.load(), auto_attack_enabled.load());
        return;
    }
    if (reported_world != t.identity) {
        reported_world = t.identity;
        log("Validated current live world timing.");
    }
    update_encounters(t.world, encounter_mode.load());
    if (auto_attack_enabled.load()) {
        performance::Scope sample(performance::Prepare); prepare_auto_attack(t.world);
        if(auto_attack_enabled.load()==2) prepare_auto_complete();
    }
    bool cinematic = false;
    bool known=false;
    BattleTiming battle;
    bool battle_known=false;
    { performance::Scope sample(performance::Queries);
      known=query_cutscene(cinematic); battle_known=query_battle(t.world,battle); }
    if(battle_known && battle.active) total.stage=performance::Combat;
    static int reported_scene = -1;
    int scene = !known ? 0 : cinematic ? 1 : 2;
    if (reported_scene != scene) {
        reported_scene = scene;
        log(!known ? "Cutscene detection unavailable; acceleration suspended." :
            cinematic ? "Cutscene active; acceleration suspended at normal speed." :
            "Cutscene inactive; selected gameplay speed can resume.");
    }
    if (policy.multiplier() > t.maximum) {
        policy.reset();
        log("Requested speed exceeds this world's limits; returning to 1x.");
    }
    bool accelerate = focused && known && battle_known && !cinematic;
    if (accelerate && battle.active && limit_enabled.load()) {
        performance::Scope sample(performance::Supercharge); refill_limit_burst(t.world);
    }
    if (accelerate && battle.active && auto_attack_enabled.load()) {
        performance::Scope sample(performance::AutoBattle); run_auto_attack(t.world);
    }
    else auto_attack::reset();
    if(focused && known && !cinematic)
        run_auto_complete(t.world,auto_attack_enabled.load(),GetTickCount64());
    else auto_complete::reset();
    auto result = policy.update(t.identity, t.value, accelerate && !battle.active);
    if (!apply_speed(t.address, t.value, result, "exploration")) {
        policy = SpeedPolicy{}; return;
    }
    float effective = result.write ? result.value : t.value;
    bool conflict = result.conflict;
    if (battle_known && battle.active) {
        auto combat = battle_policy.update(battle.identity, battle.value,
            accelerate ? policy.multiplier() : 1, focused && key != 0);
        if (!apply_speed(battle.address, battle.value, combat, "battle base")) {
            battle_policy = BattleSpeedPolicy{}; return;
        }
        effective = (combat.write ? combat.value : battle.value) * battle.effects;
        conflict = combat.conflict;
    }
    SpeedMode mode = !known || !battle_known ? SpeedMode::Unavailable :
        !focused ? SpeedMode::Background : cinematic ? SpeedMode::Cutscene :
        conflict ? SpeedMode::OtherTiming : battle.active ? SpeedMode::Battle : SpeedMode::Gameplay;
    publish_status(policy.multiplier(), effective, mode, encounter_mode.load(), limit_enabled.load(), auto_attack_enabled.load());
}

static LRESULT CALLBACK on_message(int code, WPARAM removed, LPARAM data) {
    static bool reported_callback = false;
    if (code >= 0 && removed == PM_REMOVE) {
        if (!reported_callback) { reported_callback = true; log("Window message callback active."); }
        const auto& msg = *reinterpret_cast<MSG*>(data);
        if (msg.hwnd == window) {
            if (msg.message == WM_TIMER && msg.wParam == timer_id) { update(); update_native_indicator(); }
            else if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) &&
                !(msg.lParam & (1LL << 30)) &&
                keys.accepts(static_cast<unsigned>(msg.wParam))) { update(static_cast<UINT>(msg.wParam)); update_native_indicator(); }
        }
    }
    // Never consume the message or suppress another mod's hook.
    return CallNextHookEx(nullptr, code, removed, data);
}

static bool known_executable() {
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return false;
    auto file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (ok) ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    unsigned char buffer[65536], digest[32];
    DWORD count = 0;
    while (ok) {
        if (!ReadFile(file, buffer, sizeof(buffer), &count, nullptr)) { ok = false; break; }
        if (!count) break;
        ok = BCryptHashData(hash, buffer, count, 0) >= 0;
    }
    if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    char hex[65]{};
    if (ok) for (int i = 0; i < 32; ++i) std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    return ok && std::strcmp(hex, ExpectedHash) == 0;
}

static BOOL CALLBACK find_window(HWND candidate, LPARAM) {
    DWORD pid = 0;
    GetWindowThreadProcessId(candidate, &pid);
    wchar_t klass[64]{};
    GetClassNameW(candidate, klass, 64);
    if (pid == GetCurrentProcessId() && IsWindowVisible(candidate) &&
        std::wcscmp(klass, L"UnrealWindow") == 0) { window = candidate; return FALSE; }
    return TRUE;
}

static DWORD WINAPI start(void*) {
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(module, path, MAX_PATH)) return 0;
    auto slash = std::wcsrchr(path, L'\\');
    if (!slash) return 0;
    slash[1] = 0;
    std::swprintf(config_path, MAX_PATH, L"%lsresonance_boosters.ini", path);
    // Preserve pre-rename preferences on manual upgrades without overwriting
    // an existing boosters configuration. The installer also migrates it.
    if(GetFileAttributesW(config_path)==INVALID_FILE_ATTRIBUTES) {
        wchar_t legacy[MAX_PATH]{};
        std::swprintf(legacy,MAX_PATH,L"%lsresonance_speed.ini",path);
        CopyFileW(legacy,config_path,TRUE);
    }
    wchar_t logpath[MAX_PATH];
    std::swprintf(logpath, MAX_PATH, L"%lsresonance_boosters.log", path);
    logfile = _wfopen(logpath, L"w");
    log("Resonance Game Boosters 1.1.1 starting; F1 Turbo Mode; F2 Supercharge; F3 Encounter Rate; F4 Off/Auto-Battle/Auto-Battle and Complete; F5 HUD Hide/Show.");
    if (!GetPrivateProfileIntW(L"Speed", L"Enabled", 1, config_path)) { log("Disabled in config."); return 0; }
    if (!known_executable()) { log("Unsupported executable SHA256. No hook or game writes installed."); return 0; }
    image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    performance::configure(GetPrivateProfileIntW(L"Speed",L"ProfileUpdates",0,config_path)!=0);
    keys.cycle = GetPrivateProfileIntW(L"Speed", L"CycleKey", VK_F1, config_path);
    keys.encounter = GetPrivateProfileIntW(L"Speed", L"EncounterToggleKey", VK_F3, config_path);
    keys.automatic = GetPrivateProfileIntW(L"Speed", L"AutoAttackToggleKey", VK_F4, config_path);
    keys.hud = GetPrivateProfileIntW(L"Speed", L"HudToggleKey", VK_F5, config_path);
    int automatic_mode = GetPrivateProfileIntW(L"Speed", L"AutoAttackEnabled", 0, config_path);
    auto_attack_enabled = automatic_mode>=0 && automatic_mode<=2 ? automatic_mode : 0;
    keys.limit = GetPrivateProfileIntW(L"Speed", L"BattleBoostToggleKey", VK_F2, config_path);
    limit_enabled = GetPrivateProfileIntW(L"Speed", L"BattleBoostEnabled", 0, config_path) != 0;
    int mode = GetPrivateProfileIntW(L"Speed", L"EncounterMode", 0, config_path);
    encounter_mode = mode >= 0 && mode <= 2 ? mode : 0;
    show_indicator = GetPrivateProfileIntW(L"Speed", L"ShowIndicator", 1, config_path) != 0;
    indicator_x = GetPrivateProfileIntW(L"Speed", L"IndicatorX", 20, config_path);
    indicator_y = GetPrivateProfileIntW(L"Speed", L"IndicatorY", 240, config_path);
    if (!keys.valid()) {
        log("Invalid or duplicate hotkeys; disabled."); return 0;
    }
    // Pin our own module: a message hook must never point into an unloaded DLL.
    HMODULE pinned;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&start), &pinned)) return 0;
    timer_id = reinterpret_cast<UINT_PTR>(module);
    int saved_encounters = encounter_mode.load();
    bool saved_limit = limit_enabled.load();
    int saved_auto = auto_attack_enabled.load(); bool saved_visible = show_indicator.load();
    publish_status(1, 1, SpeedMode::Unavailable, encounter_mode.load(), limit_enabled.load(), auto_attack_enabled.load());
    // Unreal may replace its startup window. Follow destruction/recreation
    // instead of leaving the timer attached to the first temporary HWND.
    for (;;) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        { performance::Scope sample(performance::Discovery); refresh_indicator_screen(); }
        performance::report();
        update_indicator();
        int mode = encounter_mode.load();
        if (mode != saved_encounters) {
            if (!WritePrivateProfileStringW(L"Speed", L"EncounterMode", mode == 0 ? L"0" : mode == 1 ? L"1" : L"2", config_path))
                log("Encounter toggle active for this session; saving preference failed.");
            saved_encounters = mode;
        }
        bool burst = limit_enabled.load();
        if (burst != saved_limit) {
            if (!WritePrivateProfileStringW(L"Speed", L"BattleBoostEnabled", burst ? L"1" : L"0", config_path))
                log("Supercharge active for this session; saving preference failed.");
            saved_limit = burst;
        }
        int automatic = auto_attack_enabled.load(); bool visible = show_indicator.load();
        if (automatic != saved_auto) {
            WritePrivateProfileStringW(L"Speed", L"AutoAttackEnabled", automatic==2 ? L"2" : automatic==1 ? L"1" : L"0", config_path);
            saved_auto = automatic;
        }
        if (visible != saved_visible) {
            WritePrivateProfileStringW(L"Speed", L"ShowIndicator", visible ? L"1" : L"0", config_path);
            saved_visible = visible;
        }
        if (window && IsWindow(window)) { Sleep(50); continue; }
        if (message_hook) { UnhookWindowsHookEx(message_hook); message_hook = nullptr; }
        window = nullptr;
        EnumWindows(find_window, 0);
        if (!window) { Sleep(100); continue; }
        auto tid = GetWindowThreadProcessId(window, nullptr);
        message_hook = SetWindowsHookExW(WH_GETMESSAGE, on_message, module, tid);
        if (!message_hook) { log("Window message hook failed; disabled."); return 0; }
        if (!SetTimer(window, timer_id, 50, nullptr)) {
            UnhookWindowsHookEx(message_hook); log("Timer failed; disabled."); return 0;
        }
        char line[128];
        std::snprintf(line, sizeof(line), "Ready on window %p thread %lu. No executable patches installed.",
            static_cast<void*>(window.load()), tid);
        log(line);
    }
}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        module = instance;
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(nullptr, 0, start, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
