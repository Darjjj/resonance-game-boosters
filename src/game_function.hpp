#pragma once
#include <array>
namespace game_api {
struct Function {
    const wchar_t* owner_name;
    const wchar_t* function_name;
    bool is_static;
    cutscene::Query query{};
    bool native=true;
    bool call(std::uintptr_t target, void* parameters) const {
        Object fn{}, object{};
        std::uint32_t serial = 0, flags = 0;
        std::uintptr_t process = 0;
        if (!live(query.function, fn, serial) || fn.outer != query.owner || fn.name != query.function_name ||
            !read(query.function + 0xB0, flags) || bool(flags & 0x400)!=native ||
            (!native && !(flags & 0x800)) ||
            bool(flags & 0x2000) != is_static) return false;
        if (is_static) target = query.defaults;
        if (!live(target, object, serial, is_static) || bool(object.flags & 0x10) != is_static) return false;
        auto klass = object.klass;
        for (int depth = 0; klass && klass != query.owner && depth < 32; ++depth) {
            Object cls{};
            if (!live(klass, cls, serial, true) || !read(klass + 0x40, klass)) return false;
        }
        if (klass != query.owner || !read(object.vtable + 0x4C * 8, process)) return false;
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(process), &info, sizeof(info)) ||
            !(info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            return false;
        using Process = void(*)(void*, void*, void*);
        reinterpret_cast<Process>(process)(reinterpret_cast<void*>(target),
            reinterpret_cast<void*>(query.function), parameters);
        return true;
    }
};

template<std::size_t N> bool discover(std::array<Function,N>& functions, int& cursor) {
    bool complete = true;
    for (const auto& fn : functions) complete &= fn.query.function != 0;
    if (complete) return true;
    std::int32_t count = 0;
    std::uintptr_t chunks = 0;
    if (!read(image + GObjects + 0x14, count) || count <= 0 || count > 4000000 ||
        !read(image + GObjects, chunks)) return false;
    for (int scanned = 0; scanned < 2048 && cursor < count; ++scanned, ++cursor) {
        std::uintptr_t chunk = 0, ptr = 0;
        Object object{}, klass{}, owner{};
        if (!read(chunks + (cursor / 65536) * 8, chunk) ||
            !read(chunk + (cursor % 65536) * 0x18, ptr) || !read(ptr, object) ||
            object.index != cursor || (object.flags & 0x18010) || !read(object.klass, klass) ||
            cutscene::object_name(klass.name) != L"Function" || !read(object.outer, owner)) continue;
        auto function_name = cutscene::object_name(object.name);
        for (auto& fn : functions) {
            if (function_name != fn.function_name || cutscene::object_name(owner.name) != fn.owner_name) continue;
            std::uintptr_t defaults = 0;
            if (!fn.is_static || (read(object.outer + 0x110, defaults) && defaults))
                fn.query = {ptr, defaults, object.outer, object.name};
        }
    }
    if (cursor >= count) cursor = 0;
    for (const auto& fn : functions) if (!fn.query.function) return false;
    return true;
}
}
