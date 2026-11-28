#!/bin/bash
# God Hand R36S: rodada de medição (binário M de medição x binário B do pacote), ~45 minutos. Não altera A nem B.
. "$(cd "$(dirname "$0")" && pwd)/godhand-teste/_comum.sh"
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
