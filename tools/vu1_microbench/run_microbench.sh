#!/bin/sh
# Runs the VU1 microbenchmarks (vu1_microbench) on the device and writes mb-<date>.log + one CSV per pass.
# Measurement only: it does not run the game, A or B. ~3 minutes.
#   sh run_microbench.sh            (from the package folder; root, or sudo without password, for perf counters)
DIR=$(cd "$(dirname "$0")" && pwd)
STAMP=$(date +%Y%m%d-%H%M%S)
LOG="$DIR/mb-$STAMP.log"
BIN="$DIR/vu1_microbench"
chmod +x "$BIN" 2>/dev/null
hottest() {
    t=0
    for z in /sys/class/thermal/thermal_zone*/temp; do v=$(cat "$z" 2>/dev/null || echo 0); [ "$v" -gt "$t" ] && t=$v; done
    echo $((t / 1000))
}
{
    echo "== vu1_microbench $STAMP"
    sha256sum "$BIN" 2>/dev/null
    uname -a; cat /proc/device-tree/model 2>/dev/null | tr -d '\0'; echo
    grep -m4 -E "model name|CPU part|CPU revision|Features" /proc/cpuinfo
    echo "perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null) uid=$(id -u)"
    for c in /sys/devices/system/cpu/cpu[0-9]*; do echo "$c $(cat $c/cpufreq/scaling_governor 2>/dev/null) $(cat $c/cpufreq/scaling_cur_freq 2>/dev/null) max=$(cat $c/cpufreq/scaling_max_freq 2>/dev/null)"; done
    echo "loadavg: $(cat /proc/loadavg)"
    echo "temperature before: $(hottest) C"
} > "$LOG" 2>&1

# Governors to "performance" during the run (as run_bench.sh), put back at the end.
SAVED=""
for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    [ -f "$g" ] || continue
    SAVED="$SAVED $g=$(cat "$g")"
    echo performance | sudo -n tee "$g" >/dev/null 2>&1 || echo performance > "$g" 2>/dev/null || true
done
sleep 2
echo "governors during the run: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null) $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null) kHz" >> "$LOG"

# Pass 1 on core 3 (the least busy with interrupts), pass 2 on core 2: two samples on separate cores.
for CORE in 3 2; do
    echo "== pass on cpu $CORE, temperature $(hottest) C" >> "$LOG"
    "$BIN" --cpu "$CORE" --reps 9 --ms 300 --csv "$DIR/mb-$STAMP-cpu$CORE.csv" >> "$LOG" 2>&1
    echo "exit code $?" >> "$LOG"
done
echo "temperature after: $(hottest) C" >> "$LOG"

for pair in $SAVED; do
    echo "${pair#*=}" | sudo -n tee "${pair%%=*}" >/dev/null 2>&1 || echo "${pair#*=}" > "${pair%%=*}" 2>/dev/null || true
done
grep -E "^\[mb" "$LOG"
echo "done: $LOG"
