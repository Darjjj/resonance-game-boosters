#include "../src/plugin.cpp"
#include <cassert>
int main() {
    // Native output must use the approved exports, including both state variants.
    struct Artwork { const char* name; int kind, mode; };
    for (auto art : {Artwork{"supercharge",1,1}, Artwork{"encounters0",2,1},
            Artwork{"encounters2",2,2}, Artwork{"auto",3,1}}) {
        char path[128]; std::snprintf(path,sizeof(path),"assets/boosters/%s.bgra",art.name);
        FILE* reference=std::fopen(path,"rb"); assert(reference);
        std::uint32_t expected[48*48]{}, actual[48*48]{};
        assert(std::fread(expected,sizeof(expected),1,reference)==1);
        assert(std::fgetc(reference)==EOF); std::fclose(reference);
        HDC output=CreateCompatibleDC(nullptr);
        HBITMAP rendered=booster_icons::make(art.kind,art.mode); assert(rendered);
        BITMAPINFO format{}; format.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        format.bmiHeader.biWidth=48; format.bmiHeader.biHeight=-48;
        format.bmiHeader.biPlanes=1; format.bmiHeader.biBitCount=32;
        assert(GetDIBits(output,rendered,0,48,actual,&format,DIB_RGB_COLORS)==48);
        if(art.kind!=0) assert(std::memcmp(expected,actual,sizeof(expected))==0);
        DeleteObject(rendered); DeleteDC(output);
    }
    // Cycling changes the digit, not the rim, lighting, size or typeface.
    std::uint32_t turbo[2][48*48]{};
    for(int i=0;i<2;++i) {
        HDC output=CreateCompatibleDC(nullptr);
        HBITMAP rendered=booster_icons::make(0,i==0 ? 2 : 4);
        BITMAPINFO format{}; format.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        format.bmiHeader.biWidth=48; format.bmiHeader.biHeight=-48;
        format.bmiHeader.biPlanes=1; format.bmiHeader.biBitCount=32;
        assert(GetDIBits(output,rendered,0,48,turbo[i],&format,DIB_RGB_COLORS)==48);
        DeleteObject(rendered); DeleteDC(output);
    }
    int changed=0,min_x=48,min_y=48,max_x=0,max_y=0;
    for(int y=0;y<48;++y) for(int x=0;x<48;++x) {
        bool different=turbo[0][y*48+x]!=turbo[1][y*48+x];
        if(different) { min_x=std::min(min_x,x); max_x=std::max(max_x,x);
            min_y=std::min(min_y,y); max_y=std::max(max_y,y); }
        changed+=different;
    }
    std::printf("Turbo digit difference: (%d,%d)-(%d,%d), %d pixels\n",min_x,min_y,max_x,max_y,changed);
    std::fflush(stdout);
    assert(min_x>=10 && max_x<=39 && min_y>=10 && max_y<=38);
    assert(changed>10 && changed<400);
    screen_hides_indicator=false;
    assert(indicator_proc(nullptr, WM_NCHITTEST, 0, 0) == HTTRANSPARENT);
    assert(indicator_proc(nullptr, WM_MOUSEACTIVATE, 0, 0) == MA_NOACTIVATE);
    // Offsets are relative to a centered 16:9 safe area, not black bars.
    auto check_position=[](int w,int h,int x,int y) {
        auto p=indicator_position(w,h,20,160);
        assert(p.x==x && p.y==y);
        assert(p.x>=0 && p.y>=0 && p.x+indicator_width<=w && p.y+indicator_height<=h);
    };
    check_position(3440,1440,460,160); // User's pillarboxed screenshot.
    check_position(1920,1080,20,160);
    check_position(2560,1440,20,160);
    check_position(3840,1080,980,160); // 32:9.
    check_position(1920,1200,20,220); // 16:10 letterbox.
    check_position(1280,720,20,160);
    auto clamped=indicator_position(1920,1080,10000,10000);
    assert(clamped.x+indicator_width==1920 && clamped.y+indicator_height==1080);
    auto negative=indicator_position(1920,1080,-100,-100);
    assert(negative.x==0 && negative.y==0);
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = indicator_width; info.bmiHeader.biHeight = -indicator_height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    assert(bitmap && pixels);
    auto old = SelectObject(dc, bitmap);
    publish_status(4, 4, SpeedMode::Gameplay, 2, true, true);
    auto status = indicator_status();
    assert(status.selected == 4 && status.encounters == 2 && status.boost && status.auto_attack);
    assert(indicator_allowed());
    auto appearance=indicator_visual_state();
    publish_status(4,0.25f,SpeedMode::Battle,2,true,true);
    assert(indicator_visual_state()==appearance); // Battle hit-stop does not change badge artwork.
    publish_status(4,4,SpeedMode::Battle,2,true,false);
    assert(indicator_visual_state()!=appearance);
    publish_status(4,4,SpeedMode::Gameplay,2,true,true);
    screen_hides_indicator=true;
    assert(!indicator_allowed());
    screen_hides_indicator=false;
    assert(indicator_allowed());
    for (auto mode : {SpeedMode::Cutscene, SpeedMode::Unavailable, SpeedMode::Background}) {
        publish_status(1, 1, mode, 0, false, false);
        assert(!indicator_allowed());
    }
    for (int state = 0; state < 3; ++state) {
        publish_status(state == 0 ? 1 : state == 1 ? 2 : 4, 1, SpeedMode::Gameplay,
            state, state == 2, state == 2);
        draw_indicator(dc, {0, 0, indicator_width, indicator_height});
        GdiFlush();
        auto bytes = static_cast<unsigned char*>(pixels);
        // No panel: every outer corner must remain the transparent key.
        for (int at : {0, (indicator_width - 1) * 4, (indicator_height - 1) * indicator_width * 4})
            assert(bytes[at] == 0 && bytes[at+1] == 0 && bytes[at+2] == 0);
        for (int cell = 0; cell < 4; ++cell) {
            bool drawn = false;
            for (int y = 1; y < indicator_height - 1; ++y)
                for (int x = cell * booster_icons::tile; x < (cell + 1) * booster_icons::tile; ++x) {
                    auto pixel = bytes + (y * indicator_width + x) * 4;
                    drawn |= pixel[0] || pixel[1] || pixel[2];
                }
            bool active = state==2 || (state==1 && (cell==0 || cell==2));
            assert(drawn == active); // Disabled boosters keep an empty fixed slot.
        }
        char path[80]; std::snprintf(path, sizeof(path), "build/indicator-icons-%d.bmp", state);
        FILE* file = std::fopen(path, "wb"); assert(file);
        BITMAPFILEHEADER header{}; header.bfType = 0x4D42;
        header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
        header.bfSize = header.bfOffBits + indicator_width * indicator_height * 4;
        std::fwrite(&header, sizeof(header), 1, file);
        std::fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, file);
        std::fwrite(pixels, indicator_width * indicator_height * 4, 1, file);
        std::fclose(file);
    }
    SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc);
    std::puts("Indicator input-pass-through checks and offscreen renders passed");
}


