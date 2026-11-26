#!/usr/bin/env python3
"""Compares two builds of godhand on the same fight, on a PC, reproducibly.

    tools/bench_compare.py --a <binary A> --b <binary B> --iso <iso> --state <stage1.state> [--runs 5] [--seconds 85]
                           [--env-a NAME=VALUE ...] [--env-b NAME=VALUE ...] [--out results.json]

What it measures, per run (runs alternate A, B, A, B... so a slow spell of the machine hits both):
  - speed: emulated fields per second / 59.94 (the game is paced to real time: on a PC this stays near 100% and only
    shows a problem when the CPU cannot keep up);
  - CPU cost per emulated field, per thread, in ms: thread CPU share x 10 ms / fields per second. This is the number
    that moves when an optimisation works, even while speed is pinned at 100% by the pacing;
  - fidelity: SHA-256 of the GS command stream recorded over fields 5100..5219 (PS2X_GS_RECORD; it holds VRAM hashes).
    The EE side is deterministic over this window: the same build gives the same bytes on every run, so a change that
    alters it changed behaviour. The fight must show "gs: IDENTICAL" for both builds.
  - audio: the SPU2 output (PS2X_AUDIO_DUMP) is NOT bit-reproducible run to run (the IOP thread runs against real
    time), so it is checked by numbers: bytes of audio per emulated field (48 kHz stereo s16 = ~3203; less means the
    IOP fell behind) and the RMS level of the last 8 MB.
Prints mean and standard deviation per build and the difference with a rough 95% interval. Needs only Python 3.
"""
import argparse
import array
import hashlib
import math
import json
import os
import re
import statistics
import subprocess
import sys
import tempfile

LINE = re.compile(r'^\[bench\] t=(\d+) fields/s=([\d.]+) speed=(\d+)%')
THREAD = re.compile(r'(\w+Thread)=(\d+)%')
RMS_BYTES = 8 << 20


def sha(path, limit=None):
    h = hashlib.sha256()
    if not os.path.exists(path):
        return None
    with open(path, 'rb') as f:
        left = limit
        while True:
            chunk = f.read(1 << 20 if left is None else min(1 << 20, left))
            if not chunk:
                break
            h.update(chunk)
            if left is not None:
                left -= len(chunk)
                if left <= 0:
                    break
    return h.hexdigest()[:16]


def audio_rms(path):
    if not os.path.exists(path):
        return 0.0
    size = os.path.getsize(path)
    with open(path, 'rb') as f:
        f.seek(max(0, size - RMS_BYTES) & ~3)
        samples = array.array('h', f.read())
    if not samples:
        return 0.0
    return math.sqrt(sum(x * x for x in samples) / len(samples))


def run_once(binary, args, extra_env, tag, workdir):
    gsr = os.path.join(workdir, f'{tag}.gsr')
    pcm = os.path.join(workdir, f'{tag}.pcm')
    env = dict(os.environ)
    env.update({
        'PS2X_PAD_SCRIPT': '', 'GODHAND_HEADLESS': '1', 'PS2X_BENCH': '1', 'PS2X_GS_SCALE': '1', 'PS2X_GS_GPU': '0',
        'GODHAND_DUMP_ON_TIME_LIMIT': '0', 'GODHAND_ISO_PREFETCH': '0', 'PS2X_GS_NULL': '1',
        'PS2X_BENCH_LOAD_STATE': args.state, 'PS2X_BENCH_LOAD_AT': '10', 'PS2X_BENCH_WARMUP': str(args.warmup),
        'PS2X_GS_RECORD': f'5100:120:{gsr}', 'PS2X_AUDIO_DUMP': pcm,
    })
    env.update(extra_env)
    proc = subprocess.run([binary, '--iso', args.iso, '--headless', '--time-limit', str(args.seconds)],
                          cwd=args.cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                          errors='replace', timeout=args.seconds + 120)
    speeds, cost = [], {}
    total_fields = 0.0
    for line in proc.stdout.splitlines():
        m = LINE.match(line)
        if m and '(state load' not in line:
            total_fields += float(m.group(2))
        if not m or int(m.group(1)) < args.warmup or '(state load' in line:
            continue
        fields = float(m.group(2))
        if fields <= 1.0:
            continue
        speeds.append(int(m.group(3)))
        for name, share in THREAD.findall(line):
            cost.setdefault(name, []).append(int(share) * 10.0 / fields)  # ms of CPU per emulated field
    result = {
        'speed': statistics.mean(speeds) if speeds else 0.0,
        'cost': {k: statistics.mean(v) for k, v in cost.items()},
        'gs': sha(gsr),
        'audio_per_field': (os.path.getsize(pcm) / total_fields) if os.path.exists(pcm) and total_fields else 0.0,
        'audio_rms': audio_rms(pcm),
    }
    for path in (gsr, pcm):
        if os.path.exists(path):
            os.remove(path)
    return result


def summary(values):
    if not values:
        return 0.0, 0.0
    return statistics.mean(values), (statistics.stdev(values) if len(values) > 1 else 0.0)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--a', required=True)
    p.add_argument('--b', required=True)
    p.add_argument('--iso', required=True)
    p.add_argument('--state', required=True)
    p.add_argument('--cwd', default='.', help='folder the game runs in (godhand.ini)')
    p.add_argument('--runs', type=int, default=5)
    p.add_argument('--seconds', type=int, default=85)
    p.add_argument('--warmup', type=int, default=25)
    p.add_argument('--env-a', nargs='*', default=[])
    p.add_argument('--env-b', nargs='*', default=[])
    p.add_argument('--out')
    args = p.parse_args()
    env_a = dict(e.split('=', 1) for e in args.env_a)
    env_b = dict(e.split('=', 1) for e in args.env_b)
    results = {'A': [], 'B': []}
    with tempfile.TemporaryDirectory(prefix='bench-compare-') as work:
        for i in range(args.runs):
            for tag, binary, env in (('A', args.a, env_a), ('B', args.b, env_b)):
                r = run_once(binary, args, env, f'{tag}{i}', work)
                results[tag].append(r)
                print(f'run {i + 1}/{args.runs} {tag}: speed={r["speed"]:.1f}% '
                      + ' '.join(f'{k}={v:.2f}ms' for k, v in sorted(r['cost'].items()))
                      + f' gs={r["gs"]} audio={r["audio_per_field"]:.0f}B/field rms={r["audio_rms"]:.0f}', flush=True)
    print()
    threads = sorted({k for side in results.values() for r in side for k in r['cost']})
    for key in ['speed'] + threads + ['audio_per_field', 'audio_rms']:
        if key in ('speed', 'audio_per_field', 'audio_rms'):
            get = lambda r, k=key: r[k]
        else:
            get = lambda r, k=key: r['cost'].get(k, 0.0)
        ma, sa = summary([get(r) for r in results['A']])
        mb, sb = summary([get(r) for r in results['B']])
        n = args.runs
        half = 1.96 * ((sa * sa + sb * sb) / n) ** 0.5 if n > 1 else 0.0
        unit = {'speed': '%', 'audio_per_field': ' B/field', 'audio_rms': ''}.get(key, ' ms/field')
        rel = f' ({100.0 * (mb - ma) / ma:+.1f}%)' if ma else ''
        print(f'{key:12s} A={ma:7.2f}{unit} ±{sa:.2f}  B={mb:7.2f}{unit} ±{sb:.2f}  B-A={mb - ma:+.2f} ±{half:.2f}{rel}')
    hashes = {r['gs'] for side in results.values() for r in side}
    print(f'gs: {"IDENTICAL in every run" if len(hashes) == 1 else "DIFFERENT: " + ", ".join(sorted(map(str, hashes)))}')
    if args.out:
        with open(args.out, 'w') as f:
            json.dump({'args': vars(args), 'results': results}, f, indent=1)
    return 0 if len(hashes) == 1 else 1


if __name__ == '__main__':
    sys.exit(main())
