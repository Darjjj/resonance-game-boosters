#include "../src/plugin.cpp"
#include <cassert>
static std::uintptr_t addresses[26]{};
static int hp[2]{10, 30}, mp[2]{2, 15};
static int charge[2]{25, 40}, set_calls = 0, maximum = 300;
static void fake_process(void* self, void* function, void* parameters) {
    int index = static_cast<Object*>(function)->index;
    auto fn = index < 20 ? index - 7 : index - 14;
    int unit = reinterpret_cast<std::uintptr_t>(self) == addresses[14] ||
        reinterpret_cast<std::uintptr_t>(self) == addresses[16] ? 1 : 0;
    switch (fn) {
        case 0: *static_cast<int*>(parameters) = maximum; break;
        case 1: *static_cast<std::uintptr_t*>(parameters) = addresses[15 + unit]; break;
        case 2: *static_cast<unsigned char*>(parameters) = unit ? 2 : 1; break;
        case 3: *static_cast<int*>(parameters) = unit ? 999 : 17; break;
        case 4: *static_cast<int*>(parameters) = charge[unit]; break;
        case 5: {
            int* values = static_cast<int*>(parameters);
            assert(values[0] == 17); // Never refill an enemy.
            charge[0] = values[1]; ++set_calls; break;
        }
        case 6: assert(false && "Supercharge must not recalculate maximum HP each poll"); break;
        case 7: assert(false && "Supercharge must not recalculate maximum MP each poll"); break;
        case 8: *static_cast<int*>(parameters) = hp[unit]; break;
        case 9: assert(false && "Supercharge must not invoke the MP getter that recalculates its cap"); break;
        case 10: case 11: {
            int* values = static_cast<int*>(parameters); assert(values[0] == 17);
            (fn == 10 ? hp[0] : mp[0]) = values[1]; break;
        }
        default: assert(false);
    }
}
int main() {
    image = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 0xA000000, MEM_RESERVE, PAGE_READWRITE));
    assert(image && VirtualAlloc(reinterpret_cast<void*>((image + GObjects) & ~0xFFFULL),
        4096, MEM_COMMIT, PAGE_READWRITE));
    alignas(8) unsigned char objects[26][0x200]{};
    alignas(8) unsigned char slots[26][0x18]{};
    auto put = [](std::uintptr_t address, auto value) { std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value)); };
    std::uintptr_t chunk = reinterpret_cast<std::uintptr_t>(slots);
    std::uintptr_t vtable[0x4D]{};
    vtable[0x4C] = reinterpret_cast<std::uintptr_t>(&fake_process);
    put(image + GObjects, reinterpret_cast<std::uintptr_t>(&chunk));
    put(image + GObjects + 0x14, 26);
    put(image + GObjects + 0x1C, 1);
    for (int i = 0; i < 26; ++i) {
        addresses[i] = reinterpret_cast<std::uintptr_t>(objects[i]);
        Object obj{}; obj.index = i; obj.vtable = reinterpret_cast<std::uintptr_t>(vtable);
        put(addresses[i], obj); put(chunk + i * 0x18, addresses[i]);
    }
    put(addresses[0] + 0x18, std::uint64_t{999}); // Function class.
    for (int i = 1; i <= 4; ++i) put(addresses[i] + 0x18, std::uint64_t(200 + i));
    int owners[6]{1,2,3,3,3,4};
    for (int i = 0; i < 6; ++i) {
        auto fn = addresses[7 + i]; auto owner = addresses[owners[i]];
        put(fn + 0x10, addresses[0]); put(fn + 0x18, std::uint64_t(100 + i)); put(fn + 0x20, owner);
        put(fn + 0xB0, std::uint32_t(i == 0 || i == 5 ? 0x2400 : 0x400));
    }
    for (int i = 6; i < 12; ++i) {
        auto fn = addresses[14 + i]; auto owner = addresses[i < 10 ? 3 : 1];
        put(fn + 0x10, addresses[0]); put(fn + 0x18, std::uint64_t(100 + i)); put(fn + 0x20, owner);
        put(fn + 0xB0, std::uint32_t(i >= 10 ? 0x2400 : 0x400));
    }
    for (int i = 5; i <= 6; ++i) {
        int owner = i == 5 ? 1 : 4;
        put(addresses[i] + 8, std::uint32_t{0x10});
        put(addresses[i] + 0x10, addresses[owner]); put(addresses[owner] + 0x110, addresses[i]);
    }
    put(addresses[19] + 0x40, addresses[2]); // Derived actor class.
    for (int i = 13; i <= 16; ++i) {
        put(addresses[i] + 0x10, addresses[i <= 14 ? 19 : 3]);
        put(addresses[i] + 0x20, addresses[18]);
    }
    put(addresses[18] + 0xC0, addresses[17]);
    cutscene::object_name = [](std::uint64_t key) -> std::wstring {
        if (key == 999) return L"Function";
        if (key >= 100 && key < 112) return limit_burst::functions[key - 100].function_name;
        switch (key) {
            case 201: return L"CPP_BFL_Unit";
            case 202: return L"CPP_BtlUnit";
            case 203: return L"CPP_UnitParameter";
            case 204: return L"CPP_BFL_Battle";
            default: return L"None";
        }
    };
    static std::uintptr_t units[2]{addresses[13], addresses[14]};
    limit_burst::query_units = [](std::uintptr_t, limit_burst::Units& out) {
        out = {reinterpret_cast<std::uintptr_t>(units), 2, 2}; return true;
    };
    battle_resources::query_points=[](std::uintptr_t world,std::uintptr_t parameter,battle_resources::Points& out) {
        if(world!=addresses[17] || parameter!=addresses[15]) return false;
        out={hp[0],500,mp[0],60}; return true;
    };
    limit_burst::refill(addresses[17]);
    assert(charge[0] == 300 && charge[1] == 40 && set_calls == 1);
    assert(hp[0] == 500 && mp[0] == 60 && hp[1] == 30 && mp[1] == 15);
    hp[0] = 1; mp[0] = 0;
    limit_burst::refill(addresses[17]);
    assert(hp[0] == 500 && mp[0] == 60);
    limit_burst::refill(addresses[17]); assert(set_calls == 1); // No redundant setters.
    charge[0] = 0; limit_burst::refill(addresses[17]); assert(charge[0] == 300 && set_calls == 2);
    charge[0] = 0; limit_burst::refill(addresses[17] + 1); assert(charge[0] == 0);
    maximum = 0; limit_burst::refill(addresses[17]); assert(charge[0] == 0);
    maximum = 300;
    put(chunk + 13 * 0x18, std::uintptr_t{0});
    limit_burst::refill(addresses[17]); assert(charge[0] == 0);
    put(chunk + 13 * 0x18, addresses[13]);
    put(addresses[12] + 0xB0, std::uint32_t{0x400}); // Static setter flag mismatch.
    limit_burst::refill(addresses[17]); assert(charge[0] == 0);
    put(addresses[12] + 0xB0, std::uint32_t{0x2400});
    limit_burst::refill(addresses[17]); assert(charge[0] == 300);
    VirtualFree(reinterpret_cast<void*>(image), 0, MEM_RELEASE);
    std::puts("Limit Burst native API checks passed (synthetic reflection and battle units)");
}
