#include "../src/plugin.cpp"
#include <cassert>
#include <unordered_map>
static std::array<std::uintptr_t,64> at;
static int next_object=10,imports=0,calls=0;
static std::uintptr_t registry=0;
static bool fail=false,fail_geometry=false;
static int removals=0;
static std::unordered_map<std::uintptr_t,unsigned char> visibility;
static std::unordered_map<std::uintptr_t,native_indicator::Vec> positions,sizes;
static bool fake_call(int api,std::uintptr_t object,void* args) {
    using namespace native_indicator;
    ++calls;
    if(fail) return false;
    switch(api) {
    case Spawn: static_cast<SpawnArgs*>(args)->result=at[next_object++]; return true;
    case Import: {
        auto& a=*static_cast<ImportArgs*>(args);
        assert(a.world==at[1] && a.count>9000 && a.capacity==a.count);
        assert(std::memcmp(reinterpret_cast<void*>(a.data),"\x89PNG\r\n\x1a\n",8)==0);
        a.result=at[next_object++]; ++imports; return true;
    }
    case AddChild: assert(object==at[61]); [[fallthrough]];
    case AddCanvas: {
        auto& a=*static_cast<Pair*>(args);
        Object image{}; assert(read(a.value,image));
        // Unreal assigns a weak-object serial while attaching an Image.
        std::uint32_t serial=700+image.index;
        std::memcpy(reinterpret_cast<void*>(registry+image.index*0x18+0x10),&serial,4);
        a.result=at[next_object++]; return true;
    }
    case Visibility: visibility[object]=*static_cast<unsigned char*>(args); return true;
    case Dpi: return true;
    case Geometry: return !fail_geometry;
    case ToLocal: {
        auto& a=*static_cast<LocalArgs*>(args);
        assert(a.include_window);
        a.local={a.screen.x/1.5,a.screen.y/1.5}; return true;
    }
    case Position: positions[object]=*static_cast<Vec*>(args); return true;
    case Size: sizes[object]=*static_cast<Vec*>(args); return true;
    case Brush: assert(static_cast<BrushArgs*>(args)->texture); return true;
    case Remove: ++removals; return true;
    case ZOrder: case Anchors: case Offsets: return true;
    default: assert(false); return false;
    }
}
int main() {
    image=reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,0xA000000,MEM_RESERVE,PAGE_READWRITE));
    assert(image && VirtualAlloc(reinterpret_cast<void*>((image+GObjects)&~0xFFFULL),4096,MEM_COMMIT,PAGE_READWRITE));
    alignas(8) unsigned char objects[64][0x620]{},entries[64][0x18]{};
    auto chunk=reinterpret_cast<std::uintptr_t>(entries); registry=chunk;
    auto put=[](std::uintptr_t p,auto value) { std::memcpy(reinterpret_cast<void*>(p),&value,sizeof(value)); };
    put(image+GObjects,reinterpret_cast<std::uintptr_t>(&chunk)); put(image+GObjects+0x14,64); put(image+GObjects+0x1C,1);
    for(int i=0;i<64;++i) {
        at[i]=reinterpret_cast<std::uintptr_t>(objects[i]); Object obj{}; obj.index=i;
        put(at[i],obj); put(chunk+i*0x18,at[i]);
    }
    put(at[1]+0x2D8,at[2]); put(at[2]+0x30,at[3]);
    // Real game layout: ScaleBox (single child) -> Overlay (multi-child).
    put(at[3]+0x10,at[6]); put(at[6]+0x18,std::uint64_t{100});
    put(at[6]+0x40,at[7]); put(at[7]+0x18,std::uint64_t{101});
    put(at[7]+0x40,at[8]); put(at[8]+0x18,std::uint64_t{102});
    put(at[61]+0x10,at[9]); put(at[9]+0x18,std::uint64_t{103}); put(at[9]+0x40,at[8]);
    auto child_slot=at[60]; put(at[3]+0x168,reinterpret_cast<std::uintptr_t>(&child_slot));
    put(at[3]+0x170,1); put(at[3]+0x174,1); put(at[60]+0x28,at[3]); put(at[60]+0x30,at[61]);
    cutscene::object_name=[](std::uint64_t n) {
        return std::wstring(n==100 ? L"ScaleBox" : n==101 ? L"ContentWidget" : n==102 ? L"PanelWidget" : n==103 ? L"Overlay" : L"None");
    };
    assert(native_indicator::attachment_panel(at[3])==at[61]);
    put(at[3]+0x170,0); assert(!native_indicator::attachment_panel(at[3]));
    put(at[3]+0x170,1);
    using namespace native_indicator;
    ready=[] { return false; }; call=fake_call;
    assert(!update(at[1],{4,2,true,true},true,1920,1080,{20,240}));
    assert(calls==0); // Missing metadata never creates half a native HUD.
    ready=[] { return true; }; image_class=at[4]; canvas_class=at[5];
    for(int frame=0;frame<6;++frame) {
        assert(!update(at[1],{4,2,true,true},true,1920,1080,{20,240}));
        assert(visibility[canvas.pointer]==1); // No partially initialized HUD.
    }
    assert(update(at[1],{4,2,true,true},true,1920,1080,{20,240}));
    assert(imports==7 && built==7 && visibility[canvas.pointer]==3);
    bool expected[]{false,true,true,false,true,true,false};
    int cell[]{0,0,1,2,2,3,3};
    for(int i=0;i<7;++i) {
        assert(visibility[pictures[i].pointer]==(expected[i] ? 3 : 1));
        assert(std::fabs(positions[slots[i].pointer].x-(22+54*cell[i])/1.5)<0.0001);
        assert(std::fabs(positions[slots[i].pointer].y-242/1.5)<0.0001);
        assert(sizes[slots[i].pointer].x==32 && sizes[slots[i].pointer].y==32);
    }
    int before=calls;
    assert(update(at[1],{4,2,true,true},true,1920,1080,{20,240}));
    assert(calls==before && imports==7); // No per-frame texture creation or UI mutation.
    assert(update(at[1],{4,2,true,2},true,1920,1080,{20,240}));
    assert(visibility[pictures[5].pointer]==1 && visibility[pictures[6].pointer]==3);
    assert(update(at[1],{4,2,true,true},false,1920,1080,{20,240}));
    for(const auto& picture:pictures) assert(visibility[picture.pointer]==1);
    assert(visibility[canvas.pointer]==1); // Pause/menu/cutscene/F5 all hide native icons.
    assert(update(at[1],{2,1,false,false},true,1920,1080,{20,240}));
    assert(visibility[pictures[0].pointer]==3 && visibility[pictures[1].pointer]==1);
    assert(visibility[pictures[3].pointer]==3 && visibility[pictures[4].pointer]==1);
    assert(imports==7); // Cycling reuses the same textures.
    assert(update(at[1],{2,1,false,false},true,3440,1440,{460,240}));
    assert(std::fabs(positions[slots[0].pointer].x-462/1.5)<0.0001);
    fail_geometry=true; last_dpi_check=0;
    assert(!update(at[1],{2,1,false,false},true,3440,1440,{460,240}));
    assert(!canvas.pointer && failed && removals==1); // Failed refresh removes the native layer before fallback.
    assert(!update(at[1],{2,1,false,false},true,3440,1440,{460,240}));
    assert(imports==7); // Do not repeatedly rebuild a failing root.
    put(chunk+1*0x18,std::uintptr_t{0});
    assert(!update(at[1],{2,1,false,false},true,3440,1440,{460,240}));
    assert(!root.pointer); // Freed root cannot receive native calls.
    VirtualFree(reinterpret_cast<void*>(image),0,MEM_RELEASE);
    std::puts("Native viewport HUD passed (creation, alpha PNG, DPI, fixed slots, hiding, texture reuse and stale roots)");
}
