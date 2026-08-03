// Behavioral test for compat.godot-cpp.
//
// Two things are under test, and both can fail:
//
//  1. The PRE-GENERATED bindings are really in the package. gen/include is
//     what upstream's binding_generator.py would have produced at build time;
//     if it were missing, <godot_cpp/classes/node.hpp> and the global
//     constants below would not compile at all.
//  2. The library links. Vector2::length(), Basis::orthonormalized(),
//     Color::to_rgba32() and AABB::get_volume() are declared in the headers
//     but DEFINED in src/variant/*.cpp, so these calls only resolve if the
//     ~1000 translation units of the package were compiled and linked in.
//
// Everything asserted here is pure math -- no gdextension_interface function
// pointers, so no running Godot process is needed. Engine classes are checked
// by compiling against them (types, enums, sizes), which is all that can be
// done outside a loaded extension.

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cmath>
#include <cstdio>

namespace {

bool close(double a, double b) {
    return std::fabs(a - b) < 1e-5;
}

} // namespace

int main() {
    using namespace godot;

    // --- out-of-line variant math: proves the library linked ---
    const bool vec2_ok = close(Vector2(3, 4).length(), 5.0) &&
                         close(Vector2(3, 4).normalized().length(), 1.0);

    const Vector3 cross = Vector3(1, 0, 0).cross(Vector3(0, 1, 0));
    const bool vec3_ok = cross == Vector3(0, 0, 1) &&
                         close(Vector3(2, 3, 6).length(), 7.0);

    const Basis basis = Basis().orthonormalized();
    const bool basis_ok = close(basis.determinant(), 1.0);

    const bool color_ok = Color(1.0f, 0.0f, 0.0f, 1.0f).to_rgba32() == 0xff0000ffu;

    const AABB box(Vector3(0, 0, 0), Vector3(2, 3, 4));
    const AABB other(Vector3(1, 1, 1), Vector3(4, 4, 4));
    const bool aabb_ok = close(box.get_volume(), 24.0) &&
                         box.intersects(other) &&
                         close(box.intersection(other).get_volume(), 1.0 * 2.0 * 3.0);

    // --- generated bindings: engine class + global enums are visible ---
    const bool gen_ok = sizeof(Node) > 0 &&
                        Node::PROCESS_MODE_INHERIT == 0 &&
                        Node::PROCESS_MODE_DISABLED == 4 &&
                        godot::OK == 0 &&
                        godot::ERR_FILE_NOT_FOUND == 7 &&
                        godot::SIDE_LEFT == 0 &&
                        Variant::OBJECT != Variant::NIL;

    const bool ok = vec2_ok && vec3_ok && basis_ok && color_ok && aabb_ok && gen_ok;
    std::printf("vec2=%d vec3=%d basis=%d color=%d aabb=%d gen=%d\n",
                vec2_ok, vec3_ok, basis_ok, color_ok, aabb_ok, gen_ok);
    return ok ? 0 : 1;
}
