#!/bin/sh
# make_gl_stubs.sh <aarch64 binary> <out dir>: empty libEGL / libGLESv2 / libgbm / libdrm with every function the binary
# imports from them. The bench (no window) never calls them, but a missing library stops the binary before main;
# run_bench.sh puts these in front only when ldd reports one missing. Never for a run that draws.
set -eu
BIN=$1; OUT=$2
mkdir -p "$OUT"
TMP=$(mktemp -d)
READELF=$(command -v aarch64-linux-gnu-readelf || echo "python3 -m ziglang readelf")
$READELF --dyn-syms -W "$BIN" | awk '$4=="FUNC" && $7=="UND" {sub(/@.*/, "", $8); print $8}' | sort -u > "$TMP/syms"
make_lib() { # <soname> <regex>
    grep -E "$2" "$TMP/syms" | awk '{printf "int %s(void) { return 0; }\n", $1}' > "$TMP/$1.c"
    [ -s "$TMP/$1.c" ] || return 0
    python3 -m ziglang cc -target aarch64-linux-gnu.2.17 -shared -fPIC -Wl,-soname,"$1" -o "$OUT/$1" "$TMP/$1.c"
    echo "$1: $(wc -l < "$TMP/$1.c") functions"
}
make_lib libEGL.so.1 '^egl'
make_lib libGLESv2.so.2 '^gl[A-Z]'
make_lib libgbm.so.1 '^gbm_'
make_lib libdrm.so.2 '^drm'
rm -rf "$TMP"
