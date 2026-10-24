#pragma once
#include "game_function.hpp"
#include "battle_discovery.hpp"

namespace auto_complete {
static std::array<game_api::Function,2> functions{{
    {L"WBP_Btl_ResultLayout_C",L"InputDecide",false,{},false},
    {L"GameplayStatics",L"IsGamePaused",true}
}};
static int cursor=0;
static battle_discovery::Snapshot discovery;
static bool native_ready() { return game_api::discover(functions,cursor); }
static bool (*ready)()=native_ready;
static bool native_invoke(int index,std::uintptr_t target,void* args) {
    std::uint16_t bytes=0;
    if(index==0 && (!read(functions[0].query.function+0xB6,bytes) || bytes!=1)) return false;
    return functions[index].call(target,args);
}
static bool (*invoke)(int,std::uintptr_t,void*)=native_invoke;
struct Context {
    std::uintptr_t layout=0,page=0,layout_id=0,page_id=0;
    int index=-1,state=-1;
};
static bool inherits(std::uintptr_t pointer,const wchar_t* expected) {
    Object object{}; std::uint32_t serial=0;
    if(!live(pointer,object,serial)) return false;
    auto klass=object.klass;
    for(int n=0;klass && n<32;++n) {
        if(!live(klass,object,serial,true)) return false;
        if(cutscene::object_name(object.name)==expected) return true;
        if(!read(klass+0x40,klass)) return false;
    }
    return false;
}
static bool native_context(std::uintptr_t world,Context& out) {
    battle_discovery::copy(discovery);
    if(discovery.result_blocked) return false;
    int matches=0;
    for(std::size_t i=0;i<discovery.result_count;++i) {
        Context ctx; ctx.layout=discovery.results[i];
        unsigned char active=0,finished=0;
        std::uintptr_t model=0,page_model=0,id=0;
        if(!game_hud::identity(ctx.layout,ctx.layout_id) ||
            !read(ctx.layout+0x3E0,active) || active!=1 ||
            !inherits(ctx.layout,L"WBP_Btl_ResultLayout_C") ||
            !read(ctx.layout+0x5C8,finished) || finished!=0 ||
            !read(ctx.layout+0x598,ctx.page) || !inherits(ctx.page,L"WBP_BtlResultPage_Layout_C") ||
            !game_hud::identity(ctx.page,ctx.page_id) ||
            !read(ctx.layout+0x5D8,model) || !game_hud::identity(model,id) ||
            !inherits(model,L"CPP_VM_BattleResult") ||
            !read(ctx.page+0x5C8,page_model) || page_model!=model ||
            !read(ctx.page+0x5D0,ctx.index) || ctx.index<0 || ctx.index>128) continue;
        out=ctx; ++matches;
    }
    if(matches!=1) return false;
    auto_attack::PausedArgs paused{world};
    if(!invoke(1,0,&paused) || paused.result) return false;
    // Recheck system/modal stacks immediately before advancing. The worker's
    // snapshot can precede a newly opened pause/options/dialog widget.
    bool root_active=false;
    for(std::size_t i=0;i<discovery.hud_root_count;++i) {
        auto root=discovery.hud_roots[i];
        if(!game_hud::active_root(root)) continue;
        root_active=true;
        for(int offset:{0x5C8,0x5D0,0x5D8,0x5E0,0x5E8}) {
            std::uintptr_t stack=0,child=0; Object object{}; std::uint32_t serial=0; unsigned char active=0;
            if(!read(root+offset,stack)) return false;
            if(!stack) continue;
            if(!live(stack,object,serial) || !read(stack+0x1A0,child)) return false;
            if(!child) continue;
            if(!live(child,object,serial) || !read(child+0x3E0,active)) return false;
            if(active && !inherits(child,L"WBP_Btl_ResultLayout_C") &&
                !inherits(child,L"WBP_BattleSequencerLay_C")) return false;
        }
    }
    if(!root_active) return false;
    return true;
}
static bool (*query_context)(std::uintptr_t,Context&)=native_context;
static ULONGLONG last_advance=0;
static void reset() { last_advance=0; }
static void prepare() { (void)ready(); }
static void step(std::uintptr_t world,int mode,ULONGLONG now) {
    if(mode!=2) { reset(); return; }
    if(!ready()) return;
    if(now-last_advance<100) return;
    Context ctx;
    if(!query_context(world,ctx)) return;
    // Use the visible Blueprint UI's ordinary input handler. It controls
    // animation readiness, count-up skips, page transitions and reward events.
    // The native base's PageList/State fields are unused by the shipped UI.
    unsigned char event=1; // EWidgetInputEventType::Pressed.
    if(!invoke(0,ctx.layout,&event)) return;
    last_advance=now;
    event=2; // Released: no held confirm can carry back into gameplay.
    invoke(0,ctx.layout,&event);
    log("Auto-Battle and Complete: confirmed the active result screen.");
}
}
static void (*prepare_auto_complete)()=auto_complete::prepare;
static void (*run_auto_complete)(std::uintptr_t,int,ULONGLONG)=auto_complete::step;
