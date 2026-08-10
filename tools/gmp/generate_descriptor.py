#!/usr/bin/env python3
"""Regenerate pkgs/c/compat.gmp.lua from the upstream GMP tarball.

compat.gmp has no install() hook: mcpp compiles GMP's portable C kernels
straight from the tarball.  Two things GMP's own build does cannot be
expressed in a descriptor -- it substitutes gmp.h from gmp-h.in, and it
COMPILES AND RUNS seven table generators -- so their output is produced here,
once, and vendored into the descriptor's `generated_files`.

Both are pure functions of (GMP_LIMB_BITS=64, GMP_NAIL_BITS=0), which is why
this is reproducible rather than a snapshot of somebody's machine: the tables
are byte-for-byte what upstream's own `gen-* 64 0` prints.  Same shape as
tools/godot-cpp/repack.sh -- vendored bytes come with the script that makes
them again.

    python3 tools/gmp/generate_descriptor.py write     # rewrite the descriptor
    python3 tools/gmp/generate_descriptor.py check     # verify it is in sync
    python3 tools/gmp/generate_descriptor.py stage DIR # lay out a verdir, for
                                                       # compiling by hand

`check` is the one to run in review: it regenerates into memory and diffs, so
a descriptor edited by hand instead of through this script is visible.

Needs: a C compiler (for the generators), curl or a cached tarball, tar.
"""
import hashlib
import os
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

VER    = "6.3.0"
WRAP   = "gmp-" + VER
TARBALL = WRAP + ".tar.gz"
URL    = "https://ftp.gnu.org/gnu/gmp/" + TARBALL
SHA256 = "e56fd59d76810932a0555aa15a14b61c16bed66110d3c75cc2ac49ddaa9ab24c"

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
DESCRIPTOR = os.path.join(REPO, "pkgs", "c", "compat.gmp.lua")

# Filled in by prepare(): the unpacked tree, and where the generators wrote.
SRC = None
GEN = None

# (generator, output, argv) -- upstream's Makefile.am recipes, verbatim.
GENERATORS = [
    ("gen-fac",         "fac_table.h",   ["64", "0"]),
    ("gen-sieve",       "sieve_table.h", ["64"]),
    ("gen-fib",         "fib_table.h",   ["header", "64", "0"]),
    ("gen-fib",         "fib_table.c",   ["table",  "64", "0"]),
    ("gen-bases",       "mp_bases.h",    ["header", "64", "0"]),
    ("gen-bases",       "mp_bases.c",    ["table",  "64", "0"]),
    ("gen-trialdivtab", "trialdivtab.h", ["64", "8000"]),
    ("gen-jacobitab",   "jacobitab.h",   []),
    ("gen-psqr",        "perfsqr.h",     ["64", "0"]),
]


def prepare(workdir):
    """Fetch + unpack the tarball and run upstream's table generators."""
    global SRC, GEN
    cache = os.environ.get("GMP_TARBALL") or os.path.join(workdir, TARBALL)
    if not os.path.exists(cache):
        print("fetching %s" % URL, file=sys.stderr)
        urllib.request.urlretrieve(URL, cache)
    digest = hashlib.sha256(open(cache, "rb").read()).hexdigest()
    if digest != SHA256:
        raise SystemExit("sha256 mismatch for %s:\n  got      %s\n  expected %s"
                         % (cache, digest, SHA256))

    with tarfile.open(cache) as tf:
        # `filter` landed in 3.12 and becomes the default in 3.14.
        if hasattr(tarfile, "data_filter"):
            tf.extractall(workdir, filter="data")
        else:
            tf.extractall(workdir)
    SRC = os.path.join(workdir, WRAP)

    GEN = os.path.join(workdir, "gen")
    os.makedirs(GEN, exist_ok=True)
    cc = os.environ.get("CC", "cc")
    built = {}
    for gen, out, argv in GENERATORS:
        if gen not in built:
            exe = os.path.join(GEN, gen)
            # Each gen-*.c #includes bootstrap.c -> mini-gmp/mini-gmp.c, so the
            # source tree is its whole include closure. gen-bases wants libm
            # (upstream links it with $(LIBM_FOR_BUILD)).
            cmd = [cc, "-O2", "-I", SRC, os.path.join(SRC, gen + ".c"), "-o", exe]
            if sys.platform != "win32":
                cmd.append("-lm")
            subprocess.run(cmd, check=True)
            built[gen] = exe
        with open(os.path.join(GEN, out), "wb") as fh:
            subprocess.run([built[gen]] + argv, check=True, stdout=fh)
    print("generated %d tables with %s" % (len(GENERATORS), cc), file=sys.stderr)

# --------------------------------------------------------------------------
# config.h -- upstream's is produced by 200-odd configure probes.  Everything
# GMP actually asks about is either guaranteed by C89/C99, or something we
# deliberately answer "no" to so that one header serves every platform.
# --------------------------------------------------------------------------
CONFIG_H = r'''/* config.h for GNU MP 6.3.0 -- written for mcpp-index's compat.gmp.

   Upstream generates this from ~200 configure probes.  compat.gmp has no
   configure step, so the answers are decided here instead -- and the whole
   file is deliberately platform-INDEPENDENT: every HAVE_* below is either
   guaranteed by C89/C99 on any hosted implementation, or answered "no" on
   purpose so that linux, macOS and Windows compile the identical library.

   The ones answered "no" on purpose, and what it costs:

     HAVE_UNISTD_H      only guards <unistd.h> for getpid(), which invalid.c
                        needs solely as a fallback when raise() is missing.
                        raise() is C89.
     HAVE_LANGINFO_H    nl_langinfo(RADIXCHAR) is one of two ways GMP finds
                        the locale's decimal point; localeconv() is the other
                        and is C89.  Reaching for the POSIX one under a
                        strict -std=c11 (which is what mcpp compiles C with)
                        is how you get an implicit declaration.
     HAVE_SYS_TYPES_H   only guards the <sys/types.h> include for quad_t.
     HAVE_QUAD_T        drops gmp_printf/gmp_scanf's BSD %q length modifier.
                        glibc only declares quad_t under __USE_MISC, which
                        -std=c11 turns off, so "yes" is not portable anyway.
     HAVE_OBSTACK_VPRINTF   drops gmp_obstack_printf / gmp_obstack_vprintf.
                        glibc-only, and its declaration needs _GNU_SOURCE.
     HAVE_ALLOCA_H      unnecessary: gmp-impl.h reaches __builtin_alloca on
                        GCC/Clang before it ever looks at <alloca.h>, and
                        mcpp ships nothing else.
     HAVE_HIDDEN_ALIAS  an ELF-only link-time optimisation; wrong on Mach-O
                        and PE.
     NO_ASM             NOT defined, i.e. longlong.h's inline asm primitives
                        (umul_ppmm / add_ssaaaa / count_leading_zeros) stay
                        ON.  They are selected by compiler predefines and
                        fall back to C on their own; upstream's
                        --disable-assembly switches them off along with the
                        hand-written kernels, which costs several times the
                        throughput on the multiply/divide path for no
                        portability gain.
     HAVE_NATIVE_mpn_*  never defined: this package compiles mpn/generic,
                        not the per-CPU .asm kernels (those need m4).
     WANT_FAT_BINARY / WANT_ASSERT / WANT_PROFILING   off, as upstream. */

#ifndef __GMP_CONFIG_H__
#define __GMP_CONFIG_H__

#include <limits.h>

/* The vendored tables (fac_table.h, fib_table.h, sieve_table.h, mp_bases.h,
   trialdivtab.h, perfsqr.h, jacobitab.h) were generated for
   GMP_LIMB_BITS = 64, GMP_NAIL_BITS = 0.  A target with no 64-bit unsigned
   type would otherwise take them silently and compute wrong answers, so it
   has to stop here.  The same test picks the limb type in gmp.h. */
#if !(ULONG_MAX >> 31 >> 31 >> 1 == 1) && \
    !(defined(ULLONG_MAX) && ULLONG_MAX >> 31 >> 31 >> 1 == 1)
# error "compat.gmp: 64-bit targets only (the vendored tables are limb=64)."
#endif

#define PACKAGE_NAME      "GNU MP"
#define PACKAGE_TARNAME   "gmp"
#define PACKAGE_VERSION   "6.3.0"
#define PACKAGE_STRING    "GNU MP 6.3.0"
#define PACKAGE_BUGREPORT "gmp-bugs@gmplib.org (see https://gmplib.org/manual/Reporting-Bugs.html)"
#define VERSION           "6.3.0"

#define GMP_LIMB_BITS    64
#define GMP_NAIL_BITS    0
#define SIZEOF_MP_LIMB_T 8

/* Taken from the compiler rather than from a probe: exact by construction,
   and identical on every target mcpp builds for. */
#define SIZEOF_UNSIGNED       __SIZEOF_INT__
#define SIZEOF_UNSIGNED_SHORT __SIZEOF_SHORT__
#define SIZEOF_UNSIGNED_LONG  __SIZEOF_LONG__
#define SIZEOF_VOID_P         __SIZEOF_POINTER__

#define STDC_HEADERS 1

/* Headers and functions guaranteed by C89/C99. */
#define HAVE_STDLIB_H   1
#define HAVE_STRING_H   1
#define HAVE_FLOAT_H    1
#define HAVE_LOCALE_H   1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H   1
#define HAVE_LOCALECONV 1
#define HAVE_MEMSET     1
#define HAVE_STRCHR     1
#define HAVE_STRERROR   1
#define HAVE_STRTOL     1
#define HAVE_STRTOUL    1
#define HAVE_RAISE      1
#define HAVE_VSNPRINTF  1
#define HAVE_STRNLEN    1
#define HAVE_LONG_LONG  1
#define HAVE_LONG_DOUBLE 1
#define HAVE_INTMAX_T   1
#define HAVE_INTPTR_T   1
#define HAVE_PTRDIFF_T  1
#define HAVE_UINT_LEAST32_T 1

/* C89 declarations GMP double-checks because a few pre-standard libcs
   omitted them. */
#define HAVE_DECL_FGETC    1
#define HAVE_DECL_FSCANF   1
#define HAVE_DECL_UNGETC   1
#define HAVE_DECL_VFPRINTF 1

/* Schoenhage-Strassen multiplication, as in a default upstream build. */
#define WANT_FFT 1

#if defined(__GNUC__)
# define HAVE_ATTRIBUTE_CONST    1
# define HAVE_ATTRIBUTE_MALLOC   1
# define HAVE_ATTRIBUTE_MODE     1
# define HAVE_ATTRIBUTE_NORETURN 1
#endif

/* TMP_ALLOC on the stack.  gmp-impl.h resolves `alloca` to __builtin_alloca
   under __GNUC__ and to _alloca under _MSC_VER before it ever needs
   <alloca.h>; anything else falls back to the malloc-based strategy. */
#if defined(__GNUC__) || defined(_MSC_VER)
# define HAVE_ALLOCA     1
# define WANT_TMP_ALLOCA 1
#else
# define WANT_TMP_REENTRANT 1
#endif

/* Byte order of a `double' and of a multi-limb value. */
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define HAVE_DOUBLE_IEEE_BIG_ENDIAN 1
# define HAVE_LIMB_BIG_ENDIAN        1
#else
# define HAVE_DOUBLE_IEEE_LITTLE_ENDIAN 1
# define HAVE_LIMB_LITTLE_ENDIAN        1
#endif

/* CPU family.  This unlocks exactly two things in the generic sources: the
   add_mssaaaa inline asm in mod_1_1.c / div_qr_1n_pi1.c, and the tighter
   MPN_IORD_U in gmp-impl.h.  Both are additionally gated on __GNUC__ and
   !NO_ASM, so a target that is neither keeps the C path. */
#if defined(__x86_64__) || defined(__amd64__)
# define HAVE_HOST_CPU_FAMILY_x86_64 1
#elif defined(__i386__)
# define HAVE_HOST_CPU_FAMILY_x86 1
#endif

/* Assembler local-label prefix, used by the MPN_IORD_U asm above.  Mach-O
   private labels are `L...`, ELF and COFF `.L...`.  Assembler syntax, not
   compiler syntax -- getting it wrong is a loud assembler error. */
#if defined(__APPLE__)
# define LSYM_PREFIX "L"
#else
# define LSYM_PREFIX ".L"
#endif

#ifndef __cplusplus
# define restrict __restrict
#endif

#endif /* __GMP_CONFIG_H__ */
'''

# --------------------------------------------------------------------------
# gmp.h -- gmp-h.in with configure's eight substitutions applied.
#
# Upstream bakes the limb type in at configure time.  Here it is decided by
# the preprocessor from <limits.h>, which gmp-h.in has already included at
# that point: gmp.h and the compiled library then cannot disagree about
# _LONG_LONG_LIMB, which is exactly the mistake an LP64-baked header makes
# when it reaches a consumer on LLP64 Windows.
# --------------------------------------------------------------------------
DEFN_LONG_LONG_LIMB = r'''/* Instantiated by mcpp-index's compat.gmp.
   Upstream's configure bakes the limb type in; deciding it here from
   <limits.h> (included just above) means this header can never disagree
   with the library about the width of mp_limb_t -- LP64 (linux, macOS)
   takes `unsigned long', LLP64 (windows) `unsigned long long'. */
#if ULONG_MAX >> 31 >> 31 >> 1 == 1
/* unsigned long is 64-bit: mp_limb_t stays unsigned long. */
#elif defined (ULLONG_MAX) && ULLONG_MAX >> 31 >> 31 >> 1 == 1
#define _LONG_LONG_LIMB 1
#else
#error "compat.gmp: 64-bit targets only (no 64-bit unsigned integer type)."
#endif'''

GMP_H_SUBS = {
    "@HAVE_HOST_CPU_FAMILY_power@":   "0",
    "@HAVE_HOST_CPU_FAMILY_powerpc@": "0",
    "@GMP_LIMB_BITS@":                "64",
    "@GMP_NAIL_BITS@":                "0",
    "@DEFN_LONG_LONG_LIMB@":          DEFN_LONG_LONG_LIMB,
    "@LIBGMP_DLL@":                   "0",
    "@CC@":                           "mcpp",
    "@CFLAGS@":                       "",
}

# --------------------------------------------------------------------------
# One patch to gmp.h beyond configure's substitutions.
#
# gmp.h picks how to spell "inline" through a vendor cascade.  About a dozen
# small functions (mpz_get_ui, mpn_add, mpn_cmp, ...) are then defined in the
# header under __GMP_EXTERN_INLINE, AND out of line by the one TU that
# compiles with -D__GMP_FORCE_<fn> (mpz/get_ui.c, mpn/generic/add.c, ...).
# That only works if the header's copy never becomes a standalone symbol.
# The GCC branch guarantees it with
# `extern __inline__ __attribute__ ((__gnu_inline__))`.
#
# The `_MSC_VER` branch answers plain `__inline`, whose MS semantics DO emit
# an external definition in every TU that includes the header -- and it is the
# only branch in the cascade without a `! defined (__GMP_EXTERN_INLINE)` guard
# (SunPro and SCO both have one).
#
# This index compiles Windows with clang targeting the MSVC ABI, and that
# compiler defines _MSC_VER but NOT __GNUC__ (measured: guarding the branch on
# `! defined (__GNUC__)` changed nothing), so it lands on the MSVC answer and
# the link dies on ten duplicate symbols:
#
#   lld-link: error: duplicate symbol: __gmpz_get_ui
#   >>> defined at gmp.h:1800  obj/.../mpz/pprime_p.o
#   >>> defined at            obj/.../mpz/get_ui.o
#
# Fix: `static __inline`, which is what GMP itself answers for the other two
# compilers whose "extern inline" leaks a global (DEC C and SCO OpenUNIX).
# Non-forced TUs get a file-local copy that still inlines; the forced TU is
# unaffected because gmp.h suppresses __GMP_EXTERN_INLINE there by hand
# (`#if ! defined (__GMP_FORCE_<fn>)`), so the external definition still comes
# from exactly one object.  The added guard keeps a future GNU-compatible
# compiler that also sets _MSC_VER on the gnu_inline answer.
#
# Upstream never hit any of this: it has no MSVC-ABI build at all.
GMP_H_PATCHES = [(
    """/* Microsoft's C compiler accepts __inline */
#ifdef _MSC_VER
#define __GMP_EXTERN_INLINE  __inline
#endif
""",
    """/* Microsoft's C compiler accepts __inline */
/* mcpp-index (compat.gmp): `static __inline`, not `__inline`, and guarded.
   Plain `__inline` has MS inline semantics -- the header's copy of every
   __GMP_EXTERN_INLINE function becomes an external definition in EVERY TU
   that includes gmp.h, which collides with the out-of-line copy the
   -D__GMP_FORCE_<fn> TU emits (ten `lld-link: duplicate symbol` errors on
   clang targeting the MSVC ABI, which defines _MSC_VER but not __GNUC__).
   `static __inline` is the answer GMP already gives for the other compilers
   whose "extern inline" leaks a global (DEC C, SCO OpenUNIX): the header's
   copy stays file-local and still inlines, while the forced TU -- where
   gmp.h suppresses this macro by hand -- remains the single external
   definition. The guard mirrors the SunPro/SCO branches, so a GNU-compatible
   compiler that also sets _MSC_VER keeps the gnu_inline answer above. */
#if defined (_MSC_VER) && ! defined (__GMP_EXTERN_INLINE)
#define __GMP_EXTERN_INLINE  static __inline
#endif
""")]

# --------------------------------------------------------------------------
# The mulfunc sources: one file compiled once per OPERATION_* macro, each
# pass defining a different mpn entry point.  Upstream spells this with a
# symlink per operation inside mpn/; here each gets a wrapper TU.
# --------------------------------------------------------------------------
MULFUNC = {
    "logops_n":    ["and_n", "andn_n", "nand_n", "ior_n", "iorn_n",
                    "nior_n", "xor_n", "xnor_n"],
    "popham":      ["popcount", "hamdist"],
    "sec_aors_1":  ["sec_add_1", "sec_sub_1"],
    "sec_div":     ["sec_div_qr", "sec_div_r"],
    "sec_pi1_div": ["sec_pi1_div_qr", "sec_pi1_div_r"],
}

# mpn/generic sources upstream does NOT put in libmpn for this configuration
# (verified against the .lo set of a configured 6.3.0 tree).
MPN_EXCLUDE = ["udiv_w_sdiv", "div_qr_1n_pi2", "div_qr_1u_pi2"] + list(MULFUNC)

# Root-level sources, i.e. the 16 upstream compiles.  mp_clz_tab.c is absent
# on purpose -- it is compiled through a wrapper that forces the table on.
ROOT_SRC = ["assert", "compat", "errno", "extract-dbl", "invalid", "memory",
            "mp_bpl", "mp_dv_tab", "mp_get_fns", "mp_minv_tab", "mp_set_fns",
            "nextprime", "primesieve", "tal-reent", "version"]

GMPXX_SRC = ["isfuns", "ismpf", "ismpq", "ismpz", "ismpznw", "limits",
             "osdoprnti", "osfuns", "osmpf", "osmpq", "osmpz"]

FWD_NOTE = ("/* compat.gmp: upstream reaches this header through -I<srcroot>.\n"
            "   mcpp compiles a package's sources with only its public\n"
            "   include_dirs, so instead of exposing GMP's private headers to\n"
            "   every consumer, each source directory gets a one-line\n"
            "   forwarder and the quoted include resolves next to the file\n"
            "   that wrote it. */\n")


def read(p):
    with open(p, "r", encoding="utf-8", errors="surrogateescape") as f:
        return f.read()


def build_files():
    """Return {path relative to the verdir: content}."""
    f = {}

    # 1. gmp.h, from the upstream template.
    gmp_h = read(os.path.join(SRC, "gmp-h.in"))
    for k, v in GMP_H_SUBS.items():
        if k not in gmp_h:
            raise SystemExit("gmp-h.in no longer contains %s" % k)
        gmp_h = gmp_h.replace(k, v)
    for old, new in GMP_H_PATCHES:
        if gmp_h.count(old) != 1:
            raise SystemExit("gmp-h.in patch target not found exactly once:\n" + old)
        gmp_h = gmp_h.replace(old, new)
    f[f"{WRAP}/gmp.h"] = gmp_h

    # 2. config.h.
    f[f"{WRAP}/config.h"] = CONFIG_H

    # 3. Generic thresholds. Upstream symlinks this one into the srcroot for
    #    every --disable-assembly build.
    f[f"{WRAP}/gmp-mparam.h"] = read(os.path.join(SRC, "mpn/generic/gmp-mparam.h"))

    # 4. Generator output. gmp-impl.h includes the first four by quoted name,
    #    so they belong beside it; trialdivtab.h/perfsqr.h/jacobitab.h are
    #    included from mpn/generic/*.c.
    for name in ["fib_table.h", "fac_table.h", "sieve_table.h", "mp_bases.h",
                 "trialdivtab.h"]:
        f[f"{WRAP}/{name}"] = read(os.path.join(GEN, name))
    for name in ["jacobitab.h", "perfsqr.h"]:
        f[f"{WRAP}/mpn/generic/{name}"] = read(os.path.join(GEN, name))

    # 5. Generated table sources, kept out of mpn/generic so the source glob
    #    for that directory stays a description of the TARBALL.
    for name in ["fib_table", "mp_bases"]:
        f[f"{WRAP}/mcpp/mcpp_gmp_{name}.c"] = (
            "/* compat.gmp: generated by upstream's gen-%s, limb=64 nail=0. */\n"
            % ("fib" if name == "fib_table" else "bases")
            + read(os.path.join(GEN, name + ".c")))

    # 6. Forwarders, one line each.
    def fwd(where, header, target):
        f[f"{WRAP}/{where}/{header}"] = FWD_NOTE + '#include "%s"\n' % target

    for h in ["config.h", "gmp-impl.h", "longlong.h", "trialdivtab.h"]:
        fwd("mpn/generic", h, "../../" + h)
    # hgcd2.c / hgcd2_jacobi.c spell this one as a path from the srcroot.
    fwd("mpn/generic/mpn/generic", "hgcd2-div.h", "../../hgcd2-div.h")
    for d in ["mpz", "mpf", "mpq", "printf"]:
        for h in ["config.h", "gmp-impl.h", "longlong.h"]:
            fwd(d, h, "../" + h)
    for h in ["config.h", "gmp-impl.h"]:
        fwd("scanf", h, "../" + h)
    for h in ["gmp-impl.h", "longlong.h"]:
        fwd("rand", h, "../" + h)
    for h in ["gmp-impl.h", "gmpxx.h"]:
        fwd("cxx", h, "../" + h)
    # gen-fib's table source spells `#include "gmp.h"` directly, so mcpp/
    # needs that forwarder too.
    for h in ["config.h", "gmp.h", "gmp-impl.h", "longlong.h"]:
        fwd("mcpp", h, "../" + h)

    # 7. The public include directory: two forwarders, so include_dirs
    #    exposes gmp.h and gmpxx.h and nothing else.
    f[f"{WRAP}/mcpp/include/gmp.h"] = (
        "/* compat.gmp's public header. The real gmp.h sits beside GMP's\n"
        "   private headers because gmp-impl.h includes it by quoted name;\n"
        "   this is what include_dirs points at, so a consumer sees these two\n"
        "   headers and none of the internals. */\n"
        '#include "../../gmp.h"\n')
    f[f"{WRAP}/mcpp/include/gmpxx.h"] = (
        "/* compat.gmp's public C++ header. The compiled half of the bindings\n"
        "   is the `gmpxx` feature; without it the stream operators and a few\n"
        "   helpers are undefined at link time. */\n"
        '#include "../../gmpxx.h"\n')

    # 8. mulfunc wrappers.
    for base, ops in MULFUNC.items():
        for op in ops:
            f[f"{WRAP}/mcpp/mcpp_gmp_{op}.c"] = (
                "/* compat.gmp: %s.c compiled as mpn_%s.\n"
                "   Upstream gets the same effect from mpn/%s.c being a\n"
                "   symlink to generic/%s.c plus a per-target -D. */\n"
                % (base, op, op, base)
                + "#define OPERATION_%s 1\n" % op
                + '#include "../mpn/generic/%s.c"\n' % base)

    # 9. __clz_tab is emitted only when longlong.h's count_leading_zeros
    #    falls back to a table -- which, with the inline asm on, it does not
    #    on x86_64 or arm64. Upstream's NO_ASM build always carries the
    #    symbol and its own test suite links against it, so force it on for
    #    this one TU: 129 bytes buys symbol parity with a stock libgmp.a.
    #    `=` and not `=1`: longlong.h re-#defines it with an EMPTY body, and
    #    only an identical redefinition is warning-free.
    f[f"{WRAP}/mcpp/mcpp_gmp_clz_tab.c"] = (
        "/* compat.gmp: mp_clz_tab.c with the table forced on -- see the\n"
        "   descriptor's note on __clz_tab. */\n"
        "#define COUNT_LEADING_ZEROS_NEED_CLZ_TAB\n"
        '#include "../mp_clz_tab.c"\n')

    # 10. The gmpxx feature's single TU.
    gmpxx = [
        "/* compat.gmp `gmpxx` feature: GMP's C++ bindings.\n"
        "   ONE translation unit rather than eleven source entries because the\n"
        "   feature table gates SOURCES, not defines: __GMP_WITHIN_GMPXX has to\n"
        "   live inside a file, so a wrapper is needed either way and one is\n"
        "   simpler than eleven. (Object-name collisions are NOT the reason --\n"
        "   `limits.cc` would have been one before mcpp 2026.8.3.4, but object\n"
        "   paths are nested unconditionally now: this package alone compiles\n"
        "   three different add.c into obj/compat_gmp/gmp-6.3.0/{mpf,mpn/generic,\n"
        "   mpz}/add.o.)\n"
        "\n"
        "   Compiled by the CONSUMER's toolchain on purpose. libgmpxx's\n"
        "   interface is std::ostream / std::istream / std::string, so its\n"
        "   symbol names carry the C++ standard library's ABI: a copy built\n"
        "   here against libstdc++ would fail to link for every libc++\n"
        "   consumer. */\n"
        "#define __GMP_WITHIN_GMPXX 1\n"
        "\n"
        "/* stdio.h / stdarg.h FIRST, and not for tidiness. gmp.h decides\n"
        "   whether to declare its FILE* and va_list entry points from whether\n"
        "   those headers have already been seen (_GMP_H_HAVE_FILE /\n"
        "   _GMP_H_HAVE_VA_LIST), and gmp-impl.h gates `struct gmp_asprintf_t`\n"
        "   + GMP_ASPRINTF_T_INIT on the same answer. Compiled separately,\n"
        "   osdoprnti.cc includes <stdarg.h> itself and gets them; amalgamated,\n"
        "   the first .cc below reaches gmp-impl.h first and freezes the answer\n"
        "   for the whole TU behind its include guard -- which reads as\n"
        "   `aggregate gmp_asprintf_t has incomplete type` on libstdc++ while\n"
        "   passing on a standard library whose <iostream> happens to drag\n"
        "   <cstdarg> in. */\n"
        "#include <stdio.h>\n"
        "#include <stdarg.h>\n"
        "#include <string.h>\n",
    ]
    for s in GMPXX_SRC:
        gmpxx.append('#include "../cxx/%s.cc"\n' % s)
    f[f"{WRAP}/mcpp/mcpp_gmpxx.cc"] = "".join(gmpxx)

    return f


def stage(dest):
    files = build_files()
    for rel, content in sorted(files.items()):
        p = os.path.join(dest, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8", errors="surrogateescape") as fh:
            fh.write(content)
    total = sum(len(c.encode("utf-8", "surrogateescape")) for c in files.values())
    print("staged %d files, %d bytes" % (len(files), total), file=sys.stderr)


def lua_string(s):
    """Lua long-bracket literal with a level that cannot appear in `s`."""
    for n in range(0, 12):
        eq = "=" * n
        if ("]%s]" % eq) not in s and ("[%s[" % eq) not in s:
            # A long string swallows one leading newline; add one back.
            return "[%s[\n%s]%s]" % (eq, s, eq)
    raise RuntimeError("no usable long-bracket level")






# --------------------------------------------------------------------------
# Descriptor emission
# --------------------------------------------------------------------------
HEADER = r'''-- compat.gmp -- GNU MP 6.3.0, compiled by mcpp itself on all three platforms.
--
-- NO install() HOOK, NO EXTERNAL BUILD SYSTEM. mcpp compiles GMP's portable
-- C kernels straight from the upstream tarball, with the consumer's own
-- toolchain, on linux / macOS / windows alike.
--
-- WHY NOT UPSTREAM'S BUILD
--
-- GMP ships only an autotools build, and driving it from an install() hook
-- (which is what the first version of this package did, linux + macOS only)
-- buys three problems:
--
--   1. `configure` COMPILES AND RUNS probe programs. One of them calls a
--      `void g(){}` with six arguments, which a C23 default -- gcc >= 15 --
--      rejects outright, so configure reports `could not find a working
--      compiler` on any recent distro. Reproduced here on gcc 16.1.0; CI
--      only stayed green because ubuntu-latest is still gcc 13.
--   2. Running probes at all requires a HOST compiler whose binaries can be
--      executed in the hook -- which is why the hook could not use mcpp's own
--      toolchain, and why the built archive came from a compiler unrelated to
--      the one the consumer links with.
--   3. There is no MSVC/clang-on-Windows path upstream at all, which is why
--      windows was deferred.
--
-- Describing the build in `mcpp = { }` removes all three at once: no
-- configure, no make, no cmake, no host compiler, no build-time deps of any
-- kind -- and windows becomes just another target, because the only thing
-- GMP's generic C needs is a GCC-compatible compiler, which is exactly what
-- this index already ships everywhere (clang++ on the MSVC ABI on Windows).
--
-- WHAT HAD TO BE VENDORED, AND WHY
--
-- GMP's build does two things a descriptor cannot express, both of which are
-- pure functions of (GMP_LIMB_BITS=64, GMP_NAIL_BITS=0):
--
--   * it substitutes gmp.h from gmp-h.in (eight @VAR@s), and
--   * it COMPILES AND RUNS seven table generators (gen-fac, gen-fib,
--     gen-bases, gen-sieve, gen-trialdivtab, gen-jacobitab, gen-psqr) to
--     produce nine tables.
--
-- Those outputs are generated once, by upstream's own generators, and
-- vendored into `generated_files` below (~270 KB, of which trialdivtab.h is
-- 109 KB). They are data, not a re-implementation: the descriptor's
-- generated tables are byte-for-byte what `gen-* 64 0` prints.
--
-- Because the tables are limb=64, config.h refuses to compile on a target
-- with no 64-bit unsigned type rather than silently computing wrong answers.
--
-- The rest of `generated_files` is glue: a one-line forwarding header per
-- source directory (upstream reaches gmp-impl.h / longlong.h / config.h
-- through -I<srcroot>; forwarders let the quoted include resolve next to the
-- file that wrote it, so the package needs NO -I and `include_dirs` can
-- expose gmp.h + gmpxx.h and nothing else), and one wrapper TU per
-- "mulfunc" source -- files upstream compiles repeatedly under different
-- -DOPERATION_* to emit a different mpn entry point each time.
--
-- EQUIVALENCE, MEASURED
--
-- Against a `--disable-assembly` autotools build of the same tarball:
--   * 598 global symbols, exactly the same set -- none missing, none extra;
--   * GMP's OWN test suite (`make check`, 178 programs) run against this
--     library: 177 passed, 0 failed, 1 skipped -- with gcc and with clang.
-- Faster than that reference build, too: `--disable-assembly` defines NO_ASM,
-- which also switches off longlong.h's inline-asm primitives (umul_ppmm,
-- add_ssaaaa, count_leading_zeros). Those are gated on compiler predefines
-- and degrade to C by themselves, so this build keeps them.
--
-- The hand-written per-CPU .asm kernels are NOT built: selecting them needs
-- m4 and a CPU match, i.e. exactly the host-dependence this package exists
-- to avoid.
--
-- License: GMP is dual LGPL-3.0-or-later OR GPL-2.0-or-later (tarball
-- README); the permissive side is declared. The vendored generated files
-- above are output of GMP's own generators and carry the same license.
'''

PKG_TMPL = r'''package = {
    spec        = "1",
    namespace   = "compat",
    name        = "gmp",
    description = "GMP -- GNU multiple precision arithmetic library (portable C kernels, built by mcpp)",
    licenses    = {"LGPL-3.0-or-later"},
    repo        = "https://gmplib.org/",
    type        = "package",

    -- One tarball, one sha256, three platforms -- the authoritative GNU
    -- release. (A GitHub snapshot would be missing `configure`, which this
    -- package does not use, but also `gmp-h.in`, which it does.)
    --
    -- No `deps`: nothing is needed at build time. The previous version
    -- carried `xim:make@latest` on linux and nothing on macOS, which is the
    -- kind of per-platform asymmetry that only shows up as a red job.
    --
    -- CN mirrors the identical bytes from `mcpp-res/gmp` -- the gitcode
    -- release asset is a byte-for-byte copy of the ftp.gnu.org tarball, so
    -- the single sha256 below authenticates either arm of the table.
    xpm = {
        linux = {
            ["6.3.0"] = {
                url    = {
                    GLOBAL = "https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/gmp/releases/download/6.3.0/gmp-6.3.0.tar.gz",
                },
                sha256 = "e56fd59d76810932a0555aa15a14b61c16bed66110d3c75cc2ac49ddaa9ab24c",
            },
        },
        macosx = {
            ["6.3.0"] = {
                url    = {
                    GLOBAL = "https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/gmp/releases/download/6.3.0/gmp-6.3.0.tar.gz",
                },
                sha256 = "e56fd59d76810932a0555aa15a14b61c16bed66110d3c75cc2ac49ddaa9ab24c",
            },
        },
        windows = {
            ["6.3.0"] = {
                url    = {
                    GLOBAL = "https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/gmp/releases/download/6.3.0/gmp-6.3.0.tar.gz",
                },
                sha256 = "e56fd59d76810932a0555aa15a14b61c16bed66110d3c75cc2ac49ddaa9ab24c",
            },
        },
    },

    mcpp = {
        language   = "c++23",
        import_std = false,
        c_standard = "c11",

        -- Two forwarding headers and nothing else. GMP's private headers
        -- (gmp-impl.h, longlong.h, config.h, the tables) stay OUT of every
        -- consumer's include path -- `config.h` in particular is a name no
        -- dependency should be putting on a -I line.
        include_dirs = { "gmp-6.3.0/mcpp/include" },

        targets = { ["gmp"] = { kind = "lib" } },
        deps    = { },

        -- __GMP_WITHIN_GMP is what switches gmp.h from the consumer view to
        -- the library's own; it must NOT reach consumers, and package cflags
        -- do not. -w: this is vendored third-party C, and 516 TUs of
        -- upstream's warnings would bury anything worth reading.
        cflags   = { "-D__GMP_WITHIN_GMP", "-w" },
        cxxflags = { "-w" },

        -- The archive routinely ends up inside a consumer's shared library
        -- (a plugin, a Python extension, a GDExtension). Without PIC that
        -- link fails outright -- "relocation R_X86_64_32 against
        -- `.rodata` can not be used when making a shared object" -- and it
        -- also fails against a PIE default, which is how a clang++ link of
        -- the gmpxx test surfaced it here. Not on windows: PE/COFF has no
        -- such distinction and clang would only report the flag as unused.
        linux  = { cflags = { "-fPIC" }, cxxflags = { "-fPIC" } },
        macosx = { cflags = { "-fPIC" }, cxxflags = { "-fPIC" } },

        -- gmpxx: GMP's C++ bindings (mpz_class / mpq_class / mpf_class --
        -- RAII wrappers with operator overloading, and the iostream
        -- operators).
        --
        -- A feature rather than always-on because it costs a C++ TU that a
        -- C consumer never needs; `gmpxx.h` itself ships unconditionally, so
        -- leaving the feature off and calling `std::cout << mpz_class`
        -- fails at LINK time with an undefined `__gmp_ostream_operator`
        -- rather than at the #include -- which is also the negative check
        -- that the gate really gates (verified: 3 undefined references).
        --
        -- Compiled by the CONSUMER's toolchain, which is the whole reason it
        -- is a source and not a prebuilt archive: these TUs' symbols carry
        -- std::ostream / std::string in their names, so a copy built against
        -- libstdc++ would not resolve for a libc++ consumer.
        features = {
            ["gmpxx"] = { sources = { "gmp-6.3.0/mcpp/mcpp_gmpxx.cc" } },
        },

        sources = {
%SOURCES%
        },

        -- Everything GMP's build would have generated. See the header.
        generated_files = {
%GENERATED%
        },
    },
}
'''


def emit_descriptor():
    src = []
    src.append("            -- root: the 16 top-level TUs upstream compiles")
    src.append("            -- (mp_clz_tab.c goes through the wrapper below)")
    for s in ROOT_SRC:
        src.append('            "gmp-6.3.0/%s.c",' % s)
    src.append("")
    src.append("            -- mpn: the portable C kernels. The exclusions are")
    src.append("            -- upstream's: udiv_w_sdiv is an extra function for")
    src.append("            -- CPUs with signed-but-not-unsigned divide,")
    src.append("            -- div_qr_1{n,u}_pi2 are selected only by a")
    src.append("            -- HAVE_NATIVE_mpn_* this build never sets, and the")
    src.append("            -- five mulfunc sources are compiled through the")
    src.append("            -- per-operation wrappers instead.")
    src.append('            "gmp-6.3.0/mpn/generic/*.c",')
    for s in MPN_EXCLUDE:
        src.append('            "!gmp-6.3.0/mpn/generic/%s.c",' % s)
    src.append("")
    src.append("            -- these six directories are compiled whole")
    for d in ["mpz", "mpf", "mpq", "printf", "scanf", "rand"]:
        src.append('            "gmp-6.3.0/%s/*.c",' % d)
    src.append("")
    src.append("            -- generated. Listed one by one rather than globbed:")
    src.append("            -- a glob over files that generated_files has yet to")
    src.append("            -- write is a build that depends on step ordering.")
    gen_c = ["mcpp_gmp_%s.c" % op for ops in MULFUNC.values() for op in ops]
    gen_c += ["mcpp_gmp_clz_tab.c", "mcpp_gmp_fib_table.c", "mcpp_gmp_mp_bases.c"]
    for s in sorted(gen_c):
        src.append('            "gmp-6.3.0/mcpp/%s",' % s)

    files = build_files()
    gen = []
    for rel, content in sorted(files.items()):
        gen.append('            ["%s"] =\n%s,' % (rel, lua_string(content)))

    body = PKG_TMPL.replace("%SOURCES%", "\n".join(src))
    body = body.replace("%GENERATED%", "\n".join(gen))
    return HEADER + body

if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode not in ("write", "check", "stage"):
        raise SystemExit(__doc__)
    with tempfile.TemporaryDirectory(prefix="mcpp-gmp-") as work:
        prepare(work)
        if mode == "stage":
            stage(sys.argv[2])
        else:
            text = emit_descriptor()
            if mode == "write":
                with open(DESCRIPTOR, "w", encoding="utf-8") as fh:
                    fh.write(text)
                print("wrote %s (%d bytes)" % (DESCRIPTOR, len(text)), file=sys.stderr)
            else:
                current = open(DESCRIPTOR, encoding="utf-8").read()
                if current == text:
                    print("compat.gmp.lua is in sync with this generator")
                else:
                    raise SystemExit(
                        "compat.gmp.lua DIFFERS from what this generator produces "
                        "(%d bytes on disk vs %d regenerated); rerun with `write`"
                        % (len(current), len(text)))
