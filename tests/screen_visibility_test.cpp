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

int main() {
    image=reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,0xA000000,MEM_RESERVE,PAGE_READWRITE));
    assert(image); for(auto offset:{GObjects,GWorld}) assert(VirtualAlloc(reinterpret_cast<void*>((image+offset)&~0xFFFULL),4096,MEM_COMMIT,PAGE_READWRITE));
    alignas(8) unsigned char objects[15][0x600]{},slots[15][0x18]{};
    std::uintptr_t at[15]{},chunk=reinterpret_cast<std::uintptr_t>(slots);
    auto put=[](std::uintptr_t p,auto value) { std::memcpy(reinterpret_cast<void*>(p),&value,sizeof(value)); };
    put(image+GObjects,reinterpret_cast<std::uintptr_t>(&chunk)); put(image+GObjects+0x14,14); put(image+GObjects+0x1C,1);
    for(int i=0;i<15;++i) {
        at[i]=reinterpret_cast<std::uintptr_t>(objects[i]); Object o{}; o.index=i;
        put(at[i],o); put(chunk+i*0x18,at[i]);
    }
    // Native classes, one Blueprint subclass, then live instances.
    for(int i=0;i<4;++i) put(at[i]+0x18,std::uint64_t(i+1));
    put(at[3]+0x40,at[0]);
    put(at[4]+0x10,at[3]); // Main menu Blueprint inherits native layout.
    put(at[5]+0x10,at[1]); // Results layout.
    put(at[6]+0x10,at[2]); // Title task owns a viewport widget.
    put(at[6]+0x50,std::uintptr_t{0}); // Actual title layout need not be a viewport root.
    put(at[7]+0x18,std::uint64_t{6}); // Startup task native class.
    put(at[10]+0x18,std::uint64_t{5}); // Task manager native class.
    put(at[11]+0x10,at[10]); put(at[11]+0x20,at[9]);
    put(at[11]+0x38,at[13]); // Current task is gameplay, cached title is inactive.
    put(at[6]+0x20,at[11]);
    put(image+GWorld,at[8]); put(at[8]+0x228,at[9]);
    cutscene::object_name=[](std::uint64_t n) {
        switch(n) {
        case 1:return std::wstring(L"CPP_WBP_MainMenuLayout");
        case 2:return std::wstring(L"CPP_WBP_Btl_ResultLayout");
        case 3:return std::wstring(L"CPP_TitleTaskMain");
        case 4:return std::wstring(L"WBP_Menu_C");
        case 5:return std::wstring(L"CPP_TaskManager");
        case 6:return std::wstring(L"CPP_TitleTaskStartup");
        case 7:return std::wstring(L"CPP_BootTaskNotify");
        case 8:return std::wstring(L"CPP_BootTaskStartup");
        default:return std::wstring(L"None");
        }
    };



    screen_guard::object_name=cutscene::object_name;
    assert(!screen_guard::blocked()); // Cached inactive screens must not hide gameplay.
    put(at[4]+0x3E0,(unsigned char)1);
    assert(screen_guard::blocked()); // Main menu.
    put(at[4]+0x3E0,(unsigned char)0);
    assert(!screen_guard::blocked());
    put(at[5]+0x3E0,(unsigned char)1);
    assert(screen_guard::blocked()); // Results, including subsequent pages.
    put(at[5]+0x3E0,(unsigned char)0);
    assert(!screen_guard::blocked()); // Return to gameplay.
    put(at[11]+0x38,at[6]); assert(screen_guard::blocked()); // Main title task, no viewport-root widget.
    put(at[6]+0x10,at[7]); assert(screen_guard::blocked()); // Live-observed startup task.
    for(auto task_name:{std::uint64_t{7},std::uint64_t{8}}) {
        put(at[7]+0x18,task_name); assert(screen_guard::blocked()); // Pre-title startup/notification screens.
    }
    put(at[11]+0x20,at[8]); assert(!screen_guard::blocked()); // Foreign manager rejected.
    put(at[11]+0x20,at[9]);
    put(at[11]+0x38,at[13]); assert(!screen_guard::blocked()); // Gameplay despite cached title task.
    put(at[5]+0x3E0,(unsigned char)1);
    put(chunk+5*0x18,std::uintptr_t{0});
    assert(!screen_guard::blocked()); // Freed result widget is rejected.
    put(chunk+5*0x18,at[5]); put(at[5]+8,std::uint32_t{0x10});
    assert(!screen_guard::blocked()); // CDO/archetype cannot count as an open screen.
    // Newly appended menu is found before a round-robin scan backlog.
    put(at[14]+0x10,at[3]); put(at[14]+0x3E0,(unsigned char)1);
    put(image+GObjects+0x14,15);
    assert(screen_guard::blocked(1));
    put(at[14]+0x3E0,(unsigned char)0); assert(!screen_guard::blocked());
    // Worker publishes a stable snapshot; reading it never starts discovery.
    refresh_indicator_screen(); assert(!native_indicator_screen());
    put(at[4]+0x3E0,(unsigned char)1);
    assert(!native_indicator_screen());
    refresh_indicator_screen(); assert(native_indicator_screen());
    put(at[4]+0x3E0,(unsigned char)0);
    refresh_indicator_screen(); assert(!native_indicator_screen());
    // Raw FName reads use the same pool layout verified by the live probe.
    assert(VirtualAlloc(reinterpret_cast<void*>((image+0x9CA1280)&~0xFFFULL),4096,MEM_COMMIT,PAGE_READWRITE));
    alignas(8) unsigned char names[512]{};
    auto names_at=reinterpret_cast<std::uintptr_t>(names);
    put(image+0x9CA1280+0x10,names_at);
    constexpr char label[]="CPP_TaskManager";
    put(names_at+16,static_cast<std::uint16_t>((sizeof(label)-1)<<6));
    std::memcpy(names+18,label,sizeof(label)-1);
    assert(screen_guard::read_name(8)==L"CPP_TaskManager");
    put(names_at+16,std::uint16_t{129<<6}); assert(screen_guard::read_name(8).empty());
    put(names_at+16,std::uint16_t{1}); assert(screen_guard::read_name(8).empty());
#ifdef PROFILE_SCREEN
    LARGE_INTEGER frequency{},start{},end{};
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
    measured_reads=0;
    for(int frame=0;frame<100;++frame) native_indicator_screen();
    QueryPerformanceCounter(&end);
    std::printf("Screen check: %.3f ms/callback, %llu memory reads/callback\n",
        (end.QuadPart-start.QuadPart)*1000.0/frequency.QuadPart/100.0,measured_reads/100);
    assert(measured_reads==0); // The game-thread screen snapshot must never scan game objects.
#endif
    VirtualFree(reinterpret_cast<void*>(image),0,MEM_RELEASE);
    std::puts("Screen visibility checks passed (menus, title, results, resume, stale objects and new widgets)");
}
