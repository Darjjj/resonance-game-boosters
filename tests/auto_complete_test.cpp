#include "../src/plugin.cpp"
#include <cassert>

int main() {
    using namespace auto_complete;
    static Context context{100,200,10,20,0,1};
    static bool available=true;
    static int advances=0,changes=0;
    ready=[] { return true; };
    query_context=[](std::uintptr_t,Context& out) { out=context; return available; };
    invoke=[](int index,std::uintptr_t target,void* args) {
        if(index==0) { assert(target==200); ++advances; *static_cast<unsigned char*>(args)=2; }
        else { assert(target==100 && *static_cast<unsigned char*>(args)==2); ++changes; }
        return true;
    };
    step(1,1,1000); assert(advances==0); // Ordinary Auto-Battle leaves results alone.
    step(1,2,1000); assert(advances==1 && changes==1);
    step(1,2,1050); assert(advances==1); // Never repeat on the same displayed page.
    ++context.index; step(1,2,1100); assert(advances==1); // Pace page changes in real time.
    step(1,2,1600); assert(advances==2);
    available=false; step(1,2,2200); assert(advances==2); // Closed/menu/unsafe context.
    available=true; ++context.page_id; step(1,2,2800); assert(advances==3); // Reused address, new object.
    step(1,0,3400); assert(advances==3);
    step(1,2,4000); assert(advances==4);
    image=reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,0xA000000,MEM_RESERVE,PAGE_READWRITE));
    assert(image);
    assert(VirtualAlloc(reinterpret_cast<void*>((image+GObjects)&~0xFFFULL),4096,MEM_COMMIT,PAGE_READWRITE));
    alignas(8) unsigned char objects[10][0x700]{},slots[10][0x18]{};
    std::uintptr_t at[10]{},chunk=reinterpret_cast<std::uintptr_t>(slots);
    auto put=[](std::uintptr_t p,auto value) { std::memcpy(reinterpret_cast<void*>(p),&value,sizeof(value)); };
    put(image+GObjects,reinterpret_cast<std::uintptr_t>(&chunk));
    put(image+GObjects+0x14,10); put(image+GObjects+0x1C,1);
    for(int i=0;i<10;++i) {
        at[i]=reinterpret_cast<std::uintptr_t>(objects[i]); Object object{}; object.index=i;
        put(at[i],object); put(chunk+i*0x18,at[i]);
    }
    put(at[1]+0x18,std::uint64_t{1}); put(at[2]+0x18,std::uint64_t{2}); put(at[3]+0x18,std::uint64_t{3});
    put(at[4]+0x10,at[1]); put(at[5]+0x10,at[2]); put(at[6]+0x10,at[3]);
    cutscene::object_name=[](std::uint64_t name) {
        if(name==1) return std::wstring(L"CPP_WBP_Btl_ResultLayout");
        if(name==2) return std::wstring(L"CPP_WBP_Btl_Result_PageList");
        if(name==3) return std::wstring(L"WBP_OverAllLayout_C");
        return std::wstring(L"Other");
    };
    game_hud::invoke=[](int,std::uintptr_t,void* args) { *static_cast<unsigned char*>(args)=1; return true; };
    static bool paused=false;
    invoke=[](int index,std::uintptr_t,void* args) {
        assert(index==2); static_cast<auto_attack::PausedArgs*>(args)->result=paused; return true;
    };
    battle_discovery::Snapshot snapshot;
    snapshot.results[0]=at[4]; snapshot.result_count=1;
    snapshot.hud_roots[0]=at[6]; snapshot.hud_root_count=1; snapshot.result_blocked=false;
    battle_discovery::publish(snapshot);
    put(at[4]+0x3E0,(unsigned char)1); put(at[5]+0x3E0,(unsigned char)1);
    put(at[4]+0x598,at[5]); put(at[4]+0x5A8,(unsigned char)1);
    put(at[5]+0x568,3); put(at[5]+0x570,0); put(at[5]+0x5B8,(unsigned char)1);
    Context native;
    assert(native_context(at[0],native) && native.layout==at[4] && native.page==at[5]);
    put(at[5]+0x5B8,(unsigned char)0); assert(!native_context(at[0],native)); // Animation/tutorial not ready.
    put(at[5]+0x5B8,(unsigned char)1);
    paused=true; assert(!native_context(at[0],native)); paused=false;
    put(at[5]+0x5B9,(unsigned char)1); assert(!native_context(at[0],native)); // Completion is left to the game.
    put(at[5]+0x5B9,(unsigned char)0);
    put(at[6]+0x5D8,at[7]); put(at[7]+0x1A0,at[8]); put(at[8]+0x3E0,(unsigned char)1);
    assert(!native_context(at[0],native)); // A modal opened after the worker snapshot.
    put(at[8]+0x3E0,(unsigned char)0); assert(native_context(at[0],native));
    put(at[4]+0x3E0,(unsigned char)0); assert(!native_context(at[0],native));
    put(at[4]+0x3E0,(unsigned char)1);
    put(chunk+5*0x18,std::uintptr_t{0}); assert(!native_context(at[0],native)); // Freed result page.
    VirtualFree(reinterpret_cast<void*>(image),0,MEM_RELEASE);
    std::puts("Auto complete mode, page identity, pacing and inactive context passed");
}
