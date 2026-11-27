#!/bin/bash
# Partes comuns dos atalhos "GodHand - *.sh" do menu Ports (ArkOS / dArkOS). Não é um atalho: os outros o carregam.
# Pasta do pacote: ports/godhand-teste/ (ao lado deste arquivo). Resultados ficam nela.
PKG="$(cd "$(dirname "$0")" && pwd)/godhand-teste"
TTY=/dev/tty1
sudo -n chmod 666 "$TTY" 2>/dev/null
[ -w "$TTY" ] || TTY=/dev/null
printf '\033c' > "$TTY"
msg() { echo "$*" | tee -a "$PKG/atalhos.log" > "$TTY"; }
SUDO=""
if command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then SUDO="sudo -E"; fi
[ -d "$PKG" ] || { msg "Pasta $PKG não encontrada. Copie o pacote para ports/godhand-teste/."; sleep 10; exit 1; }

# A ISO: primeiro "God Hand" nas pastas de PS2 dos dois cartões, depois qualquer .iso, depois a pasta do pacote.
find_iso() {
    for d in /roms/ps2 /roms2/ps2 /storage/roms/ps2 "$PKG"; do
        f=$(ls "$d"/*[Gg]od*[Hh]and*.iso 2>/dev/null | head -1); [ -n "$f" ] && { echo "$f"; return; }
    done
    for d in /roms/ps2 /roms2/ps2 /storage/roms/ps2 "$PKG"; do
        f=$(ls "$d"/*.iso 2>/dev/null | head -1); [ -n "$f" ] && { echo "$f"; return; }
    done
}

# Onde rodar: na própria pasta, se o cartão deixa executar binários; senão numa cópia na memória interna (sem a
# gravação de 640 MB), com os resultados trazidos de volta no fim (bring_back).
RUN="$PKG"
prepare_run_dir() {
    chmod +x "$PKG"/godhand-A "$PKG"/godhand-B "$PKG"/gles_replay 2>/dev/null
    if "$PKG/gles_replay" >/dev/null 2>&1; [ $? -ne 126 ] && [ -x "$PKG/godhand-A" ]; then RUN="$PKG"; return; fi
    RUN="$HOME/godhand-teste-run"
    msg "O cartão não permite executar daqui: copiando o pacote para $RUN (sem fight.gsr)..."
    mkdir -p "$RUN"
    for f in "$PKG"/*; do [ "$(basename "$f")" = fight.gsr ] || cp -r "$f" "$RUN/"; done
    chmod +x "$RUN"/godhand-A "$RUN"/godhand-B "$RUN"/gles_replay "$RUN"/*.sh 2>/dev/null
    [ -f "$PKG/fight.gsr" ] && ln -sf "$PKG/fight.gsr" "$RUN/fight.gsr"
}
bring_back() {
    [ "$RUN" = "$PKG" ] && return
    cp "$RUN"/*.log "$RUN"/*.txt "$RUN"/*.ppm "$PKG/" 2>/dev/null
}
