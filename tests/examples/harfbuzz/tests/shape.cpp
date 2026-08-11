// compat.harfbuzz — assert the shaper actually runs, and that the FreeType
// backend is compiled in rather than merely declared.
//
// Shaping needs no font file: hb_font_get_empty() gives a font whose glyphs
// are all .notdef, which is enough to prove the buffer pipeline (itemize →
// shape → read back infos/positions) is wired and returns one glyph per input
// codepoint. That is a real assertion -- if hb-ot-shape.cc had not been
// compiled into the amalgamation, glyph_count would be 0.
#include <hb.h>
#include <hb-ft.h>
import std;

int main() {
    bool ok = true;

    auto check = [&](bool cond, std::string_view what) {
        if (!cond) {
            std::println("FAIL: {}", what);
            ok = false;
        }
    };

    check(hb_version_string() != nullptr, "hb_version_string returned null");

    unsigned major = 0, minor = 0, micro = 0;
    hb_version(&major, &minor, &micro);
    check(major >= 14, "unexpected HarfBuzz major version");

    // Shape a short ASCII run through the empty font.
    hb_buffer_t* buf = hb_buffer_create();
    check(hb_buffer_allocation_successful(buf), "hb_buffer_create failed");

    static constexpr std::string_view text = "mcpp";
    hb_buffer_add_utf8(buf, text.data(), static_cast<int>(text.size()), 0,
                       static_cast<int>(text.size()));
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    hb_shape(hb_font_get_empty(), buf, nullptr, 0);

    unsigned glyph_count = 0;
    hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

    check(glyph_count == text.size(), "shaped glyph count != input length");
    check(infos != nullptr && pos != nullptr, "shaped buffer has no infos/positions");
    if (infos != nullptr) {
        // The empty font maps everything to .notdef (glyph 0); cluster values
        // must still track the original byte offsets.
        for (unsigned i = 0; i < glyph_count; ++i) {
            check(infos[i].cluster == i, "cluster does not track input offset");
        }
    }

    hb_buffer_destroy(buf);

    // The FreeType bridge: taking its address proves hb-ft.cc was compiled in.
    // Without -DHAVE_FREETYPE the header still declares this and the build
    // fails at LINK time, which is exactly what this catches.
    check(reinterpret_cast<void*>(&hb_ft_face_create_referenced) != nullptr,
          "hb_ft_face_create_referenced missing (FreeType backend not built)");

    std::println("compat.harfbuzz smoke test: {} (hb {})", ok ? "ok" : "FAILED",
                 hb_version_string());
    return ok ? 0 : 1;
}
