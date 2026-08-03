#!/usr/bin/env bash
# Build the godot-cpp source archive this index points at.
#
# WHY a repack at all: godot-cpp's GDExtension bindings (~1000 classes,
# gen/include + gen/src) are not in any upstream tag archive -- they are
# produced by upstream's own binding_generator.py from
# gdextension/extension_api.json at build time, which would make every
# consumer of the package need a Python toolchain. So the generator runs
# ONCE, here, and the result is published alongside the untouched upstream
# tree. Same idea as the frozen configure snapshots in the opencv/ffmpeg
# packages: no code generation on the consumer side.
#
# The archive is exactly:
#   * the upstream tag tarball, byte-for-byte (verified below), plus
#   * gen/, produced by running upstream's unmodified binding_generator.py.
#
# Reproduce:
#   tools/godot-cpp/repack.sh godot-4.5-stable 4.5.0 /tmp/out
# then publish /tmp/out/godot-cpp-<version>.tar.gz to
# github.com/xlings-res/godot-cpp (GLOBAL) and gitcode.com/mcpp-res/godot-cpp
# (CN), both under the bare-version tag.
set -euo pipefail

TAG="${1:?usage: repack.sh <upstream-tag> <bare-version> <outdir>}"
VERSION="${2:?}"
OUTDIR="${3:?}"

PYTHON="${PYTHON:-python3}"
WRAP="godot-cpp-${TAG}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$OUTDIR"

echo "==> fetching upstream ${TAG}"
curl -L -fsS -o "$WORK/upstream.tar.gz" \
    "https://github.com/godotengine/godot-cpp/archive/refs/tags/${TAG}.tar.gz"
echo "    upstream sha256: $(sha256sum "$WORK/upstream.tar.gz" | cut -d' ' -f1)"

tar -xzf "$WORK/upstream.tar.gz" -C "$WORK"
test -d "$WORK/$WRAP" || { echo "unexpected wrap dir in archive" >&2; exit 1; }

echo "==> generating bindings with upstream binding_generator.py"
(
    cd "$WORK/$WRAP"
    "$PYTHON" -c "
import binding_generator
binding_generator.generate_bindings('gdextension/extension_api.json', True, '64', 'single', '.')
"
    # generate_bindings() imports the module, which leaves bytecode behind
    find . -name '__pycache__' -type d -prune -exec rm -rf {} +
)

echo "==> verifying the upstream tree is untouched"
tar -xzf "$WORK/upstream.tar.gz" -C "$WORK" --transform "s|^${WRAP}|${WRAP}.orig|"
diff -r --no-dereference "$WORK/${WRAP}.orig" "$WORK/${WRAP}" > "$WORK/diff.txt" || true
if grep -v "^Only in $WORK/${WRAP}: gen\$" "$WORK/diff.txt" | grep -q .; then
    echo "upstream files were modified -- refusing:" >&2
    grep -v "^Only in $WORK/${WRAP}: gen\$" "$WORK/diff.txt" >&2
    exit 1
fi
echo "    every upstream file is byte-identical; gen/ is the only addition"
rm -rf "$WORK/${WRAP}.orig"

echo "==> packing (deterministic: sorted, fixed mtime, gzip -n)"
tar --sort=name --mtime='UTC 2026-01-01' --owner=0 --group=0 --numeric-owner \
    -C "$WORK" -cf - "$WRAP" | gzip -n -9 > "$OUTDIR/godot-cpp-${VERSION}.tar.gz"

sha256sum "$OUTDIR/godot-cpp-${VERSION}.tar.gz"
