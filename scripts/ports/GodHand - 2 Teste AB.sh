#!/bin/bash
# God Hand R36S: teste A/B (A = 0001-0009, B = 0001-0010), 8 execuções de 300 s, ~55 min.
# A tela mostra só o andamento: o jogo roda sem imagem. Deixe carregando, fora da capa, e não mexa.
. "$(cd "$(dirname "$0")" && pwd)/godhand-teste/_comum.sh"
ISO=$(find_iso)
[ -n "$ISO" ] || { msg "ISO não encontrada (coloque em roms/ps2/)."; sleep 10; exit 1; }
prepare_run_dir
msg "God Hand: teste A/B, ~55 min. Não desligue. ISO: $ISO"
(cd "$RUN" && $SUDO sh ./run_ab.sh "$ISO" 2>&1 | tee "$TTY" > /dev/null)
bring_back
msg "Pronto: resultados em ports/godhand-teste/ (ab-resumo-*.txt, ab-execucao-*.txt, bench-cpu-*.log)"
sleep 15
