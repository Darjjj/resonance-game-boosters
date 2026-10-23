#include "../src/plugin.cpp"
#include <cassert>
#include <initializer_list>
static bool story_playing = false, movie_playing = false;
static std::uintptr_t story_function, movie_function, story_default, movie_default;
static void fake_process(void* self, void* fn, void* params) {
    auto function = reinterpret_cast<std::uintptr_t>(fn);
    assert(function == story_function || function == movie_function);
    assert(reinterpret_cast<std::uintptr_t>(self) ==
        (function == story_function ? story_default : movie_default));
    *static_cast<unsigned char*>(params) = function == story_function ? story_playing : movie_playing;
}
int main() {
    image = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 0xA000000, MEM_RESERVE, PAGE_READWRITE));
    assert(image && VirtualAlloc(reinterpret_cast<void*>((image + GObjects) & ~0xFFFULL),
        4096, MEM_COMMIT, PAGE_READWRITE));
    alignas(8) unsigned char objects[7][0x200]{};
    alignas(8) unsigned char slots[7][0x18]{};
    std::uintptr_t chunk = reinterpret_cast<std::uintptr_t>(slots);
    auto put = [](std::uintptr_t address, auto value) {
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
    };
    auto address = [&](int i) { return reinterpret_cast<std::uintptr_t>(objects[i]); };
    put(image + GObjects, reinterpret_cast<std::uintptr_t>(&chunk));
    put(image + GObjects + 0x14, 7);
    put(image + GObjects + 0x1C, 1);
    std::uintptr_t vtable[0x4D]{};
    vtable[0x4C] = reinterpret_cast<std::uintptr_t>(&fake_process);
    for (int i = 0; i < 7; ++i) {
        Object obj{}; obj.index = i; obj.vtable = reinterpret_cast<std::uintptr_t>(vtable);
        put(address(i), obj);
        put(chunk + i * 0x18, address(i));
    }
    // 0=function class, 1/2=story/movie owners, 3/4=functions, 5/6=defaults.
    put(address(0) + 0x18, std::uint64_t{1});
    put(address(1) + 0x18, std::uint64_t{3});
    put(address(2) + 0x18, std::uint64_t{5});
    for (int i : {3, 4}) {
        put(address(i) + 0x10, address(0));
        put(address(i) + 0x18, std::uint64_t(i == 3 ? 2 : 4));
        put(address(i) + 0x20, address(i - 2));
        put(address(i) + 0xB0, std::uint32_t{0x2400});
        put(address(i + 2) + 8, std::uint32_t{0x10});
        put(address(i + 2) + 0x10, address(i - 2));
        put(address(i - 2) + 0x110, address(i + 2));
    }
    story_function = address(3); movie_function = address(4);
    story_default = address(5); movie_default = address(6);
    cutscene::object_name = [](std::uint64_t key) -> std::wstring {
        switch (key) {
            case 1: return L"Function";
            case 2: return L"IsPlayingEvent";
            case 3: return L"CPP_BFL_TalkEvent";
            case 4: return L"IsPlaying";
            case 5: return L"CPP_BFL_Movie";
            default: return L"None";
        }
    };
    bool active = false;
    assert(reflected_cutscene(active) && !active);
    story_playing = true;
    assert(reflected_cutscene(active) && active);
    story_playing = false; movie_playing = true;
    assert(reflected_cutscene(active) && active);
    movie_playing = false;
    assert(reflected_cutscene(active) && !active);
    put(chunk + 3 * 0x18, std::uintptr_t{0});
    assert(!reflected_cutscene(active)); // No call through a stale UFunction.
    VirtualFree(reinterpret_cast<void*>(image), 0, MEM_RELEASE);
    std::puts("Cutscene reflection tests passed (synthetic engine queries)");
}
