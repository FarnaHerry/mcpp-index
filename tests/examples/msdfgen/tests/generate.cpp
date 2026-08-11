// compat.msdfgen — generate a real multi-channel signed distance field and
// assert its contents, not just that the library links.
//
// The shape is a square built by hand, so the test needs no font file and no
// ext/ importer. What makes the assertions real:
//   * the field must be signed -- inside the square the distance is positive,
//     outside it is negative. A blank or all-zero bitmap fails both.
//   * edge colouring must have run, otherwise the three channels are identical
//     and the "multi-channel" part of MSDF did nothing.
//
// The `msdfgen/` include prefix is the one this package generates; using it
// here is what keeps the shim covered by CI.
#include <msdfgen/msdfgen.h>
#include <msdfgen/msdfgen-ext.h>
import std;

int main() {
    bool ok = true;

    auto check = [&](bool cond, std::string_view what) {
        if (!cond) {
            std::println("FAIL: {}", what);
            ok = false;
        }
    };

    // A 16x16 axis-aligned square, inset from the 32x32 bitmap so that both
    // the inside and the outside are sampled.
    msdfgen::Shape shape;
    msdfgen::Contour& contour = shape.addContour();
    const msdfgen::Point2 p0(8, 8), p1(24, 8), p2(24, 24), p3(8, 24);
    contour.addEdge(msdfgen::EdgeHolder(p0, p1));
    contour.addEdge(msdfgen::EdgeHolder(p1, p2));
    contour.addEdge(msdfgen::EdgeHolder(p2, p3));
    contour.addEdge(msdfgen::EdgeHolder(p3, p0));

    shape.normalize();
    check(shape.validate(), "hand-built shape did not validate");

    // Winding decides the sign of the field, and the order above is not
    // guaranteed to be the one msdfgen calls "inside". orientContours() is
    // upstream's own normalizer for exactly this (its standalone tool runs it
    // on imported geometry), so the assertions below can talk about inside and
    // outside without depending on how the contour happened to be written.
    shape.orientContours();

    msdfgen::edgeColoringSimple(shape, 3.0);

    msdfgen::Bitmap<float, 3> msdf(32, 32);
    msdfgen::SDFTransformation t(
        msdfgen::Projection(msdfgen::Vector2(1.0), msdfgen::Vector2(0.0)),
        msdfgen::Range(4.0));
    msdfgen::generateMSDF(msdf, shape, t);

    // Median of the three channels is the reconstructed signed distance.
    auto median_at = [&](int x, int y) {
        const float* px = msdf(x, y);
        return msdfgen::median(px[0], px[1], px[2]);
    };

    // Centre of the square is inside → positive; a corner of the bitmap is
    // far outside → negative. msdfgen encodes 0.5 as the edge.
    const float inside = median_at(16, 16);
    const float outside = median_at(0, 0);
    check(inside > 0.5f, std::format("centre should read inside, got {}", inside));
    check(outside < 0.5f, std::format("corner should read outside, got {}", outside));

    // Multi-channel: at least one pixel must have channels that differ,
    // otherwise edge colouring never ran and this is a plain SDF.
    bool multi_channel = false;
    for (int y = 0; y < 32 && !multi_channel; ++y) {
        for (int x = 0; x < 32; ++x) {
            const float* px = msdf(x, y);
            if (px[0] != px[1] || px[1] != px[2]) {
                multi_channel = true;
                break;
            }
        }
    }
    check(multi_channel, "all three channels identical — edge colouring did not run");

    // ext/import-font is compiled in: initializing FreeType through msdfgen
    // proves the FreeType-backed half of the package is present, and that the
    // transitive compat.freetype dependency reached the link line.
    msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
    check(ft != nullptr, "msdfgen::initializeFreetype returned null");
    if (ft) {
        msdfgen::deinitializeFreetype(ft);
    }

    std::println("compat.msdfgen smoke test: {} (inside={:.3f} outside={:.3f})",
                 ok ? "ok" : "FAILED", inside, outside);
    return ok ? 0 : 1;
}
