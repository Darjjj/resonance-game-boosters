#pragma once
#include <vector>
namespace icon_png {
using Bytes=std::vector<unsigned char>;
static void word(Bytes& out,std::uint32_t value) {
    for(int shift:{24,16,8,0}) out.push_back(static_cast<unsigned char>(value>>shift));
}
static std::uint32_t crc(const unsigned char* bytes,std::size_t size) {
    std::uint32_t value=~0u;
    for(std::size_t i=0;i<size;++i) {
        value^=bytes[i];
        for(int bit=0;bit<8;++bit) value=(value>>1)^(0xEDB88320u&-(value&1u));
    }
    return ~value;
}
static void chunk(Bytes& out,const char* kind,const Bytes& data) {
    word(out,static_cast<std::uint32_t>(data.size()));
    auto start=out.size();
    out.insert(out.end(),kind,kind+4); out.insert(out.end(),data.begin(),data.end());
    word(out,crc(out.data()+start,out.size()-start));
}
// Small, deterministic RGBA PNG with one uncompressed DEFLATE block. Used
// once per badge at HUD creation, never during rendering or state changes.
static Bytes encode(const std::uint32_t* pixels,int width,int height) {
    if(width<=0 || height<=0 || width>64 || height>64) return {};
    Bytes raw;
    for(int y=0;y<height;++y) {
        raw.push_back(0); // PNG filter: None.
        for(int x=0;x<width;++x) {
            auto pixel=pixels[y*width+x];
            raw.push_back(static_cast<unsigned char>(pixel>>16));
            raw.push_back(static_cast<unsigned char>(pixel>>8));
            raw.push_back(static_cast<unsigned char>(pixel));
            raw.push_back((pixel&0xFFFFFF) ? 255 : 0); // Same black key as GDI.
        }
    }
    std::uint32_t a=1,b=0;
    for(auto byte:raw) { a=(a+byte)%65521; b=(b+a)%65521; }
    auto length=static_cast<std::uint16_t>(raw.size());
    Bytes compressed{0x78,0x01,0x01,static_cast<unsigned char>(length),
        static_cast<unsigned char>(length>>8),static_cast<unsigned char>(~length),
        static_cast<unsigned char>((~length)>>8)};
    compressed.insert(compressed.end(),raw.begin(),raw.end()); word(compressed,(b<<16)|a);
    Bytes out{137,80,78,71,13,10,26,10},header;
    word(header,width); word(header,height);
    header.insert(header.end(),{8,6,0,0,0});
    chunk(out,"IHDR",header); chunk(out,"IDAT",compressed); chunk(out,"IEND",{});
    return out;
}
}
