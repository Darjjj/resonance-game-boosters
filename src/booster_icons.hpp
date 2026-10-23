#pragma once
#include "booster_art.hpp"
// Approved artwork embedded as 48px top-down Windows DIBs. No runtime decoding,
// external image files, or new rendering dependencies are needed.
namespace booster_icons {
static constexpr int size=48, tile=54;
// kind: 0 Turbo, 1 Supercharge, 2 Encounter Rate, 3 Auto-Battle.
static HBITMAP make(int kind,int mode) {
    int frame=kind==0 ? 4 : kind==1 ? 0 : kind==2 ? (mode==1 ? 1 : 2) : 3;
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=size; info.bmiHeader.biHeight=-size;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    void* pixels=nullptr;
    HBITMAP bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if (!bitmap) return nullptr;
    std::memcpy(pixels,booster_art::frames[frame],sizeof(booster_art::frames[frame]));
    if(kind==0) {
        // One background and one font for both rates: only the digit changes.
        HDC low=CreateCompatibleDC(nullptr), high=CreateCompatibleDC(nullptr);
        BITMAPINFO supersampled=info;
        supersampled.bmiHeader.biWidth=size*2; supersampled.bmiHeader.biHeight=-size*2;
        void* high_pixels=nullptr;
        HBITMAP large=CreateDIBSection(nullptr,&supersampled,DIB_RGB_COLORS,&high_pixels,nullptr,0);
        HFONT font=CreateFontW(-48,0,0,0,FW_BOLD,TRUE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Arial");
        if(!low || !high || !large || !font) {
            if(low) DeleteDC(low); if(high) DeleteDC(high);
            if(large) DeleteObject(large); if(font) DeleteObject(font);
            DeleteObject(bitmap); return nullptr;
        }
        auto old_low=SelectObject(low,bitmap), old_high=SelectObject(high,large);
        auto old_font=SelectObject(high,font);
        SetStretchBltMode(high,HALFTONE); SetBrushOrgEx(high,0,0,nullptr);
        StretchBlt(high,0,0,size*2,size*2,low,0,0,size,size,SRCCOPY);
        SetBkMode(high,TRANSPARENT);
        wchar_t label[]{L'x',static_cast<wchar_t>(L'0'+mode),0};
        SetTextColor(high,RGB(10,24,48));
        for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
            RECT box{x,y,size*2+x,size*2+y};
            DrawTextW(high,label,2,&box,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }
        SetTextColor(high,RGB(245,250,255));
        RECT box{0,0,size*2,size*2};
        DrawTextW(high,label,2,&box,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        SetStretchBltMode(low,HALFTONE); SetBrushOrgEx(low,0,0,nullptr);
        StretchBlt(low,0,0,size,size,high,0,0,size*2,size*2,SRCCOPY);
        SelectObject(high,old_font); SelectObject(high,old_high); SelectObject(low,old_low);
        DeleteObject(font); DeleteObject(large); DeleteDC(high); DeleteDC(low);
    }
    return bitmap;
}
static HBITMAP cached[10]{};
static void draw(HDC dc,int cell,int kind,int mode) {
    int index=kind==0 ? mode==1 ? 0 : mode==2 ? 1 : 2 : kind==1 ? 3+(mode!=0) : kind==2 ? 5+mode : 8+(mode!=0);
    if (!cached[index]) cached[index]=make(kind,mode);
    if (!cached[index]) return;
    auto source=CreateCompatibleDC(dc); auto old=SelectObject(source,cached[index]);
    BitBlt(dc,cell*tile+2,2,size,size,source,0,0,SRCCOPY);
    SelectObject(source,old); DeleteDC(source);
}
}

