#pragma once
#include "battle_discovery.hpp"
namespace battle_resources {
struct Points { int hp=0,hp_max=0,mp=0,mp_max=0; };
static battle_discovery::Snapshot discovered;
static bool is_type(std::uintptr_t pointer,const wchar_t* expected) {
    Object object{};
    std::uint32_t serial=0;
    if(!live(pointer,object,serial) || (object.flags&0x30)) return false;
    auto klass=object.klass;
    for(int depth=0;klass && depth<32;++depth) {
        Object cls{};
        if(!live(klass,cls,serial,true)) return false;
        if(cutscene::object_name(cls.name)==expected) return true;
        if(!read(klass+0x40,klass)) return false;
    }
    return false;
}
static bool native_points(std::uintptr_t world,std::uintptr_t parameter,Points& out) {
    Object object{};
    std::uint32_t serial=0;
    std::uintptr_t actor=0,owner_world=0;
    if(!is_type(parameter,L"CPP_BtlUnitParameter") || !read(parameter+0x360,actor) ||
        !live(actor,object,serial) || !read(object.outer+0xC0,owner_world) || owner_world!=world) return false;
    battle_discovery::copy(discovered);
    for(std::size_t i=0;i<discovered.resource_count;++i) {
        auto view=discovered.resources[i];
        std::uintptr_t actual=0,hp_view=0,mp_view=0;
        if(!is_type(view,L"CPP_VM_BtlUnitParam") || !read(view+0x1A0,actual) || actual!=parameter ||
            !read(view+0x90,hp_view) || !read(view+0x98,mp_view) ||
            !is_type(hp_view,L"CPP_VM_UnitParamPoint") || !is_type(mp_view,L"CPP_VM_UnitParamPoint")) continue;
        unsigned char hp_type=0,mp_type=0;
        int base_hp=0,extra_hp=0,raw_mp=0;
        if(!read(hp_view+0xD8,hp_type) || hp_type!=1 || !read(mp_view+0xD8,mp_type) || mp_type!=2 ||
            !read(hp_view+0xBC,out.hp_max) || !read(mp_view+0xBC,out.mp_max) ||
            out.hp_max<0 || out.hp_max>100000000 || out.mp_max<0 || out.mp_max>100000000 ||
            !read(parameter+0x10C,base_hp) || !read(parameter+0x268,extra_hp) || !read(parameter+0x110,raw_mp)) continue;
        // Same current-resource arithmetic as the pinned native getters. Caps
        // come from the game's already-updated HUD models, without recomputation.
        auto hp=static_cast<long long>(base_hp)+extra_hp;
        if(hp<0 || hp>100000000 || raw_mp<0) continue;
        out.hp=static_cast<int>(hp);
        out.mp=raw_mp<out.mp_max ? raw_mp : out.mp_max;
        return true;
    }
    return false;
}
static bool (*query_points)(std::uintptr_t,std::uintptr_t,Points&)=native_points;
}
