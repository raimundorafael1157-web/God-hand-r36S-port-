#!/bin/bash
# God Hand R36S: rodada de medição (binário M de medição x binário B do pacote), ~45 minutos. Não altera A nem B.
. "$(cd "$(dirname "$0")" && pwd)/godhand-teste/_comum.sh"
# godhand-M pode vir em pedaços compactados (godhand-M.gz.part-aa, -ab...), para cartões/adaptadores que caem ao
# copiar arquivos grandes: junta, descompacta e confere o SHA-256 antes de usar.
if ls "$PKG"/godhand-M.gz.part-* >/dev/null 2>&1; then
    if [ ! -f "$PKG/godhand-M" ] || ! (cd "$PKG" && sha256sum -c SHA256SUMS-M >/dev/null 2>&1); then
        msg "Juntando os pedaços do godhand-M..."
        cat "$PKG"/godhand-M.gz.part-* | gzip -dc > "$PKG/godhand-M.tmp" && mv "$PKG/godhand-M.tmp" "$PKG/godhand-M"
        if (cd "$PKG" && sha256sum -c SHA256SUMS-M >/dev/null 2>&1); then
            msg "godhand-M montado e conferido."
        else
            rm -f "$PKG/godhand-M"
            msg "ERRO: godhand-M montado não confere com SHA256SUMS-M. Copie os pedaços de novo."; sleep 15; exit 1
        fi
    fi
fi
[ -f "$PKG/godhand-M" ] || { msg "godhand-M não está em ports/godhand-teste/."; sleep 10; exit 1; }
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
