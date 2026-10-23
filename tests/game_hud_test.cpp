#ifdef PROFILE_SCREEN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
static unsigned long long measured_reads=0;
static BOOL counted_read(HANDLE p,LPCVOID address,LPVOID output,SIZE_T size,SIZE_T* count) {
    ++measured_reads; return ReadProcessMemory(p,address,output,size,count);
}
#define ReadProcessMemory counted_read
#endif
#include "../src/plugin.cpp"
#include <cassert>
static bool viewport=true;
static bool fake_call(int api,std::uintptr_t self,void* args) {
    if (api==1) { *static_cast<unsigned char*>(args)=viewport; return true; }
    assert(api==0);
    std::memcpy(reinterpret_cast<void*>(self+0xE0),args,sizeof(float)); return true;
}
int main() {
    image=reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,0xA000000,MEM_RESERVE,PAGE_READWRITE));
    assert(image && VirtualAlloc(reinterpret_cast<void*>((image+GObjects)&~0xFFFULL),4096,MEM_COMMIT,PAGE_READWRITE));
    alignas(8) unsigned char objects[11][0x620]{},slots[11][0x18]{};
    std::uintptr_t at[11]{},chunk=reinterpret_cast<std::uintptr_t>(slots);
    auto put=[](std::uintptr_t p,auto value) { std::memcpy(reinterpret_cast<void*>(p),&value,sizeof(value)); };
    put(image+GObjects,reinterpret_cast<std::uintptr_t>(&chunk)); put(image+GObjects+0x14,11); put(image+GObjects+0x1C,1);
    for (int i=0;i<11;++i) {
        at[i]=reinterpret_cast<std::uintptr_t>(objects[i]); Object object{}; object.index=i;
        put(at[i],object); put(chunk+i*0x18,at[i]); put(at[i]+0xE0,1.0f);
    }
    put(at[0]+0x18,std::uint64_t{123}); put(at[1]+0x10,at[0]);
    put(at[7]+0x18,std::uint64_t{200}); // Actual engine owner is Widget, not UserWidget.
    put(at[8]+0x18,std::uint64_t{201});
    for (int i=9;i<11;++i) {
        put(at[i]+0x10,at[8]); put(at[i]+0x18,std::uint64_t(202+i-9));
        put(at[i]+0x20,at[7]); put(at[i]+0xB0,std::uint32_t{0x400});
    }
    cutscene::object_name=[](std::uint64_t name) {
        switch(name) {
            case 123: return std::wstring(L"WBP_OverAllLayout_C");
            case 200: return std::wstring(L"Widget");
            case 201: return std::wstring(L"Function");
            case 202: return std::wstring(L"SetRenderOpacity");
            case 203: return std::wstring(L"IsInViewport");
            case 777: return std::wstring(L"WBP_BattleSequencerLay_C");
            default: return std::wstring(L"None");
        }
    };
    screen_guard::object_name=cutscene::object_name;
    screen_guard::blocked(16); // Worker discovers metadata and layout while HUD is on.
    game_hud::update(true);
    assert(game_hud::roots.size()==1 && game_hud::roots[0]==at[1]);
    assert(game_hud::native_ready());

    int offsets[]{0x5C0,0x5F0,0x5F8,0x610};
    for (int i=0;i<4;++i) put(at[1]+offsets[i],at[i+2]);
    put(at[1]+0x5D8,at[6]); // System menus are a child of the same overall HUD.
    put(at[4]+0xE0,0.0f); // Authored hidden state must stay hidden.
    // Pause/system menus live in the overall system stack, not the main-menu class.
    put(at[6]+0x1A0,at[3]); put(at[3]+0x3E0,(unsigned char)1);
    assert(screen_guard::blocked());
    put(at[3]+0x3E0,(unsigned char)0); assert(!screen_guard::blocked());
    // The retained battle sequencer sits in SystemFront2 after combat. It is
    // gameplay/cinematic infrastructure, not an open pause menu.
    put(at[2]+0x18,std::uint64_t{777}); put(at[3]+0x10,at[2]);
    put(at[1]+0x5D8,std::uintptr_t{0}); put(at[1]+0x5C8,at[6]);
    put(at[3]+0x3E0,(unsigned char)1);
    assert(!screen_guard::blocked());
    // A real menu on another system stack must still block independently.
    put(at[1]+0x5D8,at[5]); put(at[5]+0x1A0,at[4]); put(at[4]+0x3E0,(unsigned char)1);
    assert(screen_guard::blocked());
    put(at[4]+0x3E0,(unsigned char)0); assert(!screen_guard::blocked());
    game_hud::invoke=fake_call;
#ifdef PROFILE_SCREEN
    measured_reads=0;
#endif
    assert(game_hud::update(false)); // First request applies immediately from warm discovery.
#ifdef PROFILE_SCREEN
    std::printf("First HUD hide: %llu reads, no registry scan\n",measured_reads);
    assert(measured_reads<128);
#endif
    float value=1;
    assert(read(at[1]+0xE0,value) && value==0); // Entire root, including system/modal/chat.
    for (int i=2;i<7;++i) assert(read(at[i]+0xE0,value) && value==(i==4 ? 0 : 1));
    game_hud::update(true);
    assert(read(at[1]+0xE0,value) && value==1);
    assert(read(at[4]+0xE0,value) && value==0); // Authored hidden child stays hidden.
    game_hud::update(false);
    put(at[1]+0xE0,0.25f); // Another writer wins on restore.
    game_hud::update(true);
    assert(read(at[1]+0xE0,value) && value==0.25f);
    game_hud::update(false);
    put(chunk+1*0x18+0x10,std::uint32_t{9}); put(at[1]+0xE0,0.7f);
    game_hud::update(true);
    assert(read(at[1]+0xE0,value) && value==0.7f); // Reused root is not restored.
    viewport=false; game_hud::update(false);
    assert(read(at[1]+0xE0,value) && value==0.7f);
    game_hud::ready=[] { return false; };
    assert(!game_hud::update(false)); // No partial commit when metadata is unavailable.
    game_hud::ready=game_hud::native_ready;
    assert(game_hud::update(true));
#ifdef PROFILE_SCREEN
    measured_reads=0;
    LARGE_INTEGER frequency{},start{},end{};
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
    for(int frame=0;frame<100;++frame) assert(game_hud::update(true));
    QueryPerformanceCounter(&end);
    std::printf("Visible HUD: %.3f ms/callback, %llu memory reads/callback\n",
        (end.QuadPart-start.QuadPart)*1000.0/frequency.QuadPart/100.0,measured_reads/100);
    assert(measured_reads==0);
#endif
    VirtualFree(reinterpret_cast<void*>(image),0,MEM_RELEASE);
    std::puts("Game HUD checks passed (whole-root hiding, warm discovery, exact restoration, foreign writes and object reuse)");
}
