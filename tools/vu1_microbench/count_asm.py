#!/usr/bin/env python3
"""Counts the instructions of each function in two assembly files (x86-64 and aarch64 from asm_probe.cpp or
vu1_microbench.cpp) and prints them side by side. Usage: count_asm.py x86.s arm.s [--show name]"""
import re
import sys


def parse(path):
    funcs, cur = {}, None
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\n")
        m = re.match(r"^([A-Za-z_][\w.$]*):", line)
        if m and not m.group(1).startswith((".L", "L", "$")):
            cur = m.group(1)
            funcs[cur] = []
            continue
        if cur is None:
            continue
        s = line.strip()
        if not s or s.startswith((".", "#", "//", "@", ";")) or s.endswith(":"):
            if s.startswith(".cfi_endproc") or s.startswith(".size"):
                cur = None if s.startswith(".size") else cur
            continue
        funcs[cur].append(s.split("//")[0].split("#")[0].strip())
    return funcs


def demangle_key(name):
    m = re.search(r"(p_\w+|t_\w+)", name)
    return m.group(1) if m else name


def main():
    x86, arm = parse(sys.argv[1]), parse(sys.argv[2])
    show = sys.argv[sys.argv.index("--show") + 1] if "--show" in sys.argv else None
    xk = {demangle_key(k): v for k, v in x86.items()}
    ak = {demangle_key(k): v for k, v in arm.items()}
    names = [n for n in ak if n in xk and (n.startswith("p_") or n.startswith("t_"))]
    print(f"{'função':28} {'x86':>5} {'arm':>5}  instruções ARM (sem ret)")
    for n in names:
        a = [i for i in ak[n] if not i.startswith("ret")]
        x = [i for i in xk[n] if not i.startswith("ret")]
        ops = " ; ".join(i.split()[0] for i in a) if len(a) <= 14 else f"({len(a)} instruções)"
        print(f"{n:28} {len(x):5} {len(a):5}  {ops}")
    if show:
        for label, d in (("x86", xk), ("arm", ak)):
            print(f"\n== {show} ({label})")
            for i in d.get(show, []):
                print("   ", i)


if __name__ == "__main__":
    main()
