#!/bin/sh
# Measures the GLES 3 renderer on the R36S GPU (Mali-G31) by replaying a recorded fight: the GS commands of 120 game
# frames, drawn as fast as possible, nothing on screen (EGL without a window, so EmulationStation can stay up, but
# closing it frees memory and the GPU). No game code runs: this is the cost of drawing only.
#   sh run_gles.sh [gravação.gsr] [repetições]
# Writes gles-<date>.log next to this script, with one PPM of the picture to check it. Send both back.
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
GSR=${1:-$DIR/fight.gsr}
REPEAT=${2:-3}
LOG="$DIR/gles-$(date +%Y%m%d-%H%M%S).log"
{
    echo "== device"; uname -a; cat /proc/device-tree/model 2>/dev/null; echo
    for d in /sys/class/devfreq/*; do echo "$d $(cat $d/governor 2>/dev/null) $(cat $d/cur_freq 2>/dev/null)"; done
    free -m; ls -l /dev/dri 2>&1; ls -l "$GSR"
} > "$LOG" 2>&1

# Same clock policy as run_bench.sh: performance governors during the run, put back on exit (PERF=0 skips it).
GOV_SAVE=""
if [ "${PERF:-1}" = 1 ]; then
    for g in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor /sys/class/devfreq/*/governor; do
        [ -w "$g" ] || continue
        GOV_SAVE="$GOV_SAVE $g=$(cat "$g")"
        echo performance > "$g" 2>/dev/null
    done
fi
restore() { for p in $GOV_SAVE; do echo "${p#*=}" > "${p%%=*}" 2>/dev/null; done; }
trap restore EXIT INT TERM

# Firmwares whose Mali driver is the fbdev build have no libgbm: gles_replay then takes the empty one in
# stubs-gles/ (gbm_create_device returns nothing, and the context falls back to the default EGL display). EGL and
# GLES are never stubbed: they must be the device's own.
if command -v ldd >/dev/null 2>&1 && ldd "$DIR/gles_replay" 2>&1 | grep -q "libgbm.so.1 => not found"; then
    echo "libgbm missing: using stubs-gles/libgbm.so.1 (fbdev Mali driver)" | tee -a "$LOG"
    export LD_LIBRARY_PATH="$DIR/stubs-gles${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}" "$DIR/gles_replay" "$GSR" --repeat "$REPEAT" --dump "$DIR" --dump-every 60 >> "$LOG" 2>&1
echo "exit code $?" >> "$LOG"
grep -aE "gles\]|gles-summary" "$LOG"
echo "log: $LOG"
