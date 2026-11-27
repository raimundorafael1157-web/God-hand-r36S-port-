#!/bin/bash
# God Hand R36S: o teste A/B de 300 s (8 execuções) mais A5 e B5 de 900 s cada (térmico, sustentado), ~90 min.
# Use depois do teste 2, se os resultados dele forem estáveis.
. "$(cd "$(dirname "$0")" && pwd)/godhand-teste/_comum.sh"
ISO=$(find_iso)
[ -n "$ISO" ] || { msg "ISO não encontrada (coloque em roms/ps2/)."; sleep 10; exit 1; }
prepare_run_dir
msg "God Hand: teste A/B longo, ~90 min. Não desligue. ISO: $ISO"
(cd "$RUN" && $SUDO sh ./run_ab.sh "$ISO" --longo 2>&1 | tee "$TTY" > /dev/null)
bring_back
msg "Pronto: resultados em ports/godhand-teste/"
sleep 15
