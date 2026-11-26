#!/usr/bin/env python3
"""gs_targets.py <gs_replay --text output>: per render target (FRAME base) primitives and batches per frame, and how
many texture uploads repeat content already uploaded in an earlier frame (what a texture cache would skip)."""
import hashlib, statistics, sys
from collections import defaultdict

per_target = defaultdict(lambda: defaultdict(lambda: [0, 0]))  # target -> frame -> [prims, batches]
seen = set(); frame = -1; last_state = None; uploads = []; cur_up = None; pending_t = None
with open(sys.argv[1], errors="replace") as f:
    for line in f:
        k = line[:1]
        if k == "V":
            frame += 1; last_state = None
            uploads.append([0, 0, 0, 0])  # count, bytes, repeated count, repeated bytes
        elif frame < 0:
            continue
        elif k == "D":
            st = line[2:].split(" | ", 1)[0]
            tgt = st.split("frame=", 1)[1].split("/", 1)[0] if "frame=" in st else "?"
            pt = per_target[tgt][frame]
            pt[0] += 1
            if st != last_state:
                pt[1] += 1; last_state = st
        elif k == "T":
            pending_t = line[2:].strip()
        elif k == "I":
            parts = line.split(None, 2)
            n = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 0
            if n == 0: continue
            data = parts[2].strip() if len(parts) > 2 else ""
            h = hashlib.blake2b((pending_t or "").split(" dst=")[1].encode() if pending_t and " dst=" in pending_t else b"", digest_size=8)
            h.update(data.encode())
            key = h.digest()
            u = uploads[frame]; u[0] += 1; u[1] += n
            if key in seen: u[2] += 1; u[3] += n
            seen.add(key)

frames = frame + 1
print(f"frames: {frames}")
print(f"{'target (FBP)':>12} {'prims/frame':>12} {'batches/frame':>14}")
for tgt, fr in sorted(per_target.items(), key=lambda kv: -sum(v[0] for v in kv[1].values())):
    prims = statistics.median([fr.get(i, [0, 0])[0] for i in range(frames)])
    batches = statistics.median([fr.get(i, [0, 0])[1] for i in range(frames)])
    print(f"{tgt:>12} {prims:>12} {batches:>14}")
late = uploads[5:]  # after the first frames, what is not new
print("uploads/frame (after frame 5): median %d, repeats of earlier content: median %d of them (%d of %d KB)" % (
    statistics.median(u[0] for u in late), statistics.median(u[2] for u in late),
    statistics.median(u[3] for u in late) // 1024, statistics.median(u[1] for u in late) // 1024))
