-- compat.godot-cpp -- godot-cpp (the C++ bindings for Godot's GDExtension API)
-- as an ordinary mcpp source package: `#include <godot_cpp/...>` works out of
-- the box, no SCons, no CMake and no Python on the consumer side.
--
-- Why the download is NOT the upstream tag archive
--   godot-cpp is only half a source tree. The other half -- ~1000 engine
--   classes and every builtin Variant type, i.e. gen/include + gen/src -- is
--   produced by upstream's own binding_generator.py from
--   gdextension/extension_api.json, and no upstream tag archive or release
--   carries it. A package that ran that generator at install time would make
--   a Python toolchain a hard runtime dependency of every consumer, on every
--   platform, which is exactly what this index avoids elsewhere (the frozen
--   configure snapshots in compat.ffmpeg / the opencv module package).
--
--   So the generator runs ONCE, offline, and the result is published as an
--   immutable mirror archive: upstream's tree byte-for-byte (the repack
--   verifies this and refuses otherwise) plus the gen/ tree that upstream's
--   unmodified binding_generator.py emitted for it. Recipe and verification
--   live in tools/godot-cpp/repack.sh; upstream's own archive for
--   godot-4.5-stable hashes to
--   ac78539c0042554c494ea419549d2de88758d448721aeb0e5d41129aa87e339c.
--
--   Bindings are generated for the default configuration -- 64-bit,
--   precision=single, template_get_node on -- which is what `scons` and
--   `cmake` give you by default. A double-precision build needs a different
--   gen/ tree, so it cannot be a feature over this archive; it would be a
--   second version/package.
--
-- Defines
--   GDEXTENSION is upstream's PUBLIC compile definition (cmake sets it on the
--   godot-cpp target's INTERFACE), so it rides on a default feature: the lib
--   and every consumer TU must agree.
--
--   TYPED_METHOD_BIND rides along, and is not optional on the MSVC ABI.
--   Without it, method_bind.hpp reinterpret_casts member pointers through a
--   FORWARD-DECLARED `_gde_UnexistingClass`; under the MSVC ABI a
--   pointer-to-member's size depends on the class's inheritance model, so for
--   an incomplete class clang-cl rejects the cast outright ("cannot
--   reinterpret_cast ... to member pointer type of different size") and every
--   ClassDB::bind_method call fails to compile. Upstream's cmake sets it
--   PUBLIC for exactly this reason ($<${IS_MSVC}: TYPED_METHOD_BIND ...> in
--   cmake/windows.cmake). It is set unconditionally rather than per-OS
--   because it is a HEADER switch that changes MethodBindT's template
--   parameter list -- library and consumer must agree on it, and one uniform
--   answer is cheaper to guarantee than an OS-conditional one. The cost off
--   MSVC is some extra template instantiation, which is why upstream keeps
--   the untyped path as its default there; there is no behavioural
--   difference. (WINDOWS_ENABLED and NOMINMAX, which upstream sets alongside,
--   are NOT needed: neither appears anywhere in the shipped headers or
--   sources.) The layout-affecting ones are left
--   undefined on both sides, which is upstream's release default:
--   DEBUG_ENABLED / DEV_ENABLED (extra checks), HOT_RELOAD_ENABLED (changes
--   the Wrapped layout) and REAL_T_IS_DOUBLE (needs the double-precision
--   gen/ tree above). They are deliberately NOT features: each would re-key
--   the store into a second full ~1000-TU build of the same library.
--
-- Consuming
--   compat.godot-cpp is the plain-header form. `import godot_cpp;` is the
--   module package godotengine.godot-cpp (mcpplibs/godot-cpp-m), which builds
--   on top of this one.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "godot-cpp",
    description = "C++ bindings for the Godot GDExtension API (pre-generated bindings, no Python/SCons needed)",
    licenses    = {"MIT"},
    repo        = "https://github.com/godotengine/godot-cpp",
    type        = "package",

    -- One OS-neutral archive: godot-cpp is portable C++ with no per-platform
    -- source selection (the platform split lives in Godot itself, behind the
    -- gdextension_interface.h ABI).
    xpm = {
        linux = {
            ["10.0.0-rc1"] = {
                url = {
                    GLOBAL = "https://github.com/xlings-res/godot-cpp/releases/download/10.0.0-rc1/godot-cpp-10.0.0-rc1.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/godot-cpp/releases/download/10.0.0-rc1/godot-cpp-10.0.0-rc1.tar.gz",
                },
                sha256 = "aaafbf50d4b8469d610fdb2eb76c6f58d758dbabbc6b013f60464d99b20ceb6e",
            },
            ["4.5.0"] = {
                url = {
                    GLOBAL = "https://github.com/xlings-res/godot-cpp/releases/download/4.5.0/godot-cpp-4.5.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/godot-cpp/releases/download/4.5.0/godot-cpp-4.5.0.tar.gz",
                },
                sha256 = "b0c36e77f02c4181352cdd7547b209b93a833be1ad6197f8c650d92987221a00",
            },
        },
        macosx = {
            ["10.0.0-rc1"] = {
                url = {
                    GLOBAL = "https://github.com/xlings-res/godot-cpp/releases/download/10.0.0-rc1/godot-cpp-10.0.0-rc1.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/godot-cpp/releases/download/10.0.0-rc1/godot-cpp-10.0.0-rc1.tar.gz",
                },
                sha256 = "aaafbf50d4b8469d610fdb2eb76c6f58d758dbabbc6b013f60464d99b20ceb6e",
            },
            ["4.5.0"] = {
                url = {
                    GLOBAL = "https://github.com/xlings-res/godot-cpp/releases/download/4.5.0/godot-cpp-4.5.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/godot-cpp/releases/download/4.5.0/godot-cpp-4.5.0.tar.gz",
                },
                sha256 = "b0c36e77f02c4181352cdd7547b209b93a833be1ad6197f8c650d92987221a00",
            },
        },
        windows = {
            ["10.0.0-rc1"] = {
                url = {
                    GLOBAL = "https://github.com/xlings-res/godot-cpp/releases/download/10.0.0-rc1/godot-cpp-10.0.0-rc1.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/godot-cpp/releases/download/10.0.0-rc1/godot-cpp-10.0.0-rc1.tar.gz",
                },
                sha256 = "aaafbf50d4b8469d610fdb2eb76c6f58d758dbabbc6b013f60464d99b20ceb6e",
            },
            ["4.5.0"] = {
                url = {
                    GLOBAL = "https://github.com/xlings-res/godot-cpp/releases/download/4.5.0/godot-cpp-4.5.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/godot-cpp/releases/download/4.5.0/godot-cpp-4.5.0.tar.gz",
                },
                sha256 = "b0c36e77f02c4181352cdd7547b209b93a833be1ad6197f8c650d92987221a00",
            },
        },
    },

    mcpp = {
        schema       = "0.1",
        language     = "c++23",
        import_std   = false,
        -- Three roots, exactly as upstream's build systems expose them:
        -- hand-written headers, generated headers, and the GDExtension C ABI
        -- header that both of them include. Where that header lives moved
        -- between the two versions -- 4.5 checks in gdextension/
        -- gdextension_interface.h, 10.x generates it into gen/include/ from
        -- gdextension_interface.json -- so both roots stay listed.
        include_dirs = { "*/include", "*/gen/include", "*/gdextension" },
        -- Enumerated rather than `**`: upstream's own test project ships a
        -- test/src/*.cpp that must not be swept into the library, and the two
        -- source roots are only ever one and two levels deep.
        -- Union of both layouts, catch2-style: a glob that matches nothing on
        -- a given version is simply skipped. 10.x adds one .cpp directly under
        -- gen/src/ that 4.5 does not have.
        sources      = {
            "*/src/*.cpp",
            "*/src/*/*.cpp",
            "*/gen/src/*.cpp",
            "*/gen/src/*/*.cpp",
        },
        targets      = { ["godot-cpp"] = { kind = "lib" } },
        features     = {
            ["default"]    = { implies = { "gdextension" } },
            ["gdextension"] = { defines = { "GDEXTENSION", "TYPED_METHOD_BIND" } },
        },
        deps         = { },
        -- A GDExtension IS a shared library, so this static library's objects
        -- are almost always linked into one. Without position-independent code
        -- that link fails outright ("relocation R_X86_64_32 against `.rodata`
        -- can not be used when making a shared object"), which would leave the
        -- package unable to do the one thing it exists for -- upstream's own
        -- SCons and CMake builds pass -fPIC for exactly this reason. Not
        -- needed on windows: the PE toolchain has no such distinction and
        -- clang-cl would only report the flag as unused.
        linux  = { cxxflags = { "-fPIC" } },
        macosx = { cxxflags = { "-fPIC" } },
    },
}
