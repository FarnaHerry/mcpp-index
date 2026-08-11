// compat.nanosvg — parse a real SVG document and rasterize it.
//
// This asserts three separate things the package must get right:
//   1. `<nanosvg.h>` / `<nanosvgrast.h>` resolve (include_dirs points at src/),
//   2. the parser's output is real geometry, not an empty image, and
//   3. the generated implementation TU is actually linked -- BOTH halves of it.
//      nsvgParse comes from nanosvg.h's implementation and nsvgRasterize from
//      nanosvgrast.h's, so if the package only instantiated one of the two this
//      test fails at link time rather than silently passing.
#include <nanosvg.h>
#include <nanosvgrast.h>
import std;

int main() {
    bool ok = true;
    auto check = [&](bool cond, std::string_view what) {
        if (!cond) {
            std::println("FAIL: {}", what);
            ok = false;
        }
    };

    // A 100x100 canvas holding one opaque red rectangle covering the middle
    // half. nsvgParse mutates its input, so hand it a writable copy.
    std::string svg =
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">)"
        R"(<rect x="25" y="25" width="50" height="50" fill="#ff0000"/>)"
        R"(</svg>)";

    NSVGimage* image = nsvgParse(svg.data(), "px", 96.0f);
    check(image != nullptr, "nsvgParse returned an image");
    if (!image) return 1;

    check(image->width == 100.0f && image->height == 100.0f, "canvas dimensions");

    int shapes = 0, paths = 0;
    for (NSVGshape* sh = image->shapes; sh != nullptr; sh = sh->next) {
        ++shapes;
        for (NSVGpath* p = sh->paths; p != nullptr; p = p->next) ++paths;
    }
    check(shapes == 1, "one shape parsed");
    check(paths >= 1, "the shape has at least one path");

    // The rect's bounds must be the ones the document declared.
    if (image->shapes) {
        const float* b = image->shapes->bounds;
        check(std::abs(b[0] - 25.0f) < 0.5f && std::abs(b[1] - 25.0f) < 0.5f &&
              std::abs(b[2] - 75.0f) < 0.5f && std::abs(b[3] - 75.0f) < 0.5f,
              "shape bounds match the declared rect");
        // fill colour is packed ABGR by nanosvg
        check((image->shapes->fill.color & 0x00ffffffu) == 0x000000ffu,
              "fill colour is red");
    }

    // ---- rasterize -------------------------------------------------------
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    check(rast != nullptr, "rasterizer created");
    if (rast) {
        constexpr int W = 100, H = 100;
        std::vector<unsigned char> px(static_cast<std::size_t>(W) * H * 4, 0);
        nsvgRasterize(rast, image, 0.0f, 0.0f, 1.0f, px.data(), W, H, W * 4);

        auto at = [&](int x, int y) -> const unsigned char* {
            return px.data() + (static_cast<std::size_t>(y) * W + x) * 4;
        };
        // Inside the rect: opaque red. Outside: untouched (alpha 0).
        const unsigned char* in  = at(50, 50);
        const unsigned char* out = at(5, 5);
        check(in[3] > 200, "centre pixel is opaque");
        check(in[0] > 200 && in[1] < 60 && in[2] < 60, "centre pixel is red");
        check(out[3] == 0, "corner pixel is untouched");

        std::size_t covered = 0;
        for (std::size_t i = 3; i < px.size(); i += 4) if (px[i] > 128) ++covered;
        // 50x50 of 100x100 = 2500 px, allow for antialiased edges.
        check(covered > 2300 && covered < 2700, "covered area matches a 50x50 rect");

        nsvgDeleteRasterizer(rast);
    }

    nsvgDelete(image);

    if (ok) std::println("nanosvg OK");
    return ok ? 0 : 1;
}
