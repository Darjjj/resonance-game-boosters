#include "../src/plugin.cpp"
#include <cassert>
static int blueprint_calls=0;
static void blueprint_event(void*,void*,void* args) {
    assert(*static_cast<unsigned char*>(args)==1); ++blueprint_calls;
}

int main() {
    using namespace auto_complete;
    static Context context{100,200,10,20,0,1};
    static bool available=true;
    static int advances=0,changes=0;
    ready=[] { return true; };
    query_context=[](std::uintptr_t,Context& out) { out=context; return available; };
    invoke=[](int index,std::uintptr_t target,void* args) {
        assert(index==0 && target==100);
        auto event=*static_cast<unsigned char*>(args);
        if(event==1) ++advances;
        else { assert(event==2); ++changes; }
        return true;
    };
    step(1,1,1000); assert(advances==0); // Ordinary Auto-Battle leaves results alone.
    step(1,2,1000); assert(advances==1 && changes==1);
    step(1,2,1050); assert(advances==1); // Never flood the input handler.
    step(1,2,1099); assert(advances==1);
    ++context.index; step(1,2,1100); assert(advances==2); // Confirm every 100 real ms.
    step(1,2,1199); assert(advances==2);
    step(1,2,1200); assert(advances==3 && changes==3); // Presses and releases remain paired.
    available=false; step(1,2,2200); assert(advances==3); // Closed/menu/unsafe context.
    available=true; ++context.page_id; step(1,2,2800); assert(advances==4); // Reused address, new object.
    step(1,0,3400); assert(advances==4);
    step(1,2,4000); assert(advances==5);
    image=reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,0xA000000,MEM_RESERVE,PAGE_READWRITE));
    assert(image);
    assert(VirtualAlloc(reinterpret_cast<void*>((image+GObjects)&~0xFFFULL),4096,MEM_COMMIT,PAGE_READWRITE));
    alignas(8) unsigned char objects[11][0x700]{},slots[11][0x18]{};
    std::uintptr_t at[11]{},chunk=reinterpret_cast<std::uintptr_t>(slots);
    auto put=[](std::uintptr_t p,auto value) { std::memcpy(reinterpret_cast<void*>(p),&value,sizeof(value)); };
    put(image+GObjects,reinterpret_cast<std::uintptr_t>(&chunk));
    put(image+GObjects+0x14,11); put(image+GObjects+0x1C,1);
    for(int i=0;i<11;++i) {
        at[i]=reinterpret_cast<std::uintptr_t>(objects[i]); Object object{}; object.index=i;
        put(at[i],object); put(chunk+i*0x18,at[i]);
    }
    put(at[1]+0x18,std::uint64_t{1}); put(at[2]+0x18,std::uint64_t{2}); put(at[3]+0x18,std::uint64_t{3});
    put(at[4]+0x10,at[1]); put(at[5]+0x10,at[2]); put(at[6]+0x10,at[3]);
    put(at[9]+0x10,at[10]); put(at[10]+0x18,std::uint64_t{4});
    cutscene::object_name=[](std::uint64_t name) {
        if(name==1) return std::wstring(L"WBP_Btl_ResultLayout_C");
        if(name==2) return std::wstring(L"WBP_BtlResultPage_Layout_C");
        if(name==3) return std::wstring(L"WBP_OverAllLayout_C");
        if(name==4) return std::wstring(L"CPP_VM_BattleResult");
        return std::wstring(L"Other");
    };
    game_hud::invoke=[](int,std::uintptr_t,void* args) { *static_cast<unsigned char*>(args)=1; return true; };
    static bool paused=false;
    invoke=[](int index,std::uintptr_t,void* args) {
        assert(index==1); static_cast<auto_attack::PausedArgs*>(args)->result=paused; return true;
    };
    battle_discovery::Snapshot snapshot;
    snapshot.results[0]=at[4]; snapshot.result_count=1;
    snapshot.hud_roots[0]=at[6]; snapshot.hud_root_count=1; snapshot.result_blocked=false;
    battle_discovery::publish(snapshot);
    put(at[4]+0x3E0,(unsigned char)1); put(at[5]+0x3E0,(unsigned char)1);
    put(at[4]+0x598,at[5]); put(at[4]+0x5A8,(unsigned char)1);
    put(at[5]+0x568,3); put(at[5]+0x570,0); put(at[5]+0x5B8,(unsigned char)1);
    put(at[4]+0x5D8,at[9]); put(at[5]+0x5C8,at[9]); put(at[5]+0x5D0,0);
    Context native;
    assert(native_context(at[0],native) && native.layout==at[4] && native.page==at[5]);
    put(at[5]+0x5C8,std::uintptr_t{0}); assert(!native_context(at[0],native)); // Uninitialized page viewmodel.
    put(at[5]+0x5C8,at[9]);
    paused=true; assert(!native_context(at[0],native)); paused=false;
    put(at[4]+0x5C8,(unsigned char)1); assert(!native_context(at[0],native)); // Completion is left to the game.
    put(at[4]+0x5C8,(unsigned char)0);
    put(at[6]+0x5D8,at[7]); put(at[7]+0x1A0,at[8]); put(at[8]+0x3E0,(unsigned char)1);
    assert(!native_context(at[0],native)); // A modal opened after the worker snapshot.
    put(at[8]+0x3E0,(unsigned char)0); assert(native_context(at[0],native));
    // Live game reproduction: the Blueprint result UI does not populate the
    // native staging fields and its nested page-list isn't marked active.
    put(at[4]+0x5A8,(unsigned char)0); put(at[5]+0x3E0,(unsigned char)0);
    put(at[5]+0x568,0); put(at[5]+0x5B8,(unsigned char)0);
    put(at[4]+0x5D8,at[9]); put(at[5]+0x5C8,at[9]); put(at[5]+0x5D0,0);
    assert(native_context(at[0],native));
    put(at[4]+0x3E0,(unsigned char)0); assert(!native_context(at[0],native));
    put(at[4]+0x3E0,(unsigned char)1);
    put(chunk+5*0x18,std::uintptr_t{0}); assert(!native_context(at[0],native)); // Freed result page.
    // Script events require explicit opt-in; existing native APIs keep their
    // native flag checks and cannot accidentally invoke a Blueprint function.
    std::uintptr_t vtable[0x4D]{};
    vtable[0x4C]=reinterpret_cast<std::uintptr_t>(&blueprint_event);
    put(at[4],reinterpret_cast<std::uintptr_t>(vtable));
    put(at[0]+0x20,at[1]); put(at[0]+0x18,std::uint64_t{99});
    put(at[0]+0xB0,std::uint32_t{0xC020800});
    game_api::Function event{L"WBP_Btl_ResultLayout_C",L"InputDecide",false,{at[0],0,at[1],99},false};
    unsigned char pressed=1;
    assert(event.call(at[4],&pressed) && blueprint_calls==1);
    event.native=true; assert(!event.call(at[4],&pressed));
    event.native=false; put(at[0]+0xB0,std::uint32_t{0}); assert(!event.call(at[4],&pressed));
    VirtualFree(reinterpret_cast<void*>(image),0,MEM_RELEASE);
    std::puts("Auto complete mode, page identity, pacing and inactive context passed");
}
