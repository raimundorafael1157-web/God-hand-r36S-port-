#!/bin/sh
# Builds godhand for the R36S (aarch64, glibc 2.30, raylib on KMS/DRM) from source/godhand-recomp.
#   scripts/build_rk3326.sh generate <iso>   # once per disc: game code from YOUR disc (Linux x86_64 recompiler)
#   scripts/build_rk3326.sh build            # the ARM64 binary -> builds/rk3326/godhand
# Generated code stays in source/ (git-ignored): never commit it, it is derived from the game.
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
SRC="$REPO/source/godhand-recomp"
JOBS=${JOBS:-$(nproc)}
[ -d "$SRC" ] || { echo "run scripts/setup_base.sh first"; exit 1; }
CALLER_DIR=$(pwd)
cd "$SRC"

case "${1:-build}" in
generate)
    ISO=${2:?usage: build_rk3326.sh generate <path-to-your-SLUS-21503.iso>}
    cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Release -DGODHAND_BUILD_RECOMP=ON -DGODHAND_DEBUG_UI=OFF \
        -DCMAKE_C_FLAGS=-msse4.1 -DCMAKE_CXX_FLAGS=-msse4.1
    ninja -C build-host ps2_recomp
    export GODHAND_RECOMP="$SRC/build-host/runtime/ps2xRecomp/ps2_recomp"
    case "$ISO" in /*) ;; *) ISO="$CALLER_DIR/$ISO";; esac
    python3 -I build.py generate --iso "$ISO"
    # --iso goes before the subcommand (the base README has it after, which argparse rejects).
    python3 -I tools/snr2/snr2.py --iso "$ISO" extract
    # gen_modules.py calls snr2 without --iso: it reads the default ../disc/God Hand (USA).iso.
    mkdir -p "$REPO/source/disc"
    ln -sf "$ISO" "$REPO/source/disc/God Hand (USA).iso"
    python3 -I tools/snr2/gen_modules.py
    python3 -I build.py modules
    ;;
build)
    [ -f generated/register_functions.cpp ] || { echo "no generated code: run 'build_rk3326.sh generate <iso>' first"; exit 1; }
    python3 -m ziglang version >/dev/null 2>&1 || python3 -m pip install --user ziglang==0.17.1
    export RK3326_SYSROOT=${RK3326_SYSROOT:-$REPO/source/rk3326-sysroot}
    [ -d "$RK3326_SYSROOT" ] || "$REPO/scripts/make_sysroot.sh" "$RK3326_SYSROOT"
    cmake -S . -B build-rk3326 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-zig-glibc230.cmake -DCMAKE_BUILD_TYPE=Release \
        -DGODHAND_DEBUG_UI=OFF -DPLATFORM=DRM
    ninja -C build-rk3326 -j "$JOBS" godhand
    mkdir -p "$REPO/builds/rk3326"
    cp build-rk3326/bin/godhand "$REPO/builds/rk3326/godhand"
    cp "$REPO/scripts/run_bench.sh" "$REPO/builds/rk3326/"
    echo "binary: $REPO/builds/rk3326/godhand"
    ;;
*)
    echo "usage: build_rk3326.sh generate <iso> | build"; exit 1;;
esac
