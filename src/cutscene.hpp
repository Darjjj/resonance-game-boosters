#pragma once
#include <string>
#include <unordered_map>

// Pure game queries verified in the full-game SDK:
// CPP_BFL_TalkEvent.IsPlayingEvent and CPP_BFL_Movie.IsPlaying.
// Both are native/static functions with one return byte and no arguments.
// Query via the existing ProcessEvent vtable entry; never replace it or alter
// function flags. All discovery and calls run in the window message callback.
namespace cutscene {
struct String { wchar_t* data; int count, capacity; };
static std::wstring name(std::uint64_t value) {
    static std::unordered_map<std::uint64_t, std::wstring> cache;
    auto found = cache.find(value);
    if (found != cache.end()) return found->second;
    wchar_t buffer[1024]{};
    String output{buffer, 0, 1024};
    using Append = void(*)(const std::uint64_t*, String*);
    reinterpret_cast<Append>(image + 0x13763A0)(&value, &output);
    if (output.data != buffer || output.count < 1 || output.count > 1024) return {};
    std::wstring result(buffer, static_cast<std::size_t>(output.count - 1));
    cache.emplace(value, result);
    return result;
}
struct Query {
    std::uintptr_t function = 0, defaults = 0, owner = 0;
    std::uint64_t function_name = 0;
    bool call(bool& value) const {
        Object fn{}, cdo{};
        std::uint32_t serial = 0, flags = 0;
        std::uintptr_t process = 0;
        if (!function || !live(function, fn, serial) || fn.outer != owner || fn.name != function_name ||
            !live(defaults, cdo, serial, true) || cdo.klass != owner || !(cdo.flags & 0x10) ||
            !read(function + 0xB0, flags) || (flags & 0x2400) != 0x2400 ||
            !read(cdo.vtable + 0x4C * 8, process)) return false;
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(process), &info, sizeof(info)) ||
            !(info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            return false;
        unsigned char result = 0;
        using ProcessEvent = void(*)(void*, void*, void*);
        reinterpret_cast<ProcessEvent>(process)(reinterpret_cast<void*>(defaults),
            reinterpret_cast<void*>(function), &result);
        if (result > 1) return false;
        value = result != 0;
        return true;
    }
};
static std::wstring (*object_name)(std::uint64_t) = name;
static Query story, movie;
static int cursor = 0;

static bool discover() {
    if (story.function && movie.function) return true;
    std::int32_t count = 0;
    std::uintptr_t chunks = 0;
    if (!read(image + GObjects + 0x14, count) || count <= 0 || count > 4000000 ||
        !read(image + GObjects, chunks)) return false;
    // Bounded work avoids stalling a frame during initial reflection discovery.
    for (int scanned = 0; scanned < 2048 && cursor < count; ++scanned, ++cursor) {
        std::uintptr_t chunk = 0, ptr = 0;
        Object obj{}, klass{}, owner{};
        if (!read(chunks + (cursor / 65536) * 8, chunk) ||
            !read(chunk + (cursor % 65536) * 0x18, ptr) || !read(ptr, obj) ||
            obj.index != cursor || (obj.flags & 0x18010) || !read(obj.klass, klass) ||
            object_name(klass.name) != L"Function") continue;
        auto function_name = object_name(obj.name);
        if (function_name != L"IsPlayingEvent" && function_name != L"IsPlaying") continue;
        if (!read(obj.outer, owner)) continue;
        auto owner_name = object_name(owner.name);
        Query* target = owner_name == L"CPP_BFL_TalkEvent" && function_name == L"IsPlayingEvent" ? &story :
            owner_name == L"CPP_BFL_Movie" && function_name == L"IsPlaying" ? &movie : nullptr;
        if (!target) continue;
        std::uintptr_t defaults = 0;
        if (read(obj.outer + 0x110, defaults) && defaults)
            *target = {ptr, defaults, obj.outer, obj.name};
    }
    if (cursor >= count) cursor = 0;
    return story.function && movie.function;
}
}

static bool reflected_cutscene(bool& active) {
    if (!cutscene::discover()) return false;
    bool story = false, movie = false;
    if (!cutscene::story.call(story) || !cutscene::movie.call(movie)) {
        cutscene::story = {}; cutscene::movie = {}; cutscene::cursor = 0;
        return false;
    }
    active = story || movie;
    return true;
}
