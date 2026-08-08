-- compat.gmp — GNU Multiple Precision Arithmetic Library 6.3.0, built from
-- source as a static C library (libgmp.a + gmp.h).
--
-- GMP builds through GNU configure + make, which does not fit mcpp's "list the
-- .c files" model: configure generates gmp.h / config.h / gmp-mparam.h from
-- the host probes at build time. So, like compat.openssl and compat.openblas,
-- the xpkg install() hook runs the upstream build and lays the lib + headers
-- under the install dir. mcpp compiles the anchor TU that install() emits, and
-- links the archive via -Llib.
--
-- Configure flags:
--   * --disable-shared --enable-static — static-only archive. (The consumer
--     example links with -l:libgmp.a on linux, so a stray host libgmp.so.10
--     cannot be picked up.)
--   * --disable-assembly — portable generic-C kernels. Same tradeoff as
--     compat.openblas's TARGET=GENERIC: correctness/portability first, at the
--     cost of the hand-tuned asm speedups (a later pass could re-enable asm
--     per-platform; mcpp's feature table cannot select configure flags, so
--     this stays a build-time decision).
--
-- Platforms: linux + macosx. Windows is deferred: GMP has no supported MSVC
-- build path and no official prebuilt binaries (vcpkg uses the MPIR fork or
-- MinGW builds there), so there is no windows xpm entry — the member test
-- carries no dependency and compiles to a no-op main() on windows.
--
-- Mirror: no CN mirror yet (no authorized mcpp-res write), so the url is the
-- plain upstream string form; maintainers can later rewrite it to
-- { GLOBAL=…, CN=… } with the same sha256. GLOBAL points at ftp.gnu.org — the
-- authoritative release tarball (a GitHub snapshot would lack `configure`).
--
-- License: dual LGPL-3.0-or-later OR GPL-2.0-or-later (tarball README); the
-- permissive side is declared here.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "gmp",
    description = "GMP — GNU multiple precision arithmetic library (static, install()-driven build)",
    licenses    = {"LGPL-3.0-or-later"},
    repo        = "https://gmplib.org/",
    type        = "package",

    xpm = {
        linux = {
            -- glibc + linux-headers are here for the same reason they are on
            -- compat.openssl: GMP builds through its own Makefile with a bare
            -- `cc`, so every tool it uses has to be something this descriptor
            -- resolved, not something it hopes to find. See cc_override().
            deps = {
                "xim:make@latest",
                "xim:glibc@>=2.39", "xim:linux-headers@5.11.1",
            },
            ["6.3.0"] = {
                url     = "https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.gz",
                sha256  = "e56fd59d76810932a0555aa15a14b61c16bed66110d3c75cc2ac49ddaa9ab24c",
            },
        },
        macosx = {
            -- No xim:make here: that package is linux-only (compat.openblas
            -- declares it on macosx and is broken the same way); resolve_make()
            -- falls back to PATH (macOS ships GNU Make 3.81, which GMP accepts).
            ["6.3.0"] = {
                url     = "https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.gz",
                sha256  = "e56fd59d76810932a0555aa15a14b61c16bed66110d3c75cc2ac49ddaa9ab24c",
            },
        },
        -- windows deferred (no MSVC build path upstream)
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "c11",
        -- Anchor TU is NOT a generated_files entry: it is emitted by install()
        -- so mcpp must run install() (which also builds the lib) before it can
        -- compile this source. include/ + lib/ are produced by `make install`.
        sources      = { "mcpp_gmp_anchor.c" },
        targets      = { ["gmp"] = { kind = "lib" } },
        include_dirs = { "include" },
        deps         = { },

        -- libgmp.a pulls in libm (mpf layer uses sqrt/log/exp); libSystem on
        -- macOS already carries it, glibc does not.
        linux  = { ldflags = { "-Llib", "-l:libgmp.a", "-lm" } },
        -- ld64 has no `-l:` spelling; lib/ holds only our archive.
        macosx = { ldflags = { "-Llib", "-lgmp" } },
    },
}

import("xim.libxpkg.pkginfo")
import("xim.libxpkg.log")

local function sh_quote(value)
    return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local function resolve_make()
    local mk = pkginfo.build_dep("xim:make") or pkginfo.build_dep("make")
    if mk and mk.bin then
        local cand = path.join(mk.bin, "make")
        if os.isfile(cand) then return cand end
    end
    -- No build dep (macOS): prefer a Homebrew `gmake` when present. macOS's own
    -- /usr/bin/make is GNU Make 3.81, which GMP still accepts (needs >= 3.80).
    if os.host() == "macosx" and os.isfile("/opt/homebrew/bin/gmake") then
        return "/opt/homebrew/bin/gmake"
    end
    return "make"
end

-- The C compiler GMP's own build should use. GMP is configured and built
-- outside mcpp's compile rules, so it does not inherit the resolved
-- toolchain's sysroot flags — it just runs `cc`.
--
-- On macOS the toolchain in PATH is xim's llvm, which has no macOS SDK wired
-- up, so every compile would fail on <stdio.h>. Pin Apple's own driver, which
-- finds the SDK by itself.
--
-- On linux, same story as compat.openssl: PATH reaches the xim gcc through its
-- xvm shim, which injects `--sysroot=<subos>` resolved against the CONSUMING
-- project's subos — an empty tree. Hand the payload gcc the three things it
-- needs (headers via -isystem, crt objects via -B, and -lc via -L) from the
-- declared glibc/linux-headers build deps.
local function libc_payloads()
    local glibc = pkginfo.build_dep("xim:glibc") or pkginfo.build_dep("glibc")
    local kern  = pkginfo.build_dep("xim:linux-headers")
                  or pkginfo.build_dep("linux-headers")
    local groot = glibc and glibc.path
    local kroot = kern and kern.path
    if not (groot and os.isfile(path.join(groot, "include", "stdlib.h"))) then
        return nil
    end
    return groot, kroot
end

local function cc_override()
    if os.host() == "macosx" and os.isfile("/usr/bin/cc") then
        return "CC=/usr/bin/cc "
    end
    if os.host() ~= "linux" then return "" end

    local groot, kroot = libc_payloads()
    if not groot then
        log.warn("gmp: no xim:glibc payload resolved; leaving CC to PATH"
            .. " (fails if the active subos carries no libc headers)")
        return ""
    end
    local libdir = os.isdir(path.join(groot, "lib64"))
                   and path.join(groot, "lib64") or path.join(groot, "lib")
    local cc = "gcc --sysroot=" .. groot
        .. " -isystem " .. path.join(groot, "include")
    if kroot and os.isdir(path.join(kroot, "include")) then
        cc = cc .. " -isystem " .. path.join(kroot, "include")
    end
    cc = cc .. " -B " .. libdir .. " -L " .. libdir
    return "CC=" .. sh_quote(cc) .. " "
end

-- Last `n` lines of the build log, or nil if it cannot be read.
local function tail_lines(file, n)
    local ok, content = pcall(io.readfile, file)
    if not ok or not content then return nil end
    local lines = {}
    for line in (tostring(content) .. "\n"):gmatch("(.-)\n") do
        lines[#lines + 1] = line
    end
    if #lines == 0 then return nil end
    return table.concat(lines, "\n", math.max(1, #lines - n + 1), #lines)
end

-- Run one build step, and on failure print the tail of the log with it.
-- Everything the build says goes to an on-disk log (xim's interface mode
-- swallows subprocess stdout), and xlings surfaces a failed install() as a
-- bare `E_INTERNAL` — so without this the only signal a CI run gives is that
-- something, somewhere, went wrong.
local function run(step, logf, cmd)
    local ok, err = pcall(os.exec, string.format("bash -c %s", sh_quote(cmd)))
    if ok then return true end
    local tail = tail_lines(logf, 40) or "<log unreadable at " .. tostring(logf) .. ">"
    log.error("%s", "compat.gmp: " .. step .. " failed (" .. tostring(err)
                 .. ")\n--- last 40 lines of " .. tostring(logf) .. " ---\n" .. tail)
    return false
end

local function _install_impl()
    -- The fetched tarball unpacks to gmp-6.3.0/ beside the archive.
    local ifile   = pkginfo.install_file()
    local srcroot = ifile and tostring(ifile):replace(".tar.gz", "")
                            or ("gmp-" .. pkginfo.version())
    if not os.isdir(srcroot) then
        srcroot = "gmp-" .. pkginfo.version()
    end

    -- The prefix is emptied HERE, before the build, so the build log can live
    -- inside it: a log that a later os.tryrm would delete, or one left in the
    -- transient srcroot, is gone exactly when a failed build needs reading.
    local prefix = pkginfo.install_dir()
    os.tryrm(prefix)
    os.mkdir(prefix)
    local logf = path.join(prefix, "mcpp_gmp_build.log")

    local make  = resolve_make()
    local jobs  = (os.default_njob and os.default_njob()) or 4
    -- --disable-assembly: generic C kernels (portable, no m4/gas needed).
    -- --libdir=<prefix>/lib: unlike OpenSSL, GMP's configure REJECTS a
    -- relative --libdir ("expected an absolute directory name"); it also
    -- derives lib64 on some linux ABIs, so pinning an absolute path also
    -- keeps the descriptor's -Llib honest.
    local flags = "--disable-shared --enable-static --disable-assembly"
               .. " --libdir=" .. path.join(prefix, "lib")

    local cc = cc_override()
    if not run("./configure", logf, string.format(
        "cd %s && %s./configure --prefix=%s %s >> %s 2>&1",
        sh_quote(srcroot), cc, sh_quote(prefix), flags, sh_quote(logf))) then
        return false
    end
    if not run("make", logf, string.format(
        "cd %s && %s -j%d >> %s 2>&1",
        sh_quote(srcroot), make, jobs, sh_quote(logf))) then
        return false
    end
    if not run("make install", logf, string.format(
        "cd %s && %s install >> %s 2>&1",
        sh_quote(srcroot), make, sh_quote(logf))) then
        return false
    end

    -- Verify the build produced the expected archive and header.
    local libdir = path.join(prefix, "lib")
    local gmp_a  = path.join(libdir, "libgmp.a")
    local gmp_h  = path.join(prefix, "include", "gmp.h")
    if not os.isfile(gmp_a) or not os.isfile(gmp_h) then
        log.error("compat.gmp: build produced no libgmp.a / include/gmp.h "
               .. "under %s (see %s)", prefix, logf)
        return false
    end

    -- Emit the anchor TU mcpp compiles. Its absence after extraction is what
    -- makes mcpp run this install() before the build (same trigger as
    -- compat.openblas / compat.openssl / compat.xcb).
    io.writefile(path.join(prefix, "mcpp_gmp_anchor.c"),
                 "int mcpp_compat_gmp_anchor(void) { return 0; }\n")
    return true
end

function install()
    -- Windows is deferred: there is no windows xpm block, so version
    -- resolution already fails before this point. Kept as a named error in
    -- case a windows entry is added before this hook learns to build there.
    if os.host() == "windows" then
        log.error("compat.gmp: windows is not yet supported")
        return false
    end
    local ok, result = pcall(_install_impl)
    if not ok then
        log.error("compat.gmp install() failed: %s", tostring(result))
        return false
    end
    return true
end
