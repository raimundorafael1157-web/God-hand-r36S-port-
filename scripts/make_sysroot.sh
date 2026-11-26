#!/bin/sh
# Builds a minimal ARM64 sysroot with only what the RK3326 binary links against (DRM, GBM, EGL, GLES, ALSA headers and
# .so stubs). Only these folders: the binary itself targets glibc 2.30 through zig, and on the device it uses the
# device's own libraries. Needs an Ubuntu/Debian host (WSL works) and sudo for the apt arm64 source.
set -eu
OUT=${1:-$(cd "$(dirname "$0")/.." && pwd)/source/rk3326-sysroot}
WORK=$(mktemp -d)
if ! dpkg --print-foreign-architectures | grep -q arm64; then
    sudo dpkg --add-architecture arm64
    . /etc/os-release
    echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports $VERSION_CODENAME main universe" | sudo tee /etc/apt/sources.list.d/rk3326-arm64.list
    # Keep the regular mirrors amd64-only so apt does not look for arm64 packages there (they only serve amd64):
    # Ubuntu 24.04 keeps them in ubuntu.sources, 22.04 and older in sources.list.
    if [ -f /etc/apt/sources.list.d/ubuntu.sources ]; then
        sudo sed -i 's/^Types: deb$/Types: deb\nArchitectures: amd64/' /etc/apt/sources.list.d/ubuntu.sources
    fi
    if [ -f /etc/apt/sources.list ]; then
        sudo sed -i -E 's/^deb (https?:)/deb [arch=amd64] \1/' /etc/apt/sources.list
    fi
    sudo apt-get update
fi
cd "$WORK"
apt-get download libdrm-dev:arm64 libdrm2:arm64 libgbm-dev:arm64 libgbm1:arm64 libegl-dev:arm64 libegl1:arm64 \
    libgles-dev:arm64 libgles2:arm64 libglvnd0:arm64 libasound2-dev:arm64 libasound2t64:arm64 \
    || apt-get download libdrm-dev:arm64 libdrm2:arm64 libgbm-dev:arm64 libgbm1:arm64 libegl-dev:arm64 libegl1:arm64 \
    libgles-dev:arm64 libgles2:arm64 libglvnd0:arm64 libasound2-dev:arm64 libasound2:arm64
mkdir -p full
for deb in *.deb; do dpkg -x "$deb" full; done
rm -rf "$OUT"
mkdir -p "$OUT/usr/include" "$OUT/usr/lib/aarch64-linux-gnu"
cp -r full/usr/include/EGL full/usr/include/GLES2 full/usr/include/GLES3 full/usr/include/KHR full/usr/include/libdrm \
    full/usr/include/gbm.h full/usr/include/xf86drm.h full/usr/include/xf86drmMode.h full/usr/include/alsa "$OUT/usr/include/"
cp -a full/usr/lib/aarch64-linux-gnu/libEGL* full/usr/lib/aarch64-linux-gnu/libGLESv2* full/usr/lib/aarch64-linux-gnu/libGLdispatch* \
    full/usr/lib/aarch64-linux-gnu/libdrm.so* full/usr/lib/aarch64-linux-gnu/libgbm* full/usr/lib/aarch64-linux-gnu/libasound* \
    "$OUT/usr/lib/aarch64-linux-gnu/"
rm -rf "$WORK"
echo "sysroot ready: $OUT"
