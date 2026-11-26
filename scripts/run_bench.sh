#!/bin/sh
# Runs the measurement on the R36S. Copy this folder (godhand + run_bench.sh) to the SD card, then from a terminal:
#   sh run_bench.sh /caminho/para/God\ Hand\ (USA).iso [modo] [segundos] [save-state]
# modo: cpu (default) = nothing is drawn, measures EE + VU1 + IOP + GS front end only (PS2X_GS_NULL=1)
#       swgs          = the runtime's software GS draws every frame (its cost on top, still no screen)
# Writes bench-<modo>-<date>.log next to this script. Send that file back. Nothing here changes the system.
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
ISO=${1:?uso: sh run_bench.sh <iso> [cpu|swgs] [segundos] [save-state]}
MODE=${2:-cpu}
SECONDS_LIMIT=${3:-300}
STATE=${4:-}
LOG="$DIR/bench-$MODE-$(date +%Y%m%d-%H%M%S).log"

{
    echo "== device"; uname -a; cat /proc/device-tree/model 2>/dev/null; echo
    for c in /sys/devices/system/cpu/cpu[0-9]*; do echo "$c $(cat $c/cpufreq/scaling_governor 2>/dev/null) $(cat $c/cpufreq/scaling_cur_freq 2>/dev/null)"; done
    for z in /sys/class/thermal/thermal_zone*; do echo "$z $(cat $z/temp 2>/dev/null)"; done
    free -m; echo "== iso"; ls -l "$ISO"; echo "== mode $MODE, $SECONDS_LIMIT s, state '${STATE}'"
} > "$LOG" 2>&1

export GODHAND_HEADLESS=1 PS2X_BENCH=1 PS2X_GS_SCALE=1 PS2X_GS_GPU=0 GODHAND_TIME_LIMIT_SECONDS="$SECONDS_LIMIT"
export GODHAND_DUMP_ON_TIME_LIMIT=0 GODHAND_ISO_PREFETCH=0
[ "$MODE" = cpu ] && export PS2X_GS_NULL=1
if [ -n "$STATE" ]; then
    export PS2X_BENCH_LOAD_STATE="$STATE" PS2X_BENCH_LOAD_AT=${LOAD_AT:-15} PS2X_BENCH_WARMUP=$(( ${LOAD_AT:-15} + 10 ))
fi

echo "measuring ($MODE, $SECONDS_LIMIT s)... log: $LOG"
"$DIR/godhand" --iso "$ISO" --headless --time-limit "$SECONDS_LIMIT" >> "$LOG" 2>&1
echo "exit code $?" >> "$LOG"
{ echo "== after"; for z in /sys/class/thermal/thermal_zone*; do echo "$z $(cat $z/temp 2>/dev/null)"; done; } >> "$LOG" 2>&1
grep -E "^\[bench-summary\]|ERROR|exit code" "$LOG"
echo "done: $LOG"
