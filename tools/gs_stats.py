#!/usr/bin/env python3
"""gs_stats.py <gs_replay --text output>: what one frame of the game asks the GS for.

Per frame (V lines): primitives (D), batches = runs of consecutive primitives with the same GS state (what a native
renderer would turn into one draw call each), distinct states, render targets (FRAME), textures (TEX0 base),
alpha-blended and sprite primitives, texture uploads (T) and their bytes (I). Prints medians and maxima.
"""
import re, statistics, sys
from collections import Counter

PRIM = {0: "point", 1: "line", 2: "linestrip", 3: "tri", 4: "tristrip", 5: "trifan", 6: "sprite"}

def frames(path):
    cur = None
    with open(path, "r", errors="replace") as f:
        for line in f:
            k = line[:1]
            if k == "V":
                if cur is not None:
                    yield cur
                cur = {"prims": 0, "batches": 0, "states": set(), "targets": set(), "tex": set(), "abe": 0,
                       "sprite": 0, "uploads": 0, "upload_bytes": 0, "ptype": Counter(), "last": None}
            elif cur is None:
                continue
            elif k == "D":
                st = line[2:].split(" | ", 1)[0].rstrip("\n")  # the GS state; vertices follow after " | "
                cur["prims"] += 1
                if st != cur["last"]:
                    cur["batches"] += 1
                    cur["last"] = st
                cur["states"].add(st)
                m = re.search(r"prim=(\d+)", st); p = int(m.group(1)) if m else -1
                cur["ptype"][PRIM.get(p, str(p))] += 1
                if p == 6: cur["sprite"] += 1
                if " abe=1" in st: cur["abe"] += 1
                m = re.search(r"frame=([0-9a-f]+)", st)
                if m: cur["targets"].add(m.group(1))
                if " tme=1" in st:
                    m = re.search(r"tex0=([0-9a-f]+)", st)
                    if m: cur["tex"].add(m.group(1))
            elif k == "T":
                cur["uploads"] += 1
            elif k == "I":
                parts = line.split()
                if len(parts) > 1 and parts[1].isdigit():
                    cur["upload_bytes"] += int(parts[1])
    if cur is not None:
        yield cur

rows = list(frames(sys.argv[1]))
def med(key, f=lambda v: v): return statistics.median(f(r[key]) for r in rows)
def mx(key, f=lambda v: v): return max(f(r[key]) for r in rows)
print(f"frames: {len(rows)} (each V line; the game draws 30 per second)")
for label, key, f in [("primitives", "prims", int), ("batches (same-state runs)", "batches", int),
                      ("distinct states", "states", len), ("render targets", "targets", len),
                      ("textures used", "tex", len), ("alpha-blended prims", "abe", int), ("sprites", "sprite", int),
                      ("texture uploads", "uploads", int), ("upload KB", "upload_bytes", lambda v: v // 1024)]:
    print(f"  {label:28s} median {med(key, f):>8}   max {mx(key, f):>8}")
tot = Counter()
for r in rows: tot.update(r["ptype"])
print("  primitive types:", ", ".join(f"{k} {v * 100 // sum(tot.values())}%" for k, v in tot.most_common()))
