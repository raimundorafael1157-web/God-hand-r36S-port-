#!/bin/bash
# God Hand R36S: rodada de medição (binário M de medição x binário B do pacote), ~45 minutos. Não altera A nem B.
. "$(cd "$(dirname "$0")" && pwd)/godhand-teste/_comum.sh"
# godhand-M pode vir em pedaços compactados (godhand-M.gz.part-aa, -ab...), para cartões/adaptadores que caem ao
# copiar arquivos grandes: junta, descompacta e confere o SHA-256 antes de usar.
# O mesmo vale para godhand-B e stage1.state (godhand-B.gz.part-*, stage1.state.gz.part-*).
ok_sum() { (cd "$PKG" && grep -E "  $1\$" "$2" 2>/dev/null | sha256sum -c - >/dev/null 2>&1); }
assemble() { # nome, arquivo de SHA-256
    ls "$PKG"/"$1".gz.part-* >/dev/null 2>&1 || return 0
    ok_sum "$1" "$2" && return 0
    msg "Juntando os pedaços de $1..."
    cat "$PKG"/"$1".gz.part-* | gzip -dc > "$PKG/$1.tmp" && mv "$PKG/$1.tmp" "$PKG/$1"
    if ok_sum "$1" "$2"; then msg "$1 montado e conferido."
    else rm -f "$PKG/$1"; msg "ERRO: $1 montado não confere. Copie os pedaços de $1 de novo."; sleep 15; exit 1; fi
}
assemble godhand-M SHA256SUMS-M
assemble godhand-B SHA256SUMS
assemble stage1.state SHA256SUMS
for f in godhand-M godhand-B stage1.state SHA256SUMS SHA256SUMS-M; do
    [ -f "$PKG/$f" ] || { msg "Falta $f em ports/godhand-teste/."; sleep 10; exit 1; }
done
prepare_run_dir
chmod +x "$RUN/godhand-M" 2>/dev/null
[ "$RUN" = "$PKG" ] || cp "$PKG/godhand-M" "$PKG/SHA256SUMS-M" "$PKG/run_meas.sh" "$RUN/" 2>/dev/null
ISO=$(find_iso)
[ -n "$ISO" ] || { msg "ISO do God Hand não encontrada (coloque em roms/ps2/)."; sleep 10; exit 1; }
msg "God Hand: rodada de medição, ~45 min. Deixe carregando, fora da capa, sem mexer."
(cd "$RUN" && $SUDO sh ./run_meas.sh "$ISO" 2>&1 | tee "$TTY" > /dev/null)
[ "$RUN" = "$PKG" ] || cp "$RUN"/meas-*.txt "$RUN"/bench-cpu-*.log "$PKG/" 2>/dev/null
msg "Pronto: ports/godhand-teste/meas-*.txt e bench-cpu-B*/M*.log"
sleep 10
