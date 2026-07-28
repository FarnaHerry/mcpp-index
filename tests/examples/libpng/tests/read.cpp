// Smoke test — verify compat.libpng builds, links, and basic API works
#include "png.h"
import std;

int main() {
    auto* png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) return 1;
    auto* info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, nullptr, nullptr); return 2; }
    std::println("compat.libpng smoke test: ok");
    png_destroy_read_struct(&png, &info, nullptr);
    return 0;
}
