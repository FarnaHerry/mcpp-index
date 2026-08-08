#!/usr/bin/env bash
# check_graphics_install_side_effects.sh — installing the graphics stack must
# not change what UNRELATED members link against.
#
# This is the test the 2026-08-08 incident did not have.
#
# `compat.glx-runtime` gained a dependency on `xim:mesa`, and mesa declares
# `xim:glibc@>=2.38`. A floor is satisfied by anything above it, so xim
# installed glibc 2.44 alongside the existing 2.39. mcpp then resolved "the
# glibc payload" by taking whatever `readdir` yielded first: the compile side
# picked 2.44 while the artifact's interpreter, frozen in gcc's specs at
# install time, still named 2.39. Binaries began referencing GLIBC_2.42
# symbols against a runtime without them.
#
# What made it expensive was where it surfaced. `asio-module` and `core` do
# not use graphics, do not depend on mesa, and were not touched by the change
# -- they turned red because a SECOND glibc had appeared on the machine.
# Every test in this repo was about the package it was testing, so nothing
# asked the only question that mattered: did installing this change anything
# for everyone else?
#
# The engine-side repair is in mcpp 2026.8.8.2 (the runtime binding is read
# from the subos rather than guessed). This test is the detector for the
# class, and it fails on any mcpp older than that -- which is the point: the
# graphics stack must not land on a toolchain that can still make this
# mistake.
#
#   bash tests/check_graphics_install_side_effects.sh
#
# Env: MCPP (default `mcpp` on PATH), MEMBER (default asio-module)
set -uo pipefail

MCPP="${MCPP:-mcpp}"
MEMBER="${MEMBER:-asio-module}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJ="$ROOT/tests/examples/$MEMBER"

[[ -d "$PROJ" ]] || { echo "no such member: $MEMBER"; exit 2; }
case "$(uname -s)" in Linux) ;; *) echo "SKIP: ELF-only check"; exit 0 ;; esac

readelf_bin=$(command -v readelf || true)
if [[ -z "$readelf_bin" ]]; then
    readelf_bin=$(ls "${MCPP_HOME:-$HOME/.mcpp}"/registry/data/xpkgs/xim-x-binutils/*/bin/readelf 2>/dev/null | head -1)
fi
# Not a skip. A check that quietly passes when it cannot look is the shape of
# failure this whole file exists to prevent -- the incident got through because
# every test asked about its own package and none about everyone else's.
[[ -x "$readelf_bin" ]] || {
    echo "FAIL: no readelf on PATH and none in the binutils payload, so the"
    echo "      artifacts cannot be inspected and this check proves nothing."
    exit 1; }

# The two facts that changed under the incident, and nothing else. Both are
# read off the ARTIFACT: what a member links against is only observable there,
# and the specs file that produced the mismatch looked correct throughout.
describe() {
    local bin=$1
    local interp glibcmax
    interp=$("$readelf_bin" -l "$bin" 2>/dev/null \
             | sed -n 's/.*interpreter: \(.*\)\]/\1/p' | head -1)
    # Highest GLIBC_x.y this artifact requires. The failure mode is this
    # number rising above what the interpreter's libc actually provides.
    glibcmax=$("$readelf_bin" -V "$bin" 2>/dev/null \
               | grep -oE 'GLIBC_[0-9]+\.[0-9]+' | sort -t_ -k2 -V | tail -1)
    echo "interp=${interp:-none} glibcmax=${glibcmax:-none}"
}

# Diagnostics go to stderr, deliberately. This function's stdout IS its return
# value (the caller does `before=$(build_and_describe ...)`), so anything
# printed there on failure is captured into a variable and thrown away -- which
# is what happened: a failing build produced one blank line and no reason.
# `test`, not `build`: the members that broke in the incident (asio-module,
# core) are test projects, and `mcpp build` leaves them at objects and BMIs
# with no executable to inspect. Earlier drafts used `build` and still found
# artifacts -- left over from a previous run's `test` -- which is the same
# lean-on-stale-state that this whole file exists to argue against.
#
# The exit status is deliberately ignored: a member's own test may fail for
# reasons that have nothing to do with which libc it linked, and the artifact
# is produced either way. A build that produces NO artifact is caught below,
# which is the failure that would actually invalidate the comparison.
build_and_describe() {
    ( cd "$PROJ" && "$MCPP" test ) > "$1" 2>&1 || true
    local bin
    bin=$(find "$PROJ/target" -type f -path '*/bin/*' -perm -u+x 2>/dev/null \
          | head -1)
    [[ -n "$bin" ]] || {
        echo "no artifact produced under $PROJ/target:" >&2
        tail -25 "$1" >&2; return 1; }
    describe "$bin"
}

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# The precondition, checked rather than assumed.
#
# This compares a build from BEFORE the graphics stack against one from after.
# On a machine where it is already installed, "install the graphics stack" is a
# no-op, the baseline is already the post-install state, and the comparison is
# between a thing and itself -- which passes on any mcpp ever released,
# including the ones with the defect. Measured exactly that way: an mcpp
# predating the fix printed PASS here on a machine that already had both
# payloads.
#
# So: refuse rather than report success. A gate that cannot evaluate must not
# be green.
GLIBC_DIR="${MCPP_HOME:-$HOME/.mcpp}/registry/data/xpkgs/xim-x-glibc"
before_count=$(ls -1 "$GLIBC_DIR" 2>/dev/null | wc -l)
if [[ "$before_count" -gt 1 ]]; then
    cat <<MSG
INCONCLUSIVE: $before_count glibc payloads are already installed:
$(ls -1 "$GLIBC_DIR" | sed 's/^/    /')

  This check compares before-graphics against after-graphics. With the stack
  already present the install step changes nothing, so both halves describe
  the same state and the comparison cannot fail -- for a fixed mcpp or a
  broken one alike.

  Run it against a home that has not seen the graphics stack:
      MCPP_HOME=\$(mktemp -d)/home bash \$0
MSG
    exit 1
fi

echo "== baseline: $MEMBER before the graphics stack =="
before=$(build_and_describe "$tmp/before.log") || exit 1
echo "  $before"

# Which home actually served that build?
#
# Asked of the artifact, not of the environment. Setting MCPP_HOME does not by
# itself guarantee the build used it -- a warm target/, a project-local
# toolchain, or an inherited payload can all route around it, and then this
# check installs graphics into one home while measuring artifacts from
# another. Measured exactly that way: MCPP_HOME pointed at a fresh directory,
# the install step reported no payloads at all, and the artifact's interpreter
# came from the developer's real home. It printed PASS.
#
# The interpreter names the payload the artifact will load, so it is the one
# unambiguous statement of whose payloads this build used.
# `mkdir -p` first: an MCPP_HOME that does not exist yet is a legitimate way
# to ask for a fresh one, and `cd` to it would fail. An EMPTY home_real must
# never reach the pattern below -- `""/*` is `/*`, which matches every
# absolute path and waves the check through. (It did.)
mkdir -p "${MCPP_HOME:-$HOME/.mcpp}" 2>/dev/null || true
home_real=$(cd "${MCPP_HOME:-$HOME/.mcpp}" 2>/dev/null && pwd || true)
interp_path=${before#*interp=}; interp_path=${interp_path%% *}
if [[ -z "$home_real" ]]; then
    echo "INCONCLUSIVE: cannot resolve MCPP_HOME (${MCPP_HOME:-$HOME/.mcpp})"
    exit 1
fi
case "$interp_path" in
    "$home_real"/*) ;;
    *)
        cat <<MSG
INCONCLUSIVE: the artifact was built against a different home than the one
this check installs into.

  MCPP_HOME  : ${home_real:-<unset>}
  interpreter: $interp_path

  Installing the graphics stack into one home and measuring artifacts from
  another compares two unrelated things, and the comparison passes for any
  mcpp. Clean the member's target/ (or use a checkout that has none) so the
  build actually resolves through MCPP_HOME.
MSG
        exit 1 ;;
esac

echo "== installing the graphics stack =="
# Through the same path a user would: build a member that DEPENDS on it.
# imgui-window pulls compat.glfw, which pulls compat.glx-runtime, which is the
# package whose mesa dependency installed the second glibc. Naming payloads
# directly would test a mechanism nobody uses.
#
# A failure here is a failure of the test, not a reason to pass. The first
# draft called `mcpp toolchain install-deps`, which does not exist, and fell
# through to a branch that exits 0 -- a check that could never check anything.
GRAPHICS_MEMBER="${GRAPHICS_MEMBER:-imgui-window}"
[[ -d "$ROOT/tests/examples/$GRAPHICS_MEMBER" ]] || {
    echo "no such member: $GRAPHICS_MEMBER"; exit 2; }
( cd "$ROOT/tests/examples/$GRAPHICS_MEMBER" && "$MCPP" build ) \
    > "$tmp/install.log" 2>&1 || {
    echo "FAIL: could not build $GRAPHICS_MEMBER, so the graphics stack was"
    echo "      never installed and this check proved nothing:"
    tail -15 "$tmp/install.log"; exit 1; }
echo "  installed; glibc payloads now on disk:"
ls -1 "${MCPP_HOME:-$HOME/.mcpp}"/registry/data/xpkgs/xim-x-glibc 2>/dev/null \
    | sed 's/^/    /'

echo "== $MEMBER again, unchanged source =="
after=$(build_and_describe "$tmp/after.log") || exit 1
echo "  $after"

if [[ "$before" != "$after" ]]; then
    cat <<MSG

FAIL: installing the graphics stack changed what an unrelated member links against.

  before: $before
  after:  $after

A member that does not use graphics must not notice that graphics was
installed. When these differ, a second libc payload has become reachable and
the toolchain is choosing between them by something other than the subos'
declared runtime -- which is what shipped GLIBC_2.42 references into binaries
running on 2.39. mcpp 2026.8.8.2+ resolves the runtime from an authority; an
older mcpp cannot pass this.
MSG
    exit 1
fi

echo
after_count=$(ls -1 "$GLIBC_DIR" 2>/dev/null | wc -l)
echo "PASS: $MEMBER is byte-identical in interpreter and glibc ceiling"
# Strength is about ending with a CHOICE to get wrong, not about the count
# going up. 0 -> 1 is an increase and proves nothing: with one payload there is
# nothing to pick between, and every mcpp ever released gets it right.
if [[ "$after_count" -ge 2 ]]; then
    echo "      (strong: $before_count -> $after_count glibc payloads — the build had a"
    echo "       choice to get wrong, which is the condition the incident needed)"
else
    echo "      (weak: $after_count glibc payload — no choice existed, so this run"
    echo "       shows no drift without exercising the defect. A home seeded with a"
    echo "       glibc OLDER than mesa's floor is what makes the install add one.)"
fi
