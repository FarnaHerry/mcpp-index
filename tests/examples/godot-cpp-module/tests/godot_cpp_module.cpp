// Behavioral test for the godotengine.godot-cpp-m module package: godot-cpp's
// API reached with `import godot_cpp;` and no #include at all.
//
// The numbers are deliberately the same ones tests/examples/godot-cpp asserts
// through headers -- that member is the header spelling of this one, so a
// divergence here means the module re-export changed behaviour.
//
// Both halves can fail:
//   * Vector2::length(), Basis::orthonormalized(), Color::to_rgba32() and
//     AABB::get_volume() are DEFINED in compat.godot-cpp's src/variant/*.cpp,
//     so they only resolve if that dependency really compiled and linked.
//   * Node, Node2D, Node::PROCESS_MODE_*, godot::OK and Variant::OBJECT exist
//     only in the pre-generated gen/ tree.
//
// Not covered here, by nature: anything routed through the
// gdextension_interface_* function pointers (String, Array, class
// registration) needs a Godot process that has loaded the extension. The
// GDCLASS/macro shape is covered by the package's own test suite.

import std;
import godot_cpp;

namespace {

bool close(double a, double b) {
    return std::fabs(a - b) < 1e-5;
}

} // namespace

int main() {
    using namespace godot;

    const bool vec2_ok = close(Vector2(3, 4).length(), 5.0) &&
                         close(Vector2(3, 4).normalized().length(), 1.0);

    const Vector3 cross = Vector3(1, 0, 0).cross(Vector3(0, 1, 0));
    const bool vec3_ok = cross == Vector3(0, 0, 1) &&
                         close(Vector3(2, 3, 6).length(), 7.0);

    const bool basis_ok = close(Basis().orthonormalized().determinant(), 1.0);

    const bool color_ok = Color(1.0f, 0.0f, 0.0f, 1.0f).to_rgba32() == 0xff0000ffu;

    const AABB box(Vector3(0, 0, 0), Vector3(2, 3, 4));
    const AABB other(Vector3(1, 1, 1), Vector3(4, 4, 4));
    const bool aabb_ok = close(box.get_volume(), 24.0) &&
                         box.intersects(other) &&
                         close(box.intersection(other).get_volume(), 6.0);

    const bool gen_ok = sizeof(Node) > 0 &&
                        sizeof(Node2D) > 0 &&
                        Node::PROCESS_MODE_INHERIT == 0 &&
                        Node::PROCESS_MODE_DISABLED == 4 &&
                        godot::OK == 0 &&
                        godot::ERR_FILE_NOT_FOUND == 7 &&
                        godot::SIDE_LEFT == 0 &&
                        Variant::OBJECT != Variant::NIL;

    const bool math_ok = Math::is_equal_approx(1.0f, 1.0f) &&
                         !Math::is_zero_approx(1.0f) &&
                         close(Math::lerp(0.0, 10.0, 0.25), 2.5);

    const bool ok = vec2_ok && vec3_ok && basis_ok && color_ok && aabb_ok &&
                    gen_ok && math_ok;
    std::println("vec2={} vec3={} basis={} color={} aabb={} gen={} math={}",
                 vec2_ok, vec3_ok, basis_ok, color_ok, aabb_ok, gen_ok, math_ok);
    return ok ? 0 : 1;
}
