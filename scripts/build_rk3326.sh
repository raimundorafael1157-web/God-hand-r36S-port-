#!/bin/sh
# Builds godhand for the R36S (aarch64, glibc 2.30, raylib on KMS/DRM) from source/godhand-recomp.
#   scripts/build_rk3326.sh generate <iso>   # once per disc: game code from YOUR disc (Linux x86_64 recompiler)
#   scripts/build_rk3326.sh vu1 <iso>        # VU1 microcode ahead of time (x86_64 run of the game, ~5 min of play)
#   scripts/build_rk3326.sh state <iso>      # save state in the first fight of stage 1 -> builds/rk3326/stage1.state
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
vu1)
    # The game's VU1 programs run in an interpreter unless compiled ahead of time. In the opening cutscene that alone
    # took the game from ~100% to 10-23% speed even on a PC (null GS). This builds the game for the PC, plays the
    # opening with no window while saving each VU1 program it meets, and compiles them into generated/vu1/.
    ISO=${2:?usage: build_rk3326.sh vu1 <path-to-your-SLUS-21503.iso>}
    case "$ISO" in /*) ;; *) ISO="$CALLER_DIR/$ISO";; esac
    [ -f generated/register_functions.cpp ] || { echo "no generated code: run 'build_rk3326.sh generate <iso>' first"; exit 1; }
    cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Release -DGODHAND_BUILD_RECOMP=ON -DGODHAND_BUILD_TOOLS=ON \
        -DGODHAND_DEBUG_UI=OFF -DCMAKE_C_FLAGS=-msse4.1 -DCMAKE_CXX_FLAGS=-msse4.1
    ninja -C build-host vu1_recomp
    cmake -S . -B build-x64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DGODHAND_DEBUG_UI=OFF \
        -DCMAKE_C_FLAGS=-msse4.1 -DCMAKE_CXX_FLAGS=-msse4.1
    ninja -C build-x64 -j "$JOBS" godhand
    CAP="$SRC/build/vu1_capture"
    mkdir -p "$CAP"
    # Title -> NEW GAME -> NORMAL, then the opening cutscene and the start of stage 1 (times in PS2 fields: the same on
    # any machine speed). More play captures more programs; each run only adds to the folder.
    GODHAND_HEADLESS=1 PS2X_BENCH=1 PS2X_GS_NULL=1 GODHAND_DUMP_ON_TIME_LIMIT=0 GODHAND_ISO_PREFETCH=0 \
        PS2X_PAD_SCRIPT="1300v:start:10,1500v:circle:10,1700v:circle:10" PS2X_VU1_CAPTURE="$CAP" \
        ./build-x64/bin/godhand --iso "$ISO" --headless --time-limit "${VU1_SECONDS:-240}" || true
    ls "$CAP"/vu1_*.bin >/dev/null 2>&1 || { echo "no VU1 program captured"; exit 1; }
    mkdir -p generated/vu1
    ./build-host/runtime/ps2xTest/vu1_recomp "$CAP" generated/vu1
    echo "VU1 programs compiled: $(ls "$CAP"/vu1_*.bin | wc -l); run 'build_rk3326.sh build' next"
    ;;
state)
    # A save state inside the first fight of stage 1 (Gene against three thugs), made on the PC build from the vu1 step.
    # Save states load across x86_64 and ARM64 (checked both ways). It holds the game's RAM: keep it out of git.
    ISO=${2:?usage: build_rk3326.sh state <path-to-your-SLUS-21503.iso>}
    case "$ISO" in /*) ;; *) ISO="$CALLER_DIR/$ISO";; esac
    [ -x build-x64/bin/godhand ] || { echo "no PC build: run 'build_rk3326.sh vu1 <iso>' first"; exit 1; }
    mkdir -p "$REPO/builds/rk3326"
    GODHAND_HEADLESS=1 PS2X_BENCH=1 PS2X_GS_NULL=1 GODHAND_DUMP_ON_TIME_LIMIT=0 GODHAND_ISO_PREFETCH=0 \
        PS2X_PAD_SCRIPT="1300v:start:10,1500v:circle:10,1700v:circle:10" \
        PS2X_BENCH_SAVE_STATE="$REPO/builds/rk3326/stage1.state" PS2X_BENCH_SAVE_FIELD=4900 \
        ./build-x64/bin/godhand --iso "$ISO" --headless --time-limit "${STATE_SECONDS:-400}" || true
    [ -s "$REPO/builds/rk3326/stage1.state" ] || { echo "no state saved (the run ended before field 4900?)"; exit 1; }
    echo "state: $REPO/builds/rk3326/stage1.state"
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
    # Debug info makes it ~640 MB; stripped ~90 MB.
    if command -v aarch64-linux-gnu-strip >/dev/null 2>&1; then
        aarch64-linux-gnu-strip -o "$REPO/builds/rk3326/godhand" build-rk3326/bin/godhand
    else
        echo "aarch64-linux-gnu-strip not found (apt install binutils-aarch64-linux-gnu): copying the ~640 MB unstripped binary"
        cp build-rk3326/bin/godhand "$REPO/builds/rk3326/godhand"
    fi
    cp "$REPO/scripts/run_bench.sh" "$REPO/scripts/collect_hw_info.sh" "$REPO/builds/rk3326/"
    echo "binary: $REPO/builds/rk3326/godhand"
    ;;
*)
    echo "usage: build_rk3326.sh generate <iso> | vu1 <iso> | state <iso> | build"; exit 1;;
esac
