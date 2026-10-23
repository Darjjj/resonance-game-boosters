#pragma once
#include "game_function.hpp"
#include "battle_discovery.hpp"
#include <vector>
#include <unordered_map>
namespace game_hud {
static std::array<game_api::Function,2> functions{{
    {L"Widget",L"SetRenderOpacity",false},
    {L"Widget",L"IsInViewport",false}
}};
static battle_discovery::Snapshot discovery;
static std::vector<std::uintptr_t> roots;
struct Owned { std::uintptr_t pointer; float original; };
static std::unordered_map<std::uintptr_t,Owned> owned;
static bool native_ready() {
    if(battle_discovery::copy(discovery)) {
        roots.assign(discovery.hud_roots.begin(),discovery.hud_roots.begin()+discovery.hud_root_count);
        for(std::size_t i=0;i<functions.size();++i) {
            const auto& fn=discovery.hud_functions[i];
            functions[i].query={fn.function,0,fn.owner,fn.name};
        }
    }
    bool ready=true;
    for(const auto& fn:functions) ready &= fn.query.function!=0;
    static bool reported=false;
    if (ready && !reported) { reported=true; log("HUD Hide/Show: native Widget APIs resolved."); }
    return ready;
}
static bool (*ready)()=native_ready;
static bool native_invoke(int index,std::uintptr_t object,void* args) { return functions[index].call(object,args); }
static bool (*invoke)(int,std::uintptr_t,void*)=native_invoke;
static bool identity(std::uintptr_t pointer,std::uintptr_t& id) {
    Object object{};
    std::uint32_t serial=0;
    if (!live(pointer,object,serial) || (object.flags&0x30)) return false;
    id=(std::uintptr_t(serial)<<32)|std::uint32_t(object.index); return true;
}
static bool active_root(std::uintptr_t root) {
    Object object{},klass{};
    std::uint32_t serial=0;
    unsigned char viewport=0;
    return live(root,object,serial) && !(object.flags&0x30) && read(object.klass,klass) &&
        cutscene::object_name(klass.name)==L"WBP_OverAllLayout_C" && invoke(1,root,&viewport) && viewport==1;
}
static bool update(bool visible) {
    // Consume the already prepared worker snapshot even while the HUD is on.
    // No game-thread registry scan or first-keypress metadata discovery.
    bool prepared=ready();
    if (visible && owned.empty()) return true;
    if (!prepared) return false;
    bool previously_hidden=!owned.empty();
    for (auto it=owned.begin();it!=owned.end();) {
        std::uintptr_t id=0;
        float current=0;
        if (!identity(it->second.pointer,id) || id!=it->first || !read(it->second.pointer+0xE0,current)) {
            it=owned.erase(it); continue;
        }
        if (!visible) { ++it; continue; }
        if (current==0 && !invoke(0,it->second.pointer,&it->second.original)) { ++it; continue; }
        it=owned.erase(it);
    }
    if (visible) { if (previously_hidden && owned.empty()) log("HUD Hide/Show: game HUD opacity restored."); return owned.empty(); }
    bool found=false, complete=true;
    // Hiding the overall root covers every child stack together: gameplay,
    // battle, system/modal menus, guides, event UI and chat. Input is unchanged.
    for (auto root:roots) {
        if (!active_root(root)) continue;
        std::uintptr_t id=0;
        float opacity=0;
        if (!identity(root,id) || !read(root+0xE0,opacity) ||
            !std::isfinite(opacity) || opacity<0 || opacity>1) { complete=false; continue; }
        found=true;
        if(opacity==0) continue;
        float zero=0;
        if (invoke(0,root,&zero)) owned[id]={root,opacity};
        else complete=false;
    }
    if (!previously_hidden && !owned.empty()) log("HUD Hide/Show: entire game HUD hidden.");
    return found && complete;
}
}
static bool (*update_game_hud)(bool)=game_hud::update;
