#pragma once
#include "display.hpp"
#include "booster_icons.hpp"

static std::atomic<std::uint64_t> hud_state{0};
static std::atomic<bool> show_indicator{true};
static int indicator_x = 20, indicator_y = 240;
static std::atomic<bool> hud_visible{true};
static std::atomic<bool> native_indicator_active{false};
static std::atomic<HWND> indicator{nullptr};
static constexpr int indicator_width = booster_icons::tile*4, indicator_height = booster_icons::size+4;
static std::uint64_t indicator_visual_state() { return hud_state.load() & 0x1F000007ULL; }

// A centered 16:9 rectangle is inside both the stock constrained picture and
// an expanded ultrawide picture. No dependency on another plugin's presence.
static POINT indicator_position(int width, int height, int offset_x, int offset_y) {
    if (width<=0 || height<=0) return {0,0};
    double safe_width=std::fmin(width,height*(16.0/9.0));
    double safe_height=std::fmin(height,width*(9.0/16.0));
    double left=(width-safe_width)*0.5, top=(height-safe_height)*0.5;
    double right=std::fmax(left,left+safe_width-indicator_width);
    double bottom=std::fmax(top,top+safe_height-indicator_height);
    return {static_cast<LONG>(std::lround(std::fmin(right,std::fmax(left,left+offset_x)))),
        static_cast<LONG>(std::lround(std::fmin(bottom,std::fmax(top,top+offset_y))))};
}

static void publish_status(int selected, float effective, SpeedMode mode, int encounters, bool boost, int auto_attack) {
    int rate = std::isfinite(effective) ? static_cast<int>(effective * 100 + 0.5f) : 100;
    if (rate < 0) rate = 0;
    if (rate > 65535) rate = 65535;
    hud_state.store(static_cast<std::uint64_t>(selected) | (static_cast<std::uint64_t>(rate) << 3) |
        (static_cast<std::uint64_t>(mode) << 19) |
        (static_cast<std::uint64_t>(encounters) << 24) |
        (static_cast<std::uint64_t>(boost) << 26) | (static_cast<std::uint64_t>(auto_attack) << 27));
}
static IndicatorStatus indicator_status() {
    auto state = hud_state.load();
    return {static_cast<int>(state & 7), 
        static_cast<int>((state >> 24) & 3), bool((state >> 26) & 1), static_cast<int>((state >> 27) & 3)};
}
static bool indicator_allowed() {
    auto mode = static_cast<SpeedMode>((hud_state.load() >> 19) & 15);
    auto state=indicator_status();
    bool active=state.selected>1 || state.boost || state.encounters!=0 || state.auto_attack;
    return active && hud_visible.load() && !screen_hides_indicator.load() &&
        mode != SpeedMode::Cutscene && mode != SpeedMode::Unavailable && mode != SpeedMode::Background;
}
static void draw_indicator(HDC dc, RECT rect) {
    HBRUSH background = CreateSolidBrush(RGB(0,0,0));
    FillRect(dc,&rect,background); DeleteObject(background);
    auto state = indicator_status();
    if (state.selected>1) booster_icons::draw(dc,0,0,state.selected);
    if (state.boost) booster_icons::draw(dc,1,1,1);
    if (state.encounters!=0) booster_icons::draw(dc,2,2,state.encounters);
    if (state.auto_attack) booster_icons::draw(dc,3,3,state.auto_attack);
}
static LRESULT CALLBACK indicator_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT rect{}; GetClientRect(hwnd, &rect);
        draw_indicator(dc, rect);
        EndPaint(hwnd, &paint);
        return 0;
    }
    if (msg == WM_PRINTCLIENT) {
        RECT rect{}; GetClientRect(hwnd, &rect);
        draw_indicator(reinterpret_cast<HDC>(wparam), rect); return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}
static void update_indicator() {
    if (native_indicator_active || !hud_visible) { if (indicator) ShowWindow(indicator,SW_HIDE); return; }
    if (!indicator) {
        WNDCLASSW wc{}; wc.lpfnWndProc = indicator_proc; wc.hInstance = module;
        wc.lpszClassName = L"ResonanceBoostersIndicator";
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
        indicator = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"Resonance Game Boosters", WS_POPUP, 0, 0, indicator_width, indicator_height, window, nullptr, module, nullptr);
        if (!indicator) return;
        SetLayeredWindowAttributes(indicator, RGB(0, 0, 0), 255, LWA_COLORKEY);
    }
    HWND game = window.load();
    if (!indicator_allowed() || !game || !IsWindow(game) || IsIconic(game) || GetForegroundWindow() != game) {
        ShowWindow(indicator, SW_HIDE); return;
    }
    RECT client{};
    if (!GetClientRect(game,&client) || client.right<indicator_width || client.bottom<indicator_height) {
        ShowWindow(indicator,SW_HIDE); return;
    }
    POINT corner=indicator_position(client.right,client.bottom,indicator_x,indicator_y);
    ClientToScreen(game, &corner);
    if(GetWindowLongPtrW(indicator,GWLP_HWNDPARENT)!=reinterpret_cast<LONG_PTR>(game))
        SetWindowLongPtrW(indicator, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(game));
    RECT current_rect{};
    if(!GetWindowRect(indicator,&current_rect) || current_rect.left!=corner.x || current_rect.top!=corner.y ||
        !IsWindowVisible(indicator))
        SetWindowPos(indicator, HWND_TOPMOST, corner.x, corner.y, indicator_width, indicator_height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    static std::uint64_t previous = ~std::uint64_t{0};
    auto current = indicator_visual_state();
    if (previous != current) { previous = current; InvalidateRect(indicator, nullptr, FALSE); }
}
