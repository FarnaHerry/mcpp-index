-- compat.glx-headers — GL and GLX API headers, from libglvnd.
--
-- Fills a real hole: `GL/glx.h` is NOT part of the Khronos OpenGL-Registry, so
-- `compat.opengl` cannot supply it (that package's `*/api` carries glcorearb.h,
-- glext.h, glxext.h, wgl.h — no glx.h). On Linux the canonical provider is
-- libglvnd, which is also the package the GL runtime plan already names as the
-- wanted dispatch provider (.agents/docs/2026-06-03-gl-runtime-packages-plan.md).
-- Only its headers are taken here; the dispatch libraries remain out of scope,
-- and `compat.glx-runtime` continues to own the runtime side.
--
-- `compat.sdl2` needs this: SDL's X11 video driver includes <GL/glx.h> from
-- SDL_x11opengl.h whenever SDL_VIDEO_OPENGL_GLX is on, and without it the whole
-- X11 backend fails to compile. (compat.glfw does not, because GLFW vendors its
-- own minimal GLX declarations in src/glx_context.h.)
--
-- Header-only, the `compat.opengl` shape: one include root plus an anchor TU so
-- the package still produces a buildable lib target.
--
-- NOTE ON OVERLAP: this include root also carries `GL/gl.h`, `GL/glcorearb.h`
-- and `GL/glext.h`, which `compat.opengl` provides too. Depend on ONE of the
-- two, not both, or the winner depends on include-dir order. Consumers wanting
-- GLX should take this package; consumers wanting only core GL should keep
-- taking `compat.opengl`.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "glx-headers",
    description = "GL and GLX API headers (libglvnd) — provides GL/glx.h",
    licenses    = {"MIT"},
    repo        = "https://github.com/NVIDIA/libglvnd",
    type        = "package",

    xpm = {
        linux = {
            ["1.7.0"] = {
                url = {
                    GLOBAL = "https://github.com/NVIDIA/libglvnd/archive/refs/tags/v1.7.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/glx-headers/releases/download/1.7.0/glx-headers-1.7.0.tar.gz",
                },
                sha256 = "073e7292788d4d3eeb45ea6c7bdcce9bfdb3b3eef8d7dbd47f2f30dce046ef98",
            },
        },
        macosx = {
            ["1.7.0"] = {
                url = {
                    GLOBAL = "https://github.com/NVIDIA/libglvnd/archive/refs/tags/v1.7.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/glx-headers/releases/download/1.7.0/glx-headers-1.7.0.tar.gz",
                },
                sha256 = "073e7292788d4d3eeb45ea6c7bdcce9bfdb3b3eef8d7dbd47f2f30dce046ef98",
            },
        },
        windows = {
            ["1.7.0"] = {
                url = {
                    GLOBAL = "https://github.com/NVIDIA/libglvnd/archive/refs/tags/v1.7.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/glx-headers/releases/download/1.7.0/glx-headers-1.7.0.tar.gz",
                },
                sha256 = "073e7292788d4d3eeb45ea6c7bdcce9bfdb3b3eef8d7dbd47f2f30dce046ef98",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "c11",
        include_dirs = { "*/include" },
        generated_files = {
            ["mcpp_generated/glx_headers_anchor.c"] =
                "int mcpp_compat_glx_headers_anchor(void) { return 0; }\n",
        },
        sources = { "mcpp_generated/glx_headers_anchor.c" },
        targets = { ["glx-headers"] = { kind = "lib" } },
        -- GL/glx.h includes <X11/Xlib.h> and <X11/Xutil.h>.
        deps = {
            ["compat.x11"]       = "1.8.13",
            ["compat.xorgproto"] = "2025.1",
        },
    },
}
