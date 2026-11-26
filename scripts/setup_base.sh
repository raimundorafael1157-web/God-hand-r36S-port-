#!/bin/sh
# Prepares source/godhand-recomp: the base project at a pinned commit, our patches applied on top, our own files
# (toolchain, ARM64 intrinsic shims, zig wrappers) copied in, and sse2neon fetched and checked.
# The base repository itself is never modified upstream: everything RK3326-specific lives in patches/ and platform/.
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
BASE_URL=https://github.com/LasVola/godhand-recomp.git
BASE_COMMIT=14e956a1274332440273845fbab2c1f1e30c4a5a
SSE2NEON_URL=https://raw.githubusercontent.com/DLTcollab/sse2neon/v1.8.0/sse2neon.h
SSE2NEON_SHA256=07723c9f9457dd4316f1fde3dd4eb6f31dd67d9955f6c21f4e609ac1698be48a
DEST="$REPO/source/godhand-recomp"

if [ ! -d "$DEST/.git" ]; then
    mkdir -p "$REPO/source"
    git clone "$BASE_URL" "$DEST"
fi
cd "$DEST"
git fetch --depth 1 origin "$BASE_COMMIT" 2>/dev/null || git fetch origin
git checkout -q --force "$BASE_COMMIT"
# Keeps the game-derived folders and every build tree (build-host, build-x64, build-rk3326: hours of compiling).
git clean -qfd -e game -e generated -e generated_modules -e 'build*' -e out
# PATCH_MAX=0009 stops after that patch (package A of the A/B comparison: 0001-0009; package B: all of them).
for patch in "$REPO"/patches/*.patch; do
    num=$(basename "$patch" | cut -c1-4)
    if [ -n "${PATCH_MAX:-}" ] && [ "$num" \> "$PATCH_MAX" ]; then
        echo "skipping $(basename "$patch") (PATCH_MAX=$PATCH_MAX)"
        continue
    fi
    echo "applying $(basename "$patch")"
    git apply --whitespace=nowarn "$patch"
done
rm -rf "$DEST/renderer"   # a symlink in development checkouts
cp -r "$REPO/cmake" "$REPO/platform" "$REPO/renderer" "$DEST/"
curl -sSfL -o runtime/ps2xRuntime/include/sse2neon.h "$SSE2NEON_URL"
echo "$SSE2NEON_SHA256  runtime/ps2xRuntime/include/sse2neon.h" | sha256sum -c -
echo "base ready: $DEST (commit $BASE_COMMIT + $(ls "$REPO"/patches/*.patch | wc -l) patches)"
