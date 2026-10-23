#pragma once
#include <array>
#include <mutex>

// The worker owns discovery. Game callbacks take a small, nonblocking copy;
// they keep the previous snapshot if publication happens to be in progress.
namespace battle_discovery {
struct WidgetApi { const wchar_t* owner; const wchar_t* name; bool is_static; };
static constexpr WidgetApi widget_apis[]{
    {L"Widget",L"SetRenderOpacity",false},{L"Widget",L"IsInViewport",false},
    {L"GameplayStatics",L"SpawnObject",true},{L"KismetRenderingLibrary",L"ImportBufferAsTexture2D",true},
    {L"PanelWidget",L"AddChild",false},{L"CanvasPanel",L"AddChildToCanvas",false},
    {L"Image",L"SetBrushFromTexture",false},{L"Widget",L"SetVisibility",false},
    {L"CanvasPanelSlot",L"SetPosition",false},{L"CanvasPanelSlot",L"SetSize",false},
    {L"CanvasPanelSlot",L"SetZOrder",false},{L"Widget",L"RemoveFromParent",false},
    {L"WidgetLayoutLibrary",L"GetViewportScale",true},{L"CanvasPanelSlot",L"SetAnchors",false},
    {L"CanvasPanelSlot",L"SetOffsets",false},
    {L"Widget",L"GetCachedGeometry",false},{L"SlateBlueprintLibrary",L"ScreenToWidgetLocal",true}
};
static constexpr std::size_t widget_api_count=sizeof(widget_apis)/sizeof(widget_apis[0]);
struct Snapshot {
    struct HudFunction { std::uintptr_t function=0,owner=0; std::uint64_t name=0; std::uintptr_t defaults=0; };
    std::array<std::uintptr_t,8> interfaces{};
    std::array<std::uintptr_t,64> views{};
    std::array<std::uintptr_t,64> resources{};
    std::array<std::uintptr_t,8> hud_roots{};
    std::array<HudFunction,widget_api_count> hud_functions{};
    std::array<std::uintptr_t,2> widget_classes{}; // Image, CanvasPanel.
    std::size_t hud_root_count=0;
    std::size_t interface_count=0, view_count=0, resource_count=0;
    std::uint64_t revision=0;
};
static std::mutex mutex;
static Snapshot latest;
static void publish(Snapshot value) {
    std::lock_guard<std::mutex> lock(mutex);
    value.revision=latest.revision+1;
    latest=value;
}
static bool copy(Snapshot& out) {
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
    if(!lock || latest.revision==out.revision) return false;
    out=latest; return true;
}
}
