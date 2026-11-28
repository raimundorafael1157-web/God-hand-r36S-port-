#!/bin/sh
# Runs the measurement on the R36S. By default the run presses START on the title, NEW GAME, NORMAL (times in PS2
# fields, so the same on a slow device), then measures the opening cutscene and the start of stage 1, the heaviest
# parts reachable without a save. PS2X_PAD_SCRIPT="" keeps it on the title screen.
# Copy this folder (godhand + run_bench.sh) to the SD card, then from a terminal:
#   sh run_bench.sh /caminho/para/God\ Hand\ (USA).iso [modo] [segundos] [save-state]
# modo: cpu (default) = nothing is drawn, measures EE + VU1 + IOP + GS front end only (PS2X_GS_NULL=1)
#       swgs          = the runtime's software GS draws every frame (its cost on top, still no screen)
# Optional (environment, all off by default; the plain run is unchanged):
#   PS2X_PROF=1      GameThread time by subsystem (ee, vu1, iop, dispatch, sched, gif, vif, idle, gs-wait): [bench-sections]
#   PS2X_PIN=auto    pins GameThread/IOPThread/GSThread to cores 3/2/1 (or PS2X_PIN=GameThread:3,IOPThread:2,...)
#   PS2X_GAME_NICE=-5  raises the GameThread's priority (needs root)
# Long thermal run: sh run_bench.sh <iso> cpu 900 stage1.state  ([bench-thermal] compares the first and last minute).
# Writes bench-<modo>-<date>.log next to this script. Send that file back. The only system change is the clock
# governors set to "performance" for the run and put back afterwards (PERF=0 skips it).
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
ISO=${1:?uso: sh run_bench.sh <iso> [cpu|swgs] [segundos] [save-state]}
MODE=${2:-cpu}
SECONDS_LIMIT=${3:-600}
STATE=${4:-}
case "$STATE" in ""|/*) ;; *) STATE="$(pwd)/$STATE";; esac
# GODHAND_BIN picks the binary (package A/B: godhand-A, godhand-B); default godhand-A, or godhand in older packages.
# BENCH_TAG goes into the log name (run_ab.sh: A1, B1...).
if [ -n "${GODHAND_BIN:-}" ]; then BIN="$DIR/$GODHAND_BIN"
elif [ -x "$DIR/godhand-A" ]; then BIN="$DIR/godhand-A"
else BIN="$DIR/godhand"; fi
[ -x "$BIN" ] || { echo "binary not found: $BIN"; exit 1; }
LOG="$DIR/bench-$MODE${BENCH_TAG:+-$BENCH_TAG}-$(date +%Y%m%d-%H%M%S).log"

{
    echo "== device"; uname -a; cat /proc/device-tree/model 2>/dev/null; echo
    for c in /sys/devices/system/cpu/cpu[0-9]*; do echo "$c $(cat $c/cpufreq/scaling_governor 2>/dev/null) $(cat $c/cpufreq/scaling_cur_freq 2>/dev/null)"; done
    for z in /sys/class/thermal/thermal_zone*; do echo "$z $(cat $z/temp 2>/dev/null)"; done
    free -m; echo "== iso"; ls -l "$ISO"; echo "== mode $MODE, $SECONDS_LIMIT s, state '${STATE}', pad '${PS2X_PAD_SCRIPT-default}', iop spin '${PS2X_IOP_SPIN_US:-2000}', prof '${PS2X_PROF:-0}', pin '${PS2X_PIN:-off}'"
} > "$LOG" 2>&1

# Clocks: like the RK3326 ports that run well (Super Mario Strikers, NFSU2), lock CPU, GPU and memory controller to
# "performance" for the run, so the measure is the best case a real port would get. The previous governors are put
# back on exit. PERF=0 keeps whatever the system has.
GOV_SAVE=""
set_perf() {
    for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor /sys/class/devfreq/*/governor; do
        [ -f "$g" ] || continue
        GOV_SAVE="$GOV_SAVE $g=$(cat "$g")"
        echo performance | sudo -n tee "$g" >/dev/null 2>&1 || echo performance > "$g" 2>/dev/null || true
    done
}
restore_gov() {
    for pair in $GOV_SAVE; do
        echo "${pair#*=}" | sudo -n tee "${pair%%=*}" >/dev/null 2>&1 || echo "${pair#*=}" > "${pair%%=*}" 2>/dev/null || true
    done
}
if [ "${PERF:-1}" = 1 ]; then
    set_perf
    trap restore_gov EXIT INT TERM
fi
{ echo "== governors during the run"; for g in /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor /sys/class/devfreq/*/governor; do [ -f "$g" ] && echo "$g $(cat "$g")"; done; } >> "$LOG" 2>&1

export PS2X_PAD_SCRIPT="${PS2X_PAD_SCRIPT-1300v:start:10,1500v:circle:10,1700v:circle:10}"
export GODHAND_HEADLESS=1 PS2X_BENCH=1 PS2X_GS_SCALE=1 PS2X_GS_GPU=0 GODHAND_TIME_LIMIT_SECONDS="$SECONDS_LIMIT"
export GODHAND_DUMP_ON_TIME_LIMIT=0 GODHAND_ISO_PREFETCH=0
[ "$MODE" = cpu ] && export PS2X_GS_NULL=1
if [ -n "$STATE" ]; then
    export PS2X_BENCH_LOAD_STATE="$STATE" PS2X_BENCH_LOAD_AT=${LOAD_AT:-15} PS2X_BENCH_WARMUP=$(( ${LOAD_AT:-15} + 10 ))
fi

# The binary links libEGL/libGLESv2/libgbm/libdrm even when it draws nothing: a missing one stops it before main.
# If only those are missing, the empty ones in stubs/ (scripts/make_gl_stubs.sh) stand in: the bench never calls them.
if command -v ldd >/dev/null 2>&1; then
    { echo "== ldd"; ldd "$BIN"; } >> "$LOG" 2>&1
    MISSING=$(ldd "$BIN" 2>&1 | awk '/not found/ {print $1}')
    if [ -n "$MISSING" ]; then
        OTHER=$(echo "$MISSING" | grep -vE '^(libEGL\.so\.1|libGLESv2\.so\.2|libgbm\.so\.1|libdrm\.so\.2)$' || true)
        if [ -z "$OTHER" ] && [ -d "$DIR/stubs" ]; then
            echo "graphics libraries missing ($(echo $MISSING)): using the empty ones in stubs/ (fine for the bench)" | tee -a "$LOG"
            export LD_LIBRARY_PATH="$DIR/stubs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        else
            echo "missing libraries (see $LOG):"; echo "$MISSING"; exit 1
        fi
    fi
fi

echo "measuring ($MODE, $SECONDS_LIMIT s)... log: $LOG"
echo "== binary $(basename "$BIN") ($(wc -c < "$BIN") bytes)" >> "$LOG"
"$BIN" --iso "$ISO" --headless --time-limit "$SECONDS_LIMIT" >> "$LOG" 2>&1
echo "exit code $?" >> "$LOG"
{ echo "== after"; for z in /sys/class/thermal/thermal_zone*; do echo "$z $(cat $z/temp 2>/dev/null)"; done; } >> "$LOG" 2>&1
grep -aE "^\[bench-summary\]|(^|[^A-Za-z_])ERROR([^A-Za-z_]|$)|exit code" "$LOG"
echo "done: $LOG"
