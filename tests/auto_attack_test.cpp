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
#include <thread>
static std::uintptr_t objects_at[16]{};
static auto_attack::Context context;
static bool available = true, paused = false, legal = true;
static int selected = 0, confirmed = 0, target_id = 99;
static bool fake_call(int api, std::uintptr_t self, void* data) {
    using namespace auto_attack;
    switch (api) {
        case 0: {
            auto& args = *static_cast<UnitArgs*>(data);
            assert(args.alive);
            args.result = args.id == 42 ? objects_at[2] : args.id == 99 ? objects_at[3] : 0;
            break;
        }
        case 1: *static_cast<std::uintptr_t*>(data) = self == objects_at[2] ? objects_at[4] : objects_at[5]; break;
        case 2: *static_cast<unsigned char*>(data) = self == objects_at[4] ? 1 : 2; break;
        case 3: *static_cast<std::uintptr_t*>(data) = objects_at[6]; break;
        case 4: *static_cast<int*>(data) = 101; break;
        case 5: {
            auto& args = *static_cast<CanArgs*>(data);
            assert(args.key.category == 1 && args.key.id == 101); args.result = legal; break;
        }
        case 6: {
            auto& args = *static_cast<ActionKey*>(data);
            assert(args.category == 1 && args.id == 101);
            ++selected; context.state = 1; break;
        }
        case 7: {
            auto& args = *static_cast<TargetArgs*>(data);
            assert(args.unit == 42 && args.skill == 101); args.result = target_id; break;
        }
        case 8: {
            auto& args = *static_cast<DecideTargetArgs*>(data);
            assert(args.ids.count == 1 && *args.ids.data == 99 && !args.all);
            ++confirmed; break;
        }
        case 9: static_cast<EnabledArgs*>(data)->result = legal; break;
        case 10: static_cast<PausedArgs*>(data)->result = paused; break;
        default: assert(false);
    }
    return true;
}
int main() {
    image = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,0xA000000,MEM_RESERVE,PAGE_READWRITE));
    assert(image && VirtualAlloc(reinterpret_cast<void*>((image+GObjects)&~0xFFFULL),4096,MEM_COMMIT,PAGE_READWRITE));
    alignas(8) unsigned char objects[16][0x200]{}, slots[16][0x18]{};
    auto put = [](std::uintptr_t address, auto value) { std::memcpy(reinterpret_cast<void*>(address),&value,sizeof(value)); };
    std::uintptr_t chunk = reinterpret_cast<std::uintptr_t>(slots);
    put(image+GObjects,reinterpret_cast<std::uintptr_t>(&chunk)); put(image+GObjects+0x14,8); put(image+GObjects+0x1C,1);
    for (int i=0;i<16;++i) {
        objects_at[i]=reinterpret_cast<std::uintptr_t>(objects[i]);
        Object obj{}; obj.index=i; put(objects_at[i],obj); put(chunk+i*0x18,objects_at[i]);
    }
    auto world = objects_at[0], level = objects_at[1];
    put(level+0xC0,world);
    put(objects_at[2]+0x20,level); put(objects_at[3]+0x20,level);
    put(objects_at[6]+0x28,objects_at[2]);
    context={objects_at[7],objects_at[7],100,200,42,0,0,true};
    auto_attack::ready=[] { return true; };
    auto_attack::query_context=[](std::uintptr_t, auto_attack::Context& out) { out=context; return available; };
    auto_attack::invoke=fake_call;
    paused=true; auto_attack::step(world); assert(selected==0);
    paused=false; legal=false; auto_attack::step(world); assert(selected==0);
    legal=true; auto_attack::step(world); assert(selected==1 && confirmed==0);
    auto_attack::step(world); assert(selected==1 && confirmed==1);
    auto_attack::step(world); assert(confirmed==1); // No repeated confirmations.
    context.index=1; context.state=0; auto_attack::step(world); assert(selected==2);
    target_id=42; auto_attack::step(world); assert(confirmed==1); // Never target a friendly.
    target_id=99; auto_attack::step(world); assert(confirmed==2);
    auto_attack::reset(); context.state=0;
    put(chunk+2*0x18,std::uintptr_t{0}); auto_attack::step(world); assert(selected==2);
    put(chunk+2*0x18,objects_at[2]);
    auto_attack::step(world+1); assert(selected==2); // Wrong-world actor rejected.
    available=false; auto_attack::step(world);
    available=true; context.visible=false; context.state=0; auto_attack::step(world); assert(selected==2);
    context.visible=true; auto_attack::step(world); assert(selected==3);
    auto_attack::reset(); context.state=1; auto_attack::step(world); assert(confirmed==2); // No takeover of manual target selection.
    // The worker discovers views; game callbacks consume an atomic snapshot
    // rather than waiting for an entire registry sweep in 128-object batches.
    cutscene::object_name=[](std::uint64_t name) { return name==201 ? std::wstring(L"CPP_VM_BtlCmd") : name==202 ? std::wstring(L"CPP_UI_BattleSubsystem") : name==203 ? std::wstring(L"MVVMViewModelCollectionObject") : std::wstring(L"None"); };
    screen_guard::object_name=cutscene::object_name;
    put(objects_at[7]+0x18,std::uint64_t{201});
    put(objects_at[8]+0x10,objects_at[7]);
    auto_attack::scan_world=0;
    screen_guard::blocked(8);
    auto_attack::prepare(world);
    assert(auto_attack::views.empty());
    put(image+GObjects+0x14,9);
    screen_guard::blocked(1); // Newly appended object gets worker priority.
    auto_attack::prepare(world);
    assert(auto_attack::views.size()==1 && auto_attack::views[0]==objects_at[8]);
    // A busy publisher must never block the game thread.
    std::atomic<bool> publishing{false},release{false};
    std::thread publisher([&] {
        std::lock_guard<std::mutex> lock(battle_discovery::mutex);
        publishing=true;
        while(!release.load()) std::this_thread::yield();
    });
    while(!publishing.load()) std::this_thread::yield();
    auto_attack::prepare(world);
    assert(auto_attack::views.size()==1);
    release=true; publisher.join();
    // Native context becomes usable immediately after the worker publishes it.
    put(objects_at[9]+0x18,std::uint64_t{202});
    put(objects_at[10]+0x10,objects_at[9]); put(objects_at[10]+0x20,world);
    put(objects_at[10]+0x70,objects_at[11]); put(objects_at[10]+0x80,(unsigned char)1);
    put(objects_at[11]+0x20,objects_at[10]); put(objects_at[11]+0x28,objects_at[6]);
    int ready_unit=42;
    put(objects_at[11]+0x60,reinterpret_cast<std::uintptr_t>(&ready_unit));
    put(objects_at[11]+0x68,int{1});
    put(objects_at[8]+0x68,objects_at[6]); put(objects_at[8]+0x199,(unsigned char)1);
    put(image+GObjects+0x14,14); screen_guard::blocked(5);
    auto_attack::Context native;
    assert(auto_attack::native_context(world,native) && native.view==objects_at[8] && native.unit==42);
    put(objects_at[11]+0x31,(unsigned char)1); put(objects_at[8]+0x199,(unsigned char)0);
    assert(auto_attack::native_context(world,native)); // Target step keeps the selected view.
    assert(!auto_attack::native_context(world+1,native));
    auto_attack::prepare(world);
    put(chunk+8*0x18,std::uintptr_t{0});
    screen_guard::blocked(1);
    auto_attack::prepare(world);
    assert(auto_attack::views.empty());
    // Reused registry slots need no full scan when the global model collection changes.
    put(objects_at[14]+0x18,std::uint64_t{203}); put(objects_at[15]+0x10,objects_at[14]);
    std::uintptr_t collection_entry[3]{objects_at[7],0,objects_at[8]};
    put(objects_at[15]+0x28,reinterpret_cast<std::uintptr_t>(collection_entry));
    put(objects_at[15]+0x30,int{1}); put(objects_at[15]+0x34,int{1});
    put(image+GObjects+0x14,16); screen_guard::blocked(2);
    put(chunk+8*0x18,objects_at[8]);
    screen_guard::blocked(0); auto_attack::prepare(world);
    assert(auto_attack::views.size()==1 && auto_attack::views[0]==objects_at[8]);
    assert(selected==3 && confirmed==2);
#ifdef PROFILE_SCREEN
    LARGE_INTEGER frequency{},start{},end{};
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start); measured_reads=0;
    for(int frame=0;frame<100;++frame) auto_attack::prepare(world);
    QueryPerformanceCounter(&end);
    std::printf("Auto-Battle preparation: %.3f ms/callback, %llu memory reads/callback\n",
        (end.QuadPart-start.QuadPart)*1000.0/frequency.QuadPart/100.0,measured_reads/100);
    assert(measured_reads<=51200); // Bound discovery work across 100 game-thread callbacks.
#endif
    VirtualFree(reinterpret_cast<void*>(image),0,MEM_RELEASE);
    std::puts("Auto Attack checks passed (synthetic game APIs, legal enemy targeting and duplicate suppression)");
}
