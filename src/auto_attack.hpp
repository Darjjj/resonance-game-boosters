#pragma once
#include "game_function.hpp"
#include "battle_discovery.hpp"
#include <vector>
namespace auto_attack {
struct ActionKey { unsigned char category = 1, padding[3]{}; int id = -1; };
template<class T> struct Array { T* data = nullptr; int count = 0, capacity = 0; };
struct UnitArgs { int id; unsigned char alive = 1, padding[3]{}; std::uintptr_t result = 0; };
struct CanArgs { ActionKey key; unsigned char result = 0, padding[3]{}; };
struct TargetArgs { int unit, skill, result = -1; };
struct DecideTargetArgs { Array<int> ids; unsigned char all = 0, padding[7]{}; };
struct EnabledArgs { int unit, skill; unsigned char result = 0, padding[3]{}; };
struct PausedArgs { std::uintptr_t world; unsigned char result = 1, padding[7]{}; };
static_assert(sizeof(ActionKey)==8 && sizeof(UnitArgs)==16 && sizeof(CanArgs)==12);
static_assert(sizeof(DecideTargetArgs)==24 && sizeof(TargetArgs)==12 && sizeof(PausedArgs)==16);
static std::array<game_api::Function,11> functions{{
    {L"CPP_BFL_Battle",L"GetBattleUnitByUniqueId",true},
    {L"CPP_BtlUnit",L"GetUnitParameter",false},
    {L"CPP_UnitParameter",L"GetRelation",false},
    {L"CPP_BtlUnit",L"GetUnitAI",false},
    {L"CPP_BtlUnitAI",L"GetFightSkillId",false},
    {L"CPP_VM_BtlCmd",L"CanActivateAction",false},
    {L"CPP_VM_BtlCmd",L"DecideActionKey",false},
    {L"CPP_BFL_Battle",L"GetDefaultSkillTargetUnitUniqueId",true},
    {L"CPP_VM_BtlCmd",L"DecideTarget",false},
    {L"CPP_BFL_Battle",L"IsEnableCommand",true},
    {L"GameplayStatics",L"IsGamePaused",true}
}};
static int function_cursor = 0;
static std::uintptr_t ui = 0, scan_world = 0, active_view = 0;
static std::vector<std::uintptr_t> views;
static bool native_ready() { return game_api::discover(functions,function_cursor); }
static bool (*ready)() = native_ready;
static bool native_invoke(int index, std::uintptr_t target, void* args) { return functions[index].call(target,args); }
static bool (*invoke)(int,std::uintptr_t,void*) = native_invoke;
struct Context {
    std::uintptr_t manager = 0, view = 0, manager_identity = 0, view_identity = 0;
    int unit = -1, index = -1, state = -1;
    bool visible = false;
};
static battle_discovery::Snapshot discovered;
static void consume_discovery(std::uintptr_t world) {
    bool changed_world=scan_world!=world;
    if(changed_world) { scan_world=world; ui=0; active_view=0; views.clear(); }
    if(!battle_discovery::copy(discovered) && !changed_world) return;
    ui=0;
    for(std::size_t i=0;i<discovered.interface_count;++i) {
        Object object{}; std::uint32_t serial=0;
        if(live(discovered.interfaces[i],object,serial) && object.outer==world) ui=discovered.interfaces[i];
    }
    views.assign(discovered.views.begin(),discovered.views.begin()+discovered.view_count);
}
static bool resolve_context(std::uintptr_t world, Context& out) {
    Object object{};
    std::uint32_t serial=0;
    unsigned char during=0,phase=255,type=255,state=255;
    std::uintptr_t manager=0,asset=0,list=0;
    int count=0,index=-1;
    if (!live(ui,object,serial) || object.outer!=world || !read(ui+0x80,during) || during!=1 ||
        !read(ui+0x50,phase) || phase>2 || !read(ui+0x70,manager) || !live(manager,object,serial) ||
        object.outer!=ui || !read(manager+0x30,type) || type>2 || !read(manager+0x31,state) || state>1 ||
        !read(manager+0x28,asset) || !read(manager+0x58,index) || !read(manager+0x68,count) ||
        count<0 || count>16) return false;
    if (type<2) {
        if (index<0 || index>=count || !read(manager+0x60,list) || !read(list+index*4,out.unit)) return false;
    } else {
        if (!read(manager+0x70,out.unit)) return false;
        index=0;
    }
    if (out.unit<0) return false;
    out.manager=manager; out.manager_identity=(std::uintptr_t(serial)<<32)|std::uint32_t(object.index);
    out.index=index+(int(type)<<24); out.state=state;
    int matches=0;
    for (auto view:views) {
        std::uintptr_t view_asset=0;
        unsigned char visible=0;
        if (!live(view,object,serial) || !read(view+0x68,view_asset) || view_asset!=asset ||
            !read(view+0x199,visible) || visible>1) continue;
        if ((!visible && state==0) || (state==1 && view!=active_view)) continue;
        out.view=view; out.view_identity=(std::uintptr_t(serial)<<32)|std::uint32_t(object.index);
        out.visible=visible!=0; ++matches;
    }
    if (matches==1 && state==0) active_view=out.view;
    return matches==1;
}
static bool native_context(std::uintptr_t world, Context& out) {
    consume_discovery(world);
    return resolve_context(world,out);
}
static bool (*query_context)(std::uintptr_t,Context&) = native_context;
// Read-only preparation starts in exploration rather than the first command.
// Newly appended registry entries are checked before the older scan backlog.
static void prepare(std::uintptr_t world) {
    (void)ready();
    consume_discovery(world);
}
struct Pending { std::uintptr_t world=0, manager=0, view=0; int unit=-1,index=-1,skill=-1,stage=0; };
static Pending pending;
static void reset() { pending={}; }
static bool unit(std::uintptr_t world,int id,int relation,std::uintptr_t& actor) {
    UnitArgs args{id};
    if (!invoke(0,0,&args)) return false;
    Object object{};
    std::uint32_t serial=0;
    std::uintptr_t owner_world=0,parameter=0;
    unsigned char actual=0;
    if (!live(args.result,object,serial) || !read(object.outer+0xC0,owner_world) || owner_world!=world ||
        !invoke(1,args.result,&parameter) || !invoke(2,parameter,&actual) || actual!=relation) return false;
    actor=args.result; return true;
}
static void step(std::uintptr_t world) {
    if (!ready()) return;
    PausedArgs paused{world};
    if (!invoke(10,0,&paused) || paused.result) { reset(); return; }
    Context ctx;
    if (!query_context(world,ctx)) { reset(); return; }
    bool same=pending.world==world && pending.manager==ctx.manager_identity && pending.view==ctx.view_identity &&
        pending.unit==ctx.unit && pending.index==ctx.index;
    if (!same) reset();
    std::uintptr_t actor=0;
    if (!unit(world,ctx.unit,1,actor)) { reset(); return; }
    if (!pending.stage) {
        if (ctx.state!=0 || !ctx.visible) return;
        std::uintptr_t ai=0,owner=0;
        int skill=-1;
        if (!invoke(3,actor,&ai) || !read(ai+0x28,owner) || owner!=actor || !invoke(4,ai,&skill) || skill<=0) return;
        CanArgs can{{1,{0,0,0},skill}};
        EnabledArgs enabled{ctx.unit,skill};
        if (!invoke(5,ctx.view,&can) || !can.result || !invoke(9,0,&enabled) || !enabled.result) return;
        ActionKey key{1,{0,0,0},skill};
        if (!invoke(6,ctx.view,&key)) return;
        pending={world,ctx.manager_identity,ctx.view_identity,ctx.unit,ctx.index,skill,1};
        log("Auto-Battle selected the current friendly character's basic attack.");
    } else if (pending.stage==1 && ctx.state==1) {
        TargetArgs target{ctx.unit,pending.skill};
        std::uintptr_t enemy=0;
        if (!invoke(7,0,&target) || target.result<0 || !unit(world,target.result,2,enemy)) return;
        DecideTargetArgs args{{&target.result,1,1}};
        if (invoke(8,ctx.view,&args)) {
            pending.stage=2;
            log("Auto-Battle confirmed one legal enemy target.");
        }
    }
}
}
static void (*prepare_auto_attack)(std::uintptr_t) = auto_attack::prepare;
static void (*run_auto_attack)(std::uintptr_t) = auto_attack::step;
