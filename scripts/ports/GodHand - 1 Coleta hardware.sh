#!/bin/bash
# God Hand R36S: coleta do hardware e do driver de vídeo (segundos). Gera ports/godhand-teste/hw.txt.
. "$(cd "$(dirname "$0")" && pwd)/godhand-teste/_comum.sh"
prepare_run_dir
msg "God Hand: coletando o hardware (hw.txt)..."
(cd "$RUN" && $SUDO sh ./collect_hw_info.sh > hw.txt 2>&1)
bring_back
msg "Pronto: ports/godhand-teste/hw.txt"
sleep 5
