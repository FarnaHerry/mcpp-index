-- compat.protobuf — Protocol Buffers 35.1 C++ RUNTIME (libprotobuf), built
-- straight from the upstream release tarball as one static archive.
--
-- Shape A (C++ source compat). Three properties of the upstream release make
-- this possible without a configure step or an external build system:
--
--   1. protobuf-35.1.tar.gz is a real published release asset (not the GitHub
--      tag archive) and is self-contained: no submodules, and third_party/
--      carries utf8_range as actual source.
--   2. The 14 .pb.cc files for the well-known types AND descriptor.pb.cc are
--      CHECKED IN upstream, so building the runtime needs no protoc — there is
--      no bootstrap problem to solve here.
--   3. Platform handling is entirely in-source `#ifdef` (port_def.inc), so one
--      source list covers linux/macosx/windows and the three xpm blocks share
--      a single tarball and sha256.
--
-- SCOPE — runtime only. This package builds upstream's `libprotobuf` target
-- (79 TUs), i.e. what a program that *uses* generated code needs: messages,
-- reflection, descriptors, text/JSON formats, the well-known types. It does
-- NOT build `libprotoc` (a further 157 TUs) and ships no protoc binary, so it
-- does not generate .pb.cc from .proto. Consumers either check in
-- protoc-generated sources or build them with the official upstream protoc
-- release (protoc-35.1-<platform>.zip); wiring that into an mcpp build belongs
-- to a build.mcpp step, not to this descriptor.
--
-- Version numbering follows upstream verbatim: `35.1` is the protobuf release,
-- and it is what gRPC 1.83.0 pins (its third_party/protobuf submodule is
-- exactly tag v35.1, commit 35cd01f). The Abseil dependency is pinned to the
-- same LTS both agree on — protobuf's MODULE.bazel says `abseil-cpp
-- 20250512.1` and gRPC's submodule resolves to the same release — so a future
-- gRPC package links ONE Abseil rather than colliding with a second copy.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "protobuf",
    description = "Protocol Buffers — Google's data interchange format, C++ runtime (libprotobuf 35.1, static)",
    licenses    = {"BSD-3-Clause"},
    repo        = "https://github.com/protocolbuffers/protobuf",
    type        = "package",

    xpm = {
        linux = {
            ["35.1"] = {
                url = {
                    GLOBAL = "https://github.com/protocolbuffers/protobuf/releases/download/v35.1/protobuf-35.1.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/protobuf/releases/download/35.1/protobuf-35.1.tar.gz",
                },
                sha256 = "f0b6838e7522a8da96126d487068c959bc624926368f3024ac8fd03abd0a1ac4",
            },
        },
        macosx = {
            ["35.1"] = {
                url = {
                    GLOBAL = "https://github.com/protocolbuffers/protobuf/releases/download/v35.1/protobuf-35.1.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/protobuf/releases/download/35.1/protobuf-35.1.tar.gz",
                },
                sha256 = "f0b6838e7522a8da96126d487068c959bc624926368f3024ac8fd03abd0a1ac4",
            },
        },
        windows = {
            ["35.1"] = {
                url = {
                    GLOBAL = "https://github.com/protocolbuffers/protobuf/releases/download/v35.1/protobuf-35.1.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/protobuf/releases/download/35.1/protobuf-35.1.tar.gz",
                },
                sha256 = "f0b6838e7522a8da96126d487068c959bc624926368f3024ac8fd03abd0a1ac4",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "c11",       -- for third_party/utf8_range/utf8_range.c

        -- `*/src` carries the public <google/protobuf/…> headers;
        -- `*/third_party/utf8_range` carries utf8_range.h + utf8_validity.h,
        -- which protobuf's own TUs include unqualified.
        include_dirs = { "*/src", "*/third_party/utf8_range" },

        -- Transcribed from upstream's own authoritative list — the
        -- `libprotobuf_srcs` set in src/file_lists.cmake, which upstream
        -- auto-generates from its Bazel rules. Listed file-by-file rather than
        -- globbed on purpose: `src/google/protobuf/**/*.cc` would also sweep in
        -- libprotoc and the unit tests, and two of the runtime's own TUs live
        -- under compiler/ (importer.cc, parser.cc — the .proto text parser is
        -- part of the runtime), so no directory-level glob separates the two
        -- libraries cleanly. libprotobuf_lite_srcs is a strict subset of this
        -- list, so the lite variant needs no separate handling.
        sources = {
            -- well-known types + descriptor: generated, but CHECKED IN upstream
            "*/src/google/protobuf/any.pb.cc",
            "*/src/google/protobuf/api.pb.cc",
            "*/src/google/protobuf/duration.pb.cc",
            "*/src/google/protobuf/empty.pb.cc",
            "*/src/google/protobuf/field_mask.pb.cc",
            "*/src/google/protobuf/source_context.pb.cc",
            "*/src/google/protobuf/struct.pb.cc",
            "*/src/google/protobuf/timestamp.pb.cc",
            "*/src/google/protobuf/type.pb.cc",
            "*/src/google/protobuf/wrappers.pb.cc",
            "*/src/google/protobuf/cpp_features.pb.cc",
            "*/src/google/protobuf/descriptor.pb.cc",
            -- runtime core
            "*/src/google/protobuf/any.cc",
            "*/src/google/protobuf/any_lite.cc",
            "*/src/google/protobuf/arena.cc",
            "*/src/google/protobuf/arena_align.cc",
            "*/src/google/protobuf/arenastring.cc",
            "*/src/google/protobuf/arenaz_sampler.cc",
            "*/src/google/protobuf/compiler/importer.cc",
            "*/src/google/protobuf/compiler/parser.cc",
            "*/src/google/protobuf/descriptor.cc",
            "*/src/google/protobuf/descriptor_database.cc",
            "*/src/google/protobuf/dynamic_message.cc",
            "*/src/google/protobuf/extension_set.cc",
            "*/src/google/protobuf/extension_set_heavy.cc",
            "*/src/google/protobuf/feature_resolver.cc",
            "*/src/google/protobuf/generated_enum_util.cc",
            "*/src/google/protobuf/generated_message_bases.cc",
            "*/src/google/protobuf/generated_message_reflection.cc",
            "*/src/google/protobuf/generated_message_tctable_full.cc",
            "*/src/google/protobuf/generated_message_tctable_gen.cc",
            "*/src/google/protobuf/generated_message_tctable_lite.cc",
            "*/src/google/protobuf/generated_message_util.cc",
            "*/src/google/protobuf/implicit_weak_message.cc",
            "*/src/google/protobuf/inlined_string_field.cc",
            "*/src/google/protobuf/internal_feature_helper.cc",
            "*/src/google/protobuf/map.cc",
            "*/src/google/protobuf/map_field.cc",
            "*/src/google/protobuf/message.cc",
            "*/src/google/protobuf/message_lite.cc",
            "*/src/google/protobuf/micro_string.cc",
            "*/src/google/protobuf/parse_context.cc",
            "*/src/google/protobuf/port.cc",
            "*/src/google/protobuf/raw_ptr.cc",
            "*/src/google/protobuf/reflection_mode.cc",
            "*/src/google/protobuf/reflection_ops.cc",
            "*/src/google/protobuf/repeated_field.cc",
            "*/src/google/protobuf/repeated_ptr_field.cc",
            "*/src/google/protobuf/service.cc",
            "*/src/google/protobuf/stubs/common.cc",
            "*/src/google/protobuf/symbol_checker.cc",
            "*/src/google/protobuf/text_format.cc",
            "*/src/google/protobuf/unknown_field_set.cc",
            "*/src/google/protobuf/wire_format.cc",
            "*/src/google/protobuf/wire_format_lite.cc",
            -- io
            "*/src/google/protobuf/io/coded_stream.cc",
            "*/src/google/protobuf/io/gzip_stream.cc",   -- empty TU unless the `gzip` feature is on
            "*/src/google/protobuf/io/io_win32.cc",
            "*/src/google/protobuf/io/printer.cc",
            "*/src/google/protobuf/io/strtod.cc",
            "*/src/google/protobuf/io/tokenizer.cc",
            "*/src/google/protobuf/io/zero_copy_sink.cc",
            "*/src/google/protobuf/io/zero_copy_stream.cc",
            "*/src/google/protobuf/io/zero_copy_stream_impl.cc",
            "*/src/google/protobuf/io/zero_copy_stream_impl_lite.cc",
            -- json
            "*/src/google/protobuf/json/json.cc",
            "*/src/google/protobuf/json/internal/lexer.cc",
            "*/src/google/protobuf/json/internal/message_path.cc",
            "*/src/google/protobuf/json/internal/parser.cc",
            "*/src/google/protobuf/json/internal/unparser.cc",
            "*/src/google/protobuf/json/internal/untyped_message.cc",
            "*/src/google/protobuf/json/internal/writer.cc",
            "*/src/google/protobuf/json/internal/zero_copy_buffered_stream.cc",
            -- util
            "*/src/google/protobuf/util/delimited_message_util.cc",
            "*/src/google/protobuf/util/field_comparator.cc",
            "*/src/google/protobuf/util/field_mask_util.cc",
            "*/src/google/protobuf/util/message_differencer.cc",
            "*/src/google/protobuf/util/time_util.cc",
            "*/src/google/protobuf/util/type_resolver_util.cc",

            -- utf8_range: upstream's CMakeLists builds BOTH the `utf8_range`
            -- and `utf8_validity` targets from this one file. Every other .c
            -- in that directory (naive/lookup/lemire-*/range-*) is a
            -- benchmark alternative, and main.c is a benchmark driver whose
            -- main() would collide with the consumer's.
            "*/third_party/utf8_range/utf8_range.c",
        },

        targets = { ["protobuf"] = { kind = "lib" } },

        -- protobuf's public headers #include "absl/…" directly, so Abseil is
        -- part of this package's interface, not an implementation detail.
        deps = { ["compat.abseil"] = "20250512.1" },

        features = {
            -- GzipInputStream / GzipOutputStream. io/gzip_stream.cc is wrapped
            -- head-to-toe in `#if HAVE_ZLIB`, so by default it compiles to an
            -- empty TU and the package carries no zlib dependency at all;
            -- turning the feature on defines the macro for protobuf's own TUs
            -- and pulls the provider. (`defines` reaches only this package's
            -- TUs, which is exactly the scope needed — the switch is read by
            -- gzip_stream.cc, never by consumer code.)
            ["gzip"] = {
                defines = { "HAVE_ZLIB=1" },
                deps    = { ["compat.zlib"] = "1.3.2" },
            },
        },

        linux = {
            ldflags = { "-lpthread" },
        },
        -- macOS: libSystem carries pthread, and the CoreFoundation framework
        -- the time zone lookup needs comes in through compat.abseil.
        windows = {
            -- Same <windows.h> min/max macro hazard as compat.abseil: protobuf
            -- reaches <windows.h> through io/io_win32.cc and port.h while using
            -- std::min/std::max throughout. Upstream's CMake does not spell
            -- NOMINMAX out because its Abseil dependency's copts already do;
            -- here each package carries its own compile flags, so it has to be
            -- stated. No extra import libs: -ladvapi32 arrives with abseil.
            cxxflags = { "-DNOMINMAX", "-DWIN32_LEAN_AND_MEAN", "-D_CRT_SECURE_NO_WARNINGS" },
        },
    },
}
