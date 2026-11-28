#!/bin/bash
# God Hand R36S: microbenchmarks isolados do VU1 (não roda o jogo, nem o A nem o B). ~3 minutos.
PKG="$(cd "$(dirname "$0")" && pwd)/godhand-teste"
TTY=/dev/tty1
sudo -n chmod 666 "$TTY" 2>/dev/null
[ -w "$TTY" ] || TTY=/dev/null
printf '\033c' > "$TTY"
msg() { echo "$*" | tee -a "$PKG/atalhos.log" > "$TTY"; }
[ -f "$PKG/vu1_microbench" ] || { msg "vu1_microbench não está em ports/godhand-teste/."; sleep 10; exit 1; }
SUDO=""
if command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then SUDO="sudo -E"; fi

# Onde rodar: na própria pasta, se o cartão deixa executar; senão numa cópia na memória interna.
RUN="$PKG"
chmod +x "$PKG/vu1_microbench" 2>/dev/null
"$PKG/vu1_microbench" --help >/dev/null 2>&1
if [ $? -eq 126 ]; then
    RUN="$HOME/godhand-mb-run"
    mkdir -p "$RUN" && cp "$PKG/vu1_microbench" "$PKG/run_microbench.sh" "$RUN/" && chmod +x "$RUN/vu1_microbench"
fi
msg "God Hand: microbenchmarks do VU1 (~3 min). Não mexa até aparecer Pronto."
(cd "$RUN" && $SUDO sh ./run_microbench.sh 2>&1 | tee "$TTY" > /dev/null)
[ "$RUN" = "$PKG" ] || cp "$RUN"/mb-*.log "$RUN"/mb-*.csv "$PKG/" 2>/dev/null
msg "Pronto: ports/godhand-teste/mb-*.log e mb-*.csv"
sleep 10
