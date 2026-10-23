#include <cstdint>
#include <cstdio>
#include <cassert>
#include "../src/icon_png.hpp"
int main() {
    std::uint32_t pixels[]{0,0x00FF0000,0x0000FF00,0x000000FF};
    auto png=icon_png::encode(pixels,2,2);
    FILE* out=std::fopen("build/icon-png-test.png","wb"); assert(out);
    assert(std::fwrite(png.data(),png.size(),1,out)==1); std::fclose(out);
    assert(icon_png::encode(pixels,0,2).empty());
    assert(icon_png::encode(pixels,65,2).empty());
}
