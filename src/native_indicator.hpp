#pragma once
#include "icon_png.hpp"

// All calls run on the game's existing message callback thread. The worker
// supplies metadata only; no graphics hooks or executable/vtable patches.
namespace native_indicator {
enum Api { Opacity,InViewport,Spawn,Import,AddChild,AddCanvas,Brush,Visibility,
    Position,Size,ZOrder,Remove,Dpi,Anchors,Offsets,Geometry,ToLocal };
static std::array<game_api::Function,battle_discovery::widget_api_count> functions{};
static bool initialized=false;
static battle_discovery::Snapshot metadata;
static std::uintptr_t image_class=0,canvas_class=0;
static bool prepare() {
    if(!initialized) {
        for(std::size_t i=0;i<functions.size();++i) {
            const auto& api=battle_discovery::widget_apis[i];
            functions[i]={api.owner,api.name,api.is_static};
        }
        initialized=true;
    }
    if(battle_discovery::copy(metadata)) {
        for(std::size_t i=0;i<functions.size();++i) {
            const auto& fn=metadata.hud_functions[i];
            functions[i].query={fn.function,fn.defaults,fn.owner,fn.name};
        }
        image_class=metadata.widget_classes[0]; canvas_class=metadata.widget_classes[1];
    }
    for(const auto& fn:functions) if(!fn.query.function) return false;
    return image_class && canvas_class;
}
static bool (*ready)()=prepare;
static bool native_call(int api,std::uintptr_t object,void* args) { return functions[api].call(object,args); }
static bool (*call)(int,std::uintptr_t,void*)=native_call;
struct Ref {
    std::uintptr_t pointer=0,id=0;
    bool valid() const { std::uintptr_t current=0; return pointer && game_hud::identity(pointer,current) && current==id; }
    bool set(std::uintptr_t value) {
        std::uintptr_t identity=0;
        if(!game_hud::identity(value,identity)) { pointer=0; id=0; return false; }
        pointer=value; id=identity; return true;
    }
};
struct Vec { double x=0,y=0; };
struct Pair { std::uintptr_t value=0,result=0; };
struct SpawnArgs { std::uintptr_t klass=0,outer=0,result=0; };
struct BrushArgs { std::uintptr_t texture=0; bool match=false; unsigned char pad[7]{}; };
struct ImportArgs { std::uintptr_t world=0,data=0; int count=0,capacity=0; std::uintptr_t result=0; };
static_assert(sizeof(SpawnArgs)==0x18 && sizeof(ImportArgs)==0x20 && sizeof(BrushArgs)==0x10 && sizeof(Vec)==0x10);
static Ref root,canvas,host;
static std::array<Ref,6> pictures,slots;
static int built=0,last_width=0,last_height=0;
static float last_dpi=0;
static bool failed=false;
static std::uint64_t last_state=~std::uint64_t{0},last_dpi_check=0;
static bool set_visibility(std::uintptr_t widget,bool visible) {
    unsigned char value=visible ? 3 : 1; // HitTestInvisible / Collapsed.
    return call(Visibility,widget,&value);
}
static void clear() {
    if(root.valid() && canvas.valid()) call(Remove,canvas.pointer,nullptr);
    canvas={}; root={}; host={}; pictures={}; slots={}; built=0; failed=false;
    last_state=~std::uint64_t{0}; last_width=0; last_height=0; last_dpi=0; last_dpi_check=0;
}
static bool abandon(std::uintptr_t owner) {
    clear(); root.set(owner); failed=true;
    log("Native booster HUD unavailable; window overlay retained.");
    return false;
}
static bool inherits(std::uintptr_t widget,const wchar_t* expected) {
    Object object{}; std::uint32_t serial=0;
    if(!live(widget,object,serial)) return false;
    auto klass=object.klass;
    for(int depth=0;klass && depth<32;++depth) {
        Object cls{};
        if(!live(klass,cls,serial,true)) return false;
        if(cutscene::object_name(cls.name)==expected) return true;
        if(!read(klass+0x40,klass)) return false;
    }
    return false;
}
static std::uintptr_t attachment_panel(std::uintptr_t parent) {
    // ContentWidget panels (including the game's root ScaleBox) already have
    // their only child. Walk through them without replacing any game widgets.
    for(int depth=0;parent && depth<8;++depth) {
        if(!inherits(parent,L"PanelWidget")) return 0;
        if(!inherits(parent,L"ContentWidget")) return parent;
        struct Array { std::uintptr_t data; int count,capacity; } slots{};
        std::uintptr_t slot=0,owner=0,child=0,id=0;
        if(!read(parent+0x168,slots) || slots.count!=1 || slots.capacity<1 ||
            !read(slots.data,slot) || !game_hud::identity(slot,id) ||
            !read(slot+0x28,owner) || owner!=parent || !read(slot+0x30,child) ||
            !game_hud::identity(child,id) || child==parent) return 0;
        parent=child;
    }
    return 0;
}
struct GeometryArgs { float values[14]{}; };
struct LocalArgs {
    std::uintptr_t world=0; GeometryArgs geometry{}; Vec screen{},local{};
    bool include_window=true; unsigned char pad[7]{};
};
static_assert(sizeof(GeometryArgs)==0x38 && sizeof(LocalArgs)==0x68);
static bool create_canvas(std::uintptr_t owner) {

    std::uintptr_t tree=0,parent=0,id=0;
    if(!read(owner+0x2D8,tree) || !game_hud::identity(tree,id) ||
        !read(tree+0x30,parent) || !game_hud::identity(parent,id)) return false;
    parent=attachment_panel(parent);
    if(!host.set(parent)) return false;
    SpawnArgs spawn{canvas_class,tree,0};
    if(!call(Spawn,0,&spawn) || !canvas.set(spawn.result) || !set_visibility(canvas.pointer,false)) return false;
    Pair attach{canvas.pointer,0};
    if(!call(AddChild,parent,&attach) || !attach.result) return false;
    // A CanvasPanel root needs fill anchors; Overlay/other panel slots reject
    // this function without invocation and use their normal fill layout.
    double anchors[]{0,0,1,1};
    if(call(Anchors,attach.result,anchors)) {
        float offsets[4]{};
        if(!call(Offsets,attach.result,offsets)) return false;
        int z=10000; if(!call(ZOrder,attach.result,&z)) return false;
    }
    // Attaching Slate widgets can assign their previously-zero weak serial.
    return canvas.set(spawn.result);
}
static bool create_picture(int index) {
    static constexpr int kind[]{0,0,1,2,2,3},mode[]{2,4,1,1,2,1};
    HBITMAP badge=booster_icons::make(kind[index],mode[index]);
    if(!badge) return false;
    HDC dc=CreateCompatibleDC(nullptr);
    BITMAPINFO format{}; format.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    format.bmiHeader.biWidth=48; format.bmiHeader.biHeight=-48;
    format.bmiHeader.biPlanes=1; format.bmiHeader.biBitCount=32;
    std::uint32_t pixels[48*48]{};
    bool rendered=dc && GetDIBits(dc,badge,0,48,pixels,&format,DIB_RGB_COLORS)==48;
    if(dc) DeleteDC(dc); DeleteObject(badge);
    if(!rendered) return false;
    auto png=icon_png::encode(pixels,48,48);
    ImportArgs texture{root.pointer,reinterpret_cast<std::uintptr_t>(png.data()),
        static_cast<int>(png.size()),static_cast<int>(png.size()),0};
    if(!call(Import,0,&texture) || !texture.result) return false;
    std::uintptr_t tree=0;
    if(!read(root.pointer+0x2D8,tree)) return false;
    SpawnArgs spawn{image_class,tree,0};
    if(!call(Spawn,0,&spawn) || !pictures[index].set(spawn.result)) return false;
    BrushArgs brush{texture.result,false,{}};
    if(!call(Brush,spawn.result,&brush) || !set_visibility(spawn.result,false)) return false;
    Pair attach{spawn.result,0};
    if(!call(AddCanvas,canvas.pointer,&attach) || !slots[index].set(attach.result)) return false;
    int z=10000;
    return call(ZOrder,attach.result,&z) && pictures[index].set(spawn.result) && slots[index].set(attach.result);
}
// Returns true only once the native HUD is ready to replace the window overlay.
static bool update(std::uintptr_t owner,const IndicatorStatus& state,bool allowed,
        int width,int height,POINT corner) {
    if(!ready()) { clear(); return false; }
    if(root.pointer!=owner || !root.valid()) {
        clear();
        if(!root.set(owner)) return false;
        if(!create_canvas(owner)) return abandon(owner);
    }
    if(failed) return false;
    if(!canvas.valid()) { clear(); return false; }
    if(built<6) {
        if(!create_picture(built)) { clear(); root.set(owner); failed=true;
            log("Native booster HUD initialization failed; window overlay retained."); return false; }
        ++built;
        if(built<6) return false; // Limit initial texture/UI creation to one badge per callback.
    }
    // Convert viewport pixels through the actual parent geometry: the root
    // ScaleBox applies additional scale/offset beyond the viewport DPI curve.
    Vec origin{},step{1,1};
    bool layout=width!=last_width || height!=last_height;
    if(!last_dpi_check || layout || GetTickCount64()-last_dpi_check>500) {
        if(!host.valid()) return abandon(owner);
        GeometryArgs geometry{};
        if(!call(Geometry,host.pointer,&geometry)) return abandon(owner);
        LocalArgs first{owner,geometry,{double(corner.x+2),double(corner.y+2)},{},true,{}};
        LocalArgs second{owner,geometry,{double(corner.x+56),double(corner.y+50)},{},true,{}};
        if(!call(ToLocal,0,&first) || !call(ToLocal,0,&second)) return abandon(owner);
        origin=first.local; step={(second.local.x-first.local.x)/54,(second.local.y-first.local.y)/48};
        if(!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(step.x) ||
            !std::isfinite(step.y) || step.x<=0 || step.y<=0 || step.x>16 || step.y>16) {
            set_visibility(canvas.pointer,false); return false; // Wait for the first layout pass.
        }
        static Vec previous_origin{},previous_step{};
        layout|=origin.x!=previous_origin.x || origin.y!=previous_origin.y ||
            step.x!=previous_step.x || step.y!=previous_step.y;
        previous_origin=origin; previous_step=step;
        last_dpi_check=GetTickCount64();
    }
    auto appearance=std::uint64_t(state.selected) | (std::uint64_t(state.encounters)<<8) |
        (std::uint64_t(state.boost)<<16) | (std::uint64_t(state.auto_attack)<<17) | (std::uint64_t(allowed)<<18);
    if(layout || appearance!=last_state) {
        bool visible[]{allowed && state.selected==2,allowed && state.selected==4,
            allowed && state.boost,allowed && state.encounters==1,
            allowed && state.encounters==2,allowed && state.auto_attack};
        static constexpr int cell[]{0,0,1,2,2,3};
        for(int i=0;i<6;++i) {
            if(!pictures[i].valid() || !slots[i].valid()) { clear(); return false; }
            if(layout) {
                Vec position{origin.x+cell[i]*booster_icons::tile*step.x,origin.y};
                Vec size{48*step.x,48*step.y};
                if(!call(Position,slots[i].pointer,&position) || !call(Size,slots[i].pointer,&size)) return abandon(owner);
            }
            if(!set_visibility(pictures[i].pointer,visible[i])) return abandon(owner);
        }
        if(!set_visibility(canvas.pointer,allowed)) return abandon(owner);
        if(!last_width) log("Native booster HUD ready; badges render inside the game viewport.");
        last_state=appearance; last_width=width; last_height=height;
    }
    return true;
}
}
static void update_native_indicator() {
    bool handled=false;
    RECT client{};
    if(window && GetClientRect(window,&client) && client.right>0 && client.bottom>0) {
        for(auto root:game_hud::roots) {
            if(!game_hud::active_root(root)) continue;
            auto position=indicator_position(client.right,client.bottom,indicator_x,indicator_y);
            handled=native_indicator::update(root,indicator_status(),indicator_allowed(),client.right,client.bottom,position);
            break;
        }
    }
    native_indicator_active.store(handled);
    if(handled && indicator.load()) ShowWindowAsync(indicator.load(),SW_HIDE);
}
