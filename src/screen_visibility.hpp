#pragma once
#include "game_function.hpp"
#include "battle_discovery.hpp"
#include <vector>

// Read-only screen discovery runs only on the plugin worker. Never call game
// functions or share the game-thread FName cache from this code.
namespace screen_guard {
enum class Kind { Other, Menu, Results, TitleTask, TaskManager, BattleInterface, BattleView, ModelCollection, BattleResources, BattleUnitManager, HudRoot, HudFunction, WidgetClass };
static int cursor=0, known_count=0, appended=-1;
static bool scanned=false;
static bool title_active=false;
static std::vector<std::uintptr_t> candidates;
static std::atomic<bool> cached_blocked{true};
static std::wstring read_name(std::uint64_t value) {
    auto key=static_cast<std::uint32_t>(value);
    std::uintptr_t block=0;
    std::uint16_t header=0;
    if (!read(image+0x9CA1280+0x10+(key>>16)*8,block) ||
        !read(block+(key&65535)*2,header)) return {};
    auto length=header>>6;
    // Native class names used here are ASCII. Unicode subclasses still walk
    // to their native base; no engine string allocator is used on this thread.
    if ((header&1) || length<1 || length>128) return {};
    char bytes[128]{};
    SIZE_T count=0;
    if (!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(block+(key&65535)*2+2),
        bytes,length,&count) || count!=static_cast<SIZE_T>(length)) return {};
    return std::wstring(bytes,bytes+length);
}
static std::wstring (*object_name)(std::uint64_t)=read_name;

static Kind classify(std::uintptr_t pointer) {
    Object object{};
    std::uint32_t serial=0;
    if (!live(pointer,object,serial) || (object.flags&0x30)) return Kind::Other;
    auto klass=object.klass;
    for (int depth=0;klass && depth<32;++depth) {
        Object cls{};
        if (!live(klass,cls,serial,true)) break;
        auto name=object_name(cls.name);
        if (name==L"WBP_OverAllLayout_C") return Kind::HudRoot;
        if(name==L"Class" && (object_name(object.name)==L"Image" || object_name(object.name)==L"CanvasPanel"))
            return Kind::WidgetClass;
        if (name==L"Function") {
            Object owner{}; std::uint32_t flags=0;
            auto fn=object_name(object.name);
            if(read(object.outer,owner) && read(pointer+0xB0,flags) && (flags&0x400)) {
                auto owner_name=object_name(owner.name);
                for(const auto& api:battle_discovery::widget_apis)
                    if(fn==api.name && owner_name==api.owner && bool(flags&0x2000)==api.is_static)
                        return Kind::HudFunction;
            }
        }
        if (name==L"CPP_WBP_MainMenuLayout" || name==L"CPP_WBP_TalkEvent_Pause" ||
            name==L"CPP_WBP_TalkEvent_SystemMenu") return Kind::Menu;
        if (name==L"CPP_WBP_Btl_ResultLayout") return Kind::Results;
        if (name==L"CPP_TitleTaskMain" || name==L"CPP_TitleTaskStartup" ||
            name==L"CPP_BootTaskNotify" || name==L"CPP_BootTaskStartup") return Kind::TitleTask;
        if (name==L"CPP_TaskManager") return Kind::TaskManager;
        if (name==L"CPP_UI_BattleSubsystem") return Kind::BattleInterface;
        if (name==L"CPP_VM_BtlCmd") return Kind::BattleView;
        if (name==L"MVVMViewModelCollectionObject") return Kind::ModelCollection;
        if (name==L"CPP_VM_BtlUnitParam") return Kind::BattleResources;
        if (name==L"CPP_UI_BtlUnitManager") return Kind::BattleUnitManager;
        if (!read(klass+0x40,klass)) break;
    }
    return Kind::Other;
}
static void add_view(battle_discovery::Snapshot& out,std::uintptr_t view) {
    for(std::size_t i=0;i<out.view_count;++i) if(out.views[i]==view) return;
    if(out.view_count==out.views.size()) {
        for(std::size_t i=1;i<out.view_count;++i) out.views[i-1]=out.views[i];
        --out.view_count;
    }
    out.views[out.view_count++]=view;
}
static void add_resource(battle_discovery::Snapshot& out,std::uintptr_t view) {
    for(std::size_t i=0;i<out.resource_count;++i) if(out.resources[i]==view) return;
    if(out.resource_count==out.resources.size()) {
        for(std::size_t i=1;i<out.resource_count;++i) out.resources[i-1]=out.resources[i];
        --out.resource_count;
    }
    out.resources[out.resource_count++]=view;
}
static void collect_units(std::uintptr_t manager,battle_discovery::Snapshot& out) {
    // Pinned UE TMap<int, UObject*>: 24-byte sparse entries, allocation flags
    // use four inline words or the secondary allocation. Never read map holes.
    auto map=manager+0x28;
    std::uintptr_t data=0,flags=0;
    int slots=0,bits=0;
    if(!read(map,data) || !read(map+8,slots) || slots<0 || slots>256 ||
        !read(map+0x28,bits) || bits<slots || bits>4096 || !read(map+0x20,flags)) return;
    if(!flags) flags=map+0x10;
    for(int i=0;i<slots;++i) {
        std::uint32_t allocated=0;
        std::uintptr_t view=0;
        int key=-1,unique=-1;
        if(!read(flags+(i/32)*4,allocated) || !(allocated&(1u<<(i%32))) ||
            !read(data+i*0x18,key) || !read(data+i*0x18+8,view) ||
            classify(view)!=Kind::BattleResources || !read(view+0x198,unique) || unique!=key) continue;
        add_resource(out,view);
    }
}
static void collect_models(std::uintptr_t collection,battle_discovery::Snapshot& out) {
    struct Array { std::uintptr_t data; int count,capacity; } list{};
    if(!read(collection+0x28,list) || list.count<0 || list.count>1024 || list.capacity<list.count) return;
    struct Entry { std::uintptr_t klass; std::uint64_t name; std::uintptr_t instance; };
    static_assert(sizeof(Entry)==0x18);
    std::array<Entry,1024> entries{};
    SIZE_T bytes=0;
    auto size=static_cast<SIZE_T>(list.count)*sizeof(Entry);
    if(size && (!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(list.data),entries.data(),size,&bytes) || bytes!=size)) return;
    for(int i=0;i<list.count;++i) {
        auto kind=classify(entries[i].instance);
        if(kind==Kind::BattleView) add_view(out,entries[i].instance);
        else if(kind==Kind::BattleResources) add_resource(out,entries[i].instance);
    }
}
static bool blocked(int budget=2048) {
    title_active=false;
    int count=0;
    std::uintptr_t chunks=0;
    if (!read(image+GObjects+0x14,count) || count<=0 || count>4000000 || !read(image+GObjects,chunks)) return true;
    if (count<known_count) { cursor=0; appended=-1; scanned=false; candidates.clear(); }
    if (known_count>0 && count>known_count && appended<0) appended=known_count;
    known_count=count;
    for (int n=0;n<budget;++n) {
        int index;
        if (appended>=0 && appended<count) { index=appended++; if(appended>=count) appended=-1; }
        else { if(cursor>=count) { cursor=0; scanned=true; } index=cursor++; }
        std::uintptr_t chunk=0,pointer=0;
        if (!read(chunks+(index/65536)*8,chunk) || !read(chunk+(index%65536)*0x18,pointer) ||
            !pointer) continue;
        auto kind=classify(pointer);
        if(kind==Kind::Other || kind==Kind::TitleTask) continue;
        bool exists=false;
        for(auto cached:candidates) exists|=cached==pointer;
        if (!exists) candidates.push_back(pointer);
    }
    if (cursor>=count) scanned=true;
    bool hidden=false;
    battle_discovery::Snapshot battle_objects;
    std::uintptr_t current_world=0;
    read(image+GWorld,current_world);
    for(auto it=candidates.begin();it!=candidates.end();) {
        auto kind=classify(*it);
        if(kind==Kind::Other) { it=candidates.erase(it); continue; }
        if(kind==Kind::HudRoot) {
            if(battle_objects.hud_root_count<battle_objects.hud_roots.size())
                battle_objects.hud_roots[battle_objects.hud_root_count++]=*it;
            // CommonUI system/save/modal stacks contain the pause/options UI.
            // The displayed child remains allocated after closing, so check
            // its active flag instead of testing only for a non-null pointer.
            for(int offset:{0x5C8,0x5D0,0x5D8,0x5E0,0x5E8}) {
                std::uintptr_t stack=0,child=0; Object object{}; std::uint32_t serial=0;
                unsigned char active=0;
                if(read(*it+offset,stack) && live(stack,object,serial) &&
                    read(stack+0x1A0,child) && live(child,object,serial) &&
                    read(child+0x3E0,active) && active==1) hidden=true;
            }
        } else if(kind==Kind::HudFunction) {
            Object fn{},owner{};
            if(read(*it,fn) && read(fn.outer,owner)) {
                auto name=object_name(fn.name),owner_name=object_name(owner.name);
                for(std::size_t i=0;i<battle_discovery::widget_api_count;++i) {
                    const auto& api=battle_discovery::widget_apis[i];
                    if(name!=api.name || owner_name!=api.owner) continue;
                    std::uintptr_t defaults=0;
                    if(!api.is_static || (read(fn.outer+0x110,defaults) && defaults))
                        battle_objects.hud_functions[i]={*it,fn.outer,fn.name,defaults};
                }
            }
        } else if(kind==Kind::WidgetClass) {
            Object cls{};
            if(read(*it,cls)) battle_objects.widget_classes[object_name(cls.name)==L"Image" ? 0 : 1]=*it;
        } else if(kind==Kind::BattleInterface) {
            Object object{};
            if(read(*it,object) && (!current_world || object.outer==current_world) &&
                battle_objects.interface_count<battle_objects.interfaces.size())
                battle_objects.interfaces[battle_objects.interface_count++]=*it;
            std::uintptr_t unit_manager=0;
            if(read(*it+0x78,unit_manager) && classify(unit_manager)==Kind::BattleUnitManager)
                collect_units(unit_manager,battle_objects);
        } else if(kind==Kind::BattleView) {
            add_view(battle_objects,*it);
        } else if(kind==Kind::BattleResources) {
            add_resource(battle_objects,*it);
        } else if(kind==Kind::BattleUnitManager) {
            collect_units(*it,battle_objects);
        } else if(kind==Kind::ModelCollection) {
            collect_models(*it,battle_objects);
        } else if(kind==Kind::TaskManager) {
            // The actual title layout is nested in WidgetTree, not a viewport
            // root. Inspect only this game instance's currently selected task.
            std::uintptr_t world=0,instance=0,task=0;
            Object world_object{},instance_object{},manager_object{},task_object{};
            std::uint32_t serial=0;
            if (read(image+GWorld,world) && live(world,world_object,serial) &&
                read(world+0x228,instance) && live(instance,instance_object,serial) &&
                live(*it,manager_object,serial) && manager_object.outer==instance &&
                read(*it+0x38,task) && classify(task)==Kind::TitleTask &&
                live(task,task_object,serial) && task_object.outer==*it) { hidden=true; title_active=true; }
        } else {
            // CommonActivatableWidget.bIsActive in the pinned game build.
            unsigned char active=0;
            if(read(*it+0x3E0,active) && active==1) hidden=true;
        }
        ++it;
    }
    battle_discovery::publish(battle_objects);
    return hidden || !scanned;
}
}
static std::atomic<bool> screen_hides_indicator{true};
static void refresh_indicator_screen() {
    bool hidden=screen_guard::blocked();
    int state=screen_guard::title_active ? 2 : hidden ? 1 : 0;
    static int reported=-1;
    if(state!=reported) {
        reported=state;
        log(state==2 ? "Title/startup task active; booster icons hidden." : state==1 ?
            "Menu/results screen or initial discovery; booster icons hidden." : "Gameplay screen; booster icons allowed.");
    }
    screen_guard::cached_blocked.store(hidden);
}
// The game-thread callback reads a snapshot, with no registry scan or lock.
static bool native_indicator_screen() { return screen_guard::cached_blocked.load(); }
static bool (*query_indicator_screen)()=native_indicator_screen;
