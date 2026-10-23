#include "../src/plugin.cpp"
#include <cassert>
int main() {
    image=reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,0xA000000,MEM_RESERVE,PAGE_READWRITE));
    assert(image && VirtualAlloc(reinterpret_cast<void*>((image+GObjects)&~0xFFFULL),4096,MEM_COMMIT,PAGE_READWRITE));
    alignas(8) unsigned char objects[14][0x800]{},slots[14][0x18]{};
    std::uintptr_t at[14]{},chunk=reinterpret_cast<std::uintptr_t>(slots);
    auto put=[](std::uintptr_t p,auto value) { std::memcpy(reinterpret_cast<void*>(p),&value,sizeof(value)); };
    put(image+GObjects,reinterpret_cast<std::uintptr_t>(&chunk)); put(image+GObjects+0x14,14); put(image+GObjects+0x1C,1);
    for(int i=0;i<14;++i) {
        at[i]=reinterpret_cast<std::uintptr_t>(objects[i]); Object o{}; o.index=i;
        put(at[i],o); put(chunk+i*0x18,at[i]);
    }
    put(at[7]+0x18,std::uint64_t{1}); put(at[8]+0x18,std::uint64_t{2}); put(at[9]+0x18,std::uint64_t{3});
    cutscene::object_name=[](std::uint64_t name) {
        return name==1 ? std::wstring(L"CPP_BtlUnitParameter") : name==2 ? std::wstring(L"CPP_VM_BtlUnitParam") :
            name==3 ? std::wstring(L"CPP_VM_UnitParamPoint") : name==4 ? std::wstring(L"CPP_UI_BattleSubsystem") : name==5 ? std::wstring(L"CPP_UI_BtlUnitManager") : std::wstring(L"None");
    };
    screen_guard::object_name=cutscene::object_name;
    put(at[1]+0xC0,at[0]); put(at[2]+0x20,at[1]);
    put(at[3]+0x10,at[7]); put(at[3]+0x360,at[2]);
    put(at[3]+0x10C,int{10}); put(at[3]+0x268,int{5}); put(at[3]+0x110,int{80});
    put(at[4]+0x10,at[8]); put(at[4]+0x1A0,at[3]); put(at[4]+0x90,at[5]); put(at[4]+0x98,at[6]);
    put(at[5]+0x10,at[9]); put(at[6]+0x10,at[9]);
    put(at[5]+0xD8,(unsigned char)1); put(at[6]+0xD8,(unsigned char)2);
    put(at[5]+0xBC,int{500}); put(at[6]+0xBC,int{60});
    put(at[10]+0x18,std::uint64_t{4}); put(at[12]+0x18,std::uint64_t{5});
    put(at[11]+0x10,at[10]); put(at[11]+0x20,at[0]); put(at[11]+0x78,at[13]);
    put(at[13]+0x10,at[12]); put(at[13]+0x20,at[11]);
    put(at[4]+0x198,int{77});
    std::uintptr_t map_entry[3]{77,at[4],0};
    put(at[13]+0x28,reinterpret_cast<std::uintptr_t>(map_entry)); put(at[13]+0x30,int{1});
    put(at[13]+0x34,int{1}); put(at[13]+0x38,std::uint32_t{1}); put(at[13]+0x50,int{1});
    screen_guard::blocked(14); // Same worker discovery used in the actual plugin.
    battle_resources::Points points;
    assert(battle_resources::native_points(at[0],at[3],points));
    assert(points.hp==15 && points.hp_max==500 && points.mp==60 && points.mp_max==60);
    put(at[5]+0xBC,int{700}); put(at[3]+0x110,int{2});
    assert(battle_resources::native_points(at[0],at[3],points) && points.hp_max==700 && points.mp==2);
    assert(!battle_resources::native_points(at[0]+1,at[3],points));
    put(chunk+5*0x18,std::uintptr_t{0}); assert(!battle_resources::native_points(at[0],at[3],points));
    put(chunk+5*0x18,at[5]); put(at[5]+0xD8,(unsigned char)2);
    assert(!battle_resources::native_points(at[0],at[3],points)); // Wrong resource type.
    put(at[5]+0xD8,(unsigned char)1); put(at[4]+0x1A0,at[1]);
    assert(!battle_resources::native_points(at[0],at[3],points)); // Foreign parameter/model pair.
    put(at[4]+0x1A0,at[3]); put(at[5]+0xBC,int{100000001});
    assert(!battle_resources::native_points(at[0],at[3],points));
    put(at[5]+0xBC,int{500});
    put(chunk+4*0x18,std::uintptr_t{0}); screen_guard::blocked(0);
    put(chunk+4*0x18,at[4]); screen_guard::blocked(0);
    assert(battle_resources::native_points(at[0],at[3],points)); // Reused model, no registry scan.
    put(at[13]+0x38,std::uint32_t{0}); screen_guard::blocked(0);
    assert(!battle_resources::native_points(at[0],at[3],points)); // Sparse map hole is not a live entry.
    std::uint32_t heap_flags=1;
    put(at[13]+0x48,reinterpret_cast<std::uintptr_t>(&heap_flags)); screen_guard::blocked(0);
    assert(battle_resources::native_points(at[0],at[3],points)); // Secondary allocation flag storage.
    heap_flags=0; screen_guard::blocked(0);
    assert(!battle_resources::native_points(at[0],at[3],points));
    VirtualFree(reinterpret_cast<void*>(image),0,MEM_RELEASE);
    std::puts("Cached battle resources passed (current caps, HP bonus, MP clamp, stale objects and ownership)");
}
