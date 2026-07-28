// Smoke test — verify compat.freetype builds, links, and basic API works
#include <ft2build.h>
#include FT_FREETYPE_H
import std;

int main() {
    FT_Library library{};
    FT_Error error = FT_Init_FreeType(&library);
    if (error) {
        std::println("FT_Init_FreeType failed: {}", error);
        return 1;
    }
    std::println("compat.freetype smoke test: ok");
    FT_Done_FreeType(library);
    return 0;
}
